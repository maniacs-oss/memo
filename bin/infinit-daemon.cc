#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#ifdef INFINIT_MACOSX
#  include <sys/param.h>
#  include <sys/mount.h>
#endif

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include <elle/log.hh>
#include <elle/system/PIDFile.hh>
#include <elle/system/Process.hh>
#include <elle/serialization/json.hh>
#include <elle/system/self-path.hh>

#include <reactor/network/http-server.hh>
#include <reactor/network/unix-domain-server.hh>
#include <reactor/network/unix-domain-socket.hh>

#include <infinit/utility.hh>

ELLE_LOG_COMPONENT("infinit-daemon");

#include <main.hh>

infinit::Infinit ifnt;

struct Mount
{
  std::unique_ptr<elle::system::Process> process;
  infinit::MountOptions options;
};

class MountManager
{
public:
  void
  start(std::string const& name, infinit::MountOptions opts = {},
        bool force_mount = false,
        bool wait_for_mount = false);
  void
  stop(std::string const& name);
  void
  status(std::string const& name, elle::serialization::SerializerOut& reply);
  bool
  exists(std::string const& name);
  std::string
  mountpoint(std::string const& name);
  std::vector<std::string>
  list();
  ELLE_ATTRIBUTE_RW(boost::optional<std::string>, log_level);
  ELLE_ATTRIBUTE_RW(boost::optional<std::string>, log_path);
private:
  std::unordered_map<std::string, Mount> _mounts;
};

std::vector<std::string>
MountManager::list()
{
  std::vector<std::string> res;
  for (auto const& volume: ifnt.volumes_get())
    res.push_back(volume.name);
  return res;
}

std::string
MountManager::mountpoint(std::string const& name)
{
  auto it = _mounts.find(name);
  if (it == _mounts.end())
    throw elle::Exception("not mounted: " + name);
  ELLE_ASSERT(it->second.options.mountpoint);
  return it->second.options.mountpoint.get();
}

bool
MountManager::exists(std::string const& name_)
{
  std::string name(name_);
  if (name.find("/") == name.npos)
    name = elle::system::username() + "/" + name;
  try
  {
    auto volume = ifnt.volume_get(name);
    return true;
  }
  catch (elle::Error const& e)
  {
    return false;
  }
}

bool
is_mounted(std::string const& path)
{
#ifdef INFINIT_LINUX
  auto mounts = boost::filesystem::path("/proc") / std::to_string(getpid()) / "mounts";
  boost::filesystem::ifstream ifs(mounts);
  while (!ifs.eof())
  {
    std::string line;
    std::getline(ifs, line);
    std::vector<std::string> elems;
    boost::algorithm::split(elems, line, boost::is_any_of(" "));
    if (elems.size() >= 2 && elems.at(1) == path)
      return true;
  }
  return false;
#elif defined(INFINIT_WINDOWS)
  // We mount as drive letters under windows
  return boost::filesystem::exists(path);
#elif defined(INFINIT_MACOSX)
  struct statfs sfs;
  int res = statfs(path.c_str(), &sfs);
  if (res)
    return false;
  return boost::filesystem::path(path) == boost::filesystem::path(sfs.f_mntonname);
#else
  throw elle::Error("is_mounted is not implemented");
#endif
}

void
MountManager::start(std::string const& name, infinit::MountOptions opts,
                    bool force_mount,
                    bool wait_for_mount)
{
  auto volume = ifnt.volume_get(name);
  volume.mount_options.merge(opts);
  Mount m{nullptr, volume.mount_options};
  if (force_mount && !m.options.mountpoint)
    m.options.mountpoint =
    (boost::filesystem::temp_directory_path() / boost::filesystem::unique_path()).string();
  std::vector<std::string> arguments;
  static const auto root = elle::system::self_path().parent_path();
  arguments.push_back((root / "infinit-volume").string());
  arguments.push_back("--run");
  arguments.push_back(volume.name);
  std::unordered_map<std::string, std::string> env;
  m.options.to_commandline(arguments, env);
  if (this->_log_level)
    env.insert(std::make_pair("ELLE_LOG_LEVEL", _log_level.get()));
  if (this->_log_path)
    env.insert(
      std::make_pair("ELLE_LOG_FILE",
                     _log_path.get() + "/infinit-volume-" + name
                     + '-' + boost::posix_time::to_iso_extended_string(
                       boost::posix_time::microsec_clock::universal_time())
                     + ".log"));
  ELLE_TRACE("Spawning with %s %s", arguments, env);
  // FIXME upgrade Process to accept env
  for (auto const& e: env)
    elle::os::setenv(e.first, e.second, true);
  m.process = elle::make_unique<elle::system::Process>(arguments);
  int pid = m.process->pid();
  std::thread t([pid] {
      int status = 0;
      ::waitpid(pid, &status, 0);
  });
  t.detach();
  auto mountpoint = m.options.mountpoint;
  this->_mounts.emplace(name, std::move(m));
  if (wait_for_mount && mountpoint)
  {
    for (int i=0; i<100; ++i)
    {
      if (kill(pid, 0))
      {
        ELLE_TRACE("Process is dead: %s", strerror(errno));
        break;
      }
      if (is_mounted(mountpoint.get()))
        return;
      reactor::sleep(100_ms);
    }
    ELLE_ERR("mount of %s failed", name);
    stop(name);
    throw elle::Error("Mount failure for " + name);
  }
}

void
MountManager::stop(std::string const& name)
{
  auto it = _mounts.find(name);
  if (it == _mounts.end())
    throw elle::Error("not mounted: " + name);
  ::kill(it->second.process->pid(), SIGTERM); // FIXME: try harder
  this->_mounts.erase(it);
}

void
MountManager::status(std::string const& name,
                     elle::serialization::SerializerOut& reply)
{
  auto it = this->_mounts.find(name);
  if (it == this->_mounts.end())
    throw elle::Exception("not mounted: " + name);
  bool live = ! kill(it->second.process->pid(), 0);
  reply.serialize("live", live);
  if (it->second.options.mountpoint)
    reply.serialize("mountpoint", it->second.options.mountpoint.get());
}

class DockerVolumePlugin
{
public:
  DockerVolumePlugin(MountManager& manager, bool tcp);
  ~DockerVolumePlugin();
  void install(bool tcp);
  void uninstall();
private:
  MountManager& _manager;
  std::unique_ptr<reactor::network::HttpServer> _server;
  std::unordered_map<std::string, int> _mount_count;
};

static
std::string
daemon_command(std::string const& s);

class PIDFile
  : public elle::PIDFile
{
public:
  PIDFile()
    : elle::PIDFile(this->path())
  {}

  static
  boost::filesystem::path
  path()
  {
    return infinit::xdg_runtime_dir () / "daemon.pid";
  }

  static
  int
  read()
  {
    return elle::PIDFile::read(PIDFile::path());
  }
};

static
std::string
daemon_command(std::string const& s);

static
int
daemon_running()
{
  int pid = -1;
  try
  {
    pid = PIDFile::read();
  }
  catch (elle::Error const& e)
  {
    ELLE_TRACE("error getting PID: %s", e);
    return 0;
  }
  if (kill(pid, 0) != 0)
    return 0;
  try
  {
    daemon_command("{\"operation\": \"status\"}");
    return pid;
  }
  catch (elle::Error const& e)
  {
    ELLE_TRACE("status command threw %s", e);
    return 0;
  }
}

static
void
daemon_stop()
{
  int pid = daemon_running();
  if (!pid)
    elle::err("daemon is not running");
  try
  {
    daemon_command("{\"operation\": \"stop\"}");
  }
  catch (elle::Error const& e)
  {
    ELLE_TRACE("stop command threw %s", e);
  }
  for (int i = 0; i<50; ++i)
  {
    if (kill(pid, 0))
    {
      std::cout << "daemon stopped" << std::endl;
      return;
    }
    usleep(100000);
  }
  ELLE_TRACE("Sending TERM to %s", pid);
  if (kill(pid, SIGTERM))
    ELLE_TRACE("kill failed");
  for (int i=0; i<50; ++i)
  {
    if (kill(pid, 0))
      return;
    usleep(100000);
  }
  ELLE_TRACE("Process still running, sending KILL");
  kill(pid, SIGKILL);
  for (int i=0; i<50; ++i)
  {
    if (kill(pid, 0))
      return;
    usleep(100000);
  }
}

static
void
daemonize()
{
  if (daemon(1, 0))
    elle::err("failed to daemonize: %s", strerror(errno));
}

static
std::string
daemon_command(std::string const& s)
{
  reactor::Scheduler sched;
  std::string reply;
  reactor::Thread main_thread(
    sched,
    "main",
    [&]
    {
      reactor::network::UnixDomainSocket sock(daemon_sock_path());
      std::string cmd = s + "\n";
      ELLE_TRACE("writing query: %s", s);
      sock.write(elle::ConstWeakBuffer(cmd.data(), cmd.size()));
      ELLE_TRACE("reading result");
      reply = sock.read_until("\n").string();
      ELLE_TRACE("ok: '%s'", reply);
    });
  sched.run();
  return reply;
}

static
std::string
process_command(elle::json::Object query, MountManager& manager)
{
  ELLE_TRACE("command: %s", elle::json::pretty_print(query));
  elle::serialization::json::SerializerIn command(query, false);
  std::stringstream ss;
  {
    elle::serialization::json::SerializerOut response(ss, false);
    auto op = command.deserialize<std::string>("operation");
    response.serialize("operation", op);
    try
    {
      if (op == "status")
      {
        response.serialize("status", "Ok");
      }
      else if (op == "stop")
      {
        throw elle::Exit(0);
      }
      else if (op == "volume-start")
      {
        auto volume = command.deserialize<std::string>("volume");
        auto opts =
          command.deserialize<boost::optional<infinit::MountOptions>>("options");
        manager.start(volume, opts ? opts.get() : infinit::MountOptions(),
                      false, true);
      }
      else if (op == "volume-stop")
      {
        auto volume = command.deserialize<std::string>("volume");
        manager.stop(volume);
      }
      else if (op == "volume-status")
      {
        auto volume = command.deserialize<std::string>("volume");
        manager.status(volume, response);
      }
      else
      {
        throw std::runtime_error(("unknown operation: " + op).c_str());
      }
      response.serialize("result", "Ok");
    }
    catch (elle::Exception const& e)
    {
      response.serialize("result", "Error");
      response.serialize("error", e.what());
    }
  }
  ss << '\n';
  return ss.str();
}

COMMAND(stop)
{
  daemon_stop();
}

COMMAND(status)
{
  if (daemon_running())
    std::cout << "Running" << std::endl;
  else
    std::cout << "Stopped" << std::endl;
}

COMMAND(start)
{
  if (daemon_running())
    elle::err("daemon already running");
  MountManager manager;
  DockerVolumePlugin dvp(manager, flag(args, "docker-socket-tcp"));
  if (!flag(args, "foreground"))
    daemonize();
  PIDFile pid;
  reactor::network::UnixDomainServer srv;
  auto sockaddr = daemon_sock_path();
  boost::filesystem::remove(sockaddr);
  srv.listen(sockaddr);
  auto loglevel = optional(args, "log-level");
  manager.log_level(loglevel);
  auto logpath = optional(args, "log-path");
  manager.log_path(logpath);
  elle::With<reactor::Scope>() << [&] (reactor::Scope& scope)
  {
    while (true)
    {
      auto socket = elle::utility::move_on_copy(srv.accept());
      auto name = elle::sprintf("%s server", **socket);
      scope.run_background(
        name,
        [socket,&manager]
        {
          try
          {
            while (true)
            {
              auto json =
                boost::any_cast<elle::json::Object>(elle::json::read(**socket));
              auto reply = process_command(json, manager);
              ELLE_TRACE("Writing reply: '%s'", reply);
              socket->write(reply);
            }
          }
          catch (elle::Error const& e)
          {
            ELLE_TRACE("%s", e);
            try
            {
              socket->write(std::string("{\"error\": \"") + e.what() + "\"}\n");
            }
            catch (elle::Error const&)
            {}
          }
        });
    }
  };
}

DockerVolumePlugin::DockerVolumePlugin(MountManager& manager, bool tcp)
: _manager(manager)
{
  install(tcp);
}
DockerVolumePlugin::~DockerVolumePlugin()
{
  uninstall();
}

void
DockerVolumePlugin::uninstall()
{
}

void
DockerVolumePlugin::install(bool tcp)
{
  // plugin path is either in /etc/docker/plugins or /usr/lib/docker/plugins
  auto dir = boost::filesystem::path("/usr") /"lib"/ "docker" / "plugins";
  boost::system::error_code erc;
  boost::filesystem::create_directories(dir, erc);
  if (tcp)
  {
    this->_server = elle::make_unique<reactor::network::HttpServer>();
    int port = _server->port();
    std::string url = "tcp://localhost:" + std::to_string(port);
    boost::filesystem::ofstream ofs(dir / "infinit.spec");
    if (!ofs.good())
    {
      ELLE_LOG("Execute the following command: echo %s |sudo tee %s/infinit.spec",
               url, dir.string());
    }
    ofs << url;
  }
  else
  {
    auto us = elle::make_unique<reactor::network::UnixDomainServer>();
    auto sock_path = boost::filesystem::path("/run") / "docker" / "plugins" / "infinit.sock";
    boost::filesystem::create_directories(sock_path.parent_path());
    boost::filesystem::remove(sock_path, erc);
    us->listen(sock_path);
    this->_server = elle::make_unique<reactor::network::HttpServer>(std::move(us));
  }
  {
    auto json = "\"name\": \"infinit\", \"address\": \"http://www.infinit.sh\"";
    boost::filesystem::ofstream ofs(dir / "infinit.json");
    if (!ofs.good())
    {
      ELLE_LOG("Execute the following command: echo '%s' |sudo tee %s/infinit.json",
               json, dir.string());
    }
    ofs << json;
  }
  #define ROUTE_SIG  (reactor::network::HttpServer::Headers const&,     \
                      reactor::network::HttpServer::Cookies const&,     \
                      reactor::network::HttpServer::Parameters const&,  \
                      elle::Buffer const& data) -> std::string
  _server->register_route("/Plugin.Activate",  reactor::http::Method::POST,
    [] ROUTE_SIG {
      ELLE_TRACE("Activating plugin");
      return "{\"Implements\": [\"VolumeDriver\"]}";
    });
  _server->register_route("/VolumeDriver.Create", reactor::http::Method::POST,
    [] ROUTE_SIG {
      return "{\"Err\": \"Use 'infinit-volume --create'\"}";
    });
  _server->register_route("/VolumeDriver.Remove", reactor::http::Method::POST,
    [] ROUTE_SIG {
      return "{\"Err\": \"Use 'infinit-volume --delete'\"}";
    });
  _server->register_route("/VolumeDriver.Get", reactor::http::Method::POST,
    [this] ROUTE_SIG {
      auto stream = elle::IOStream(data.istreambuf());
      auto json = boost::any_cast<elle::json::Object>(elle::json::read(stream));
      auto name = boost::any_cast<std::string>(json.at("Name"));
      if (_manager.exists(name))
        return "{\"Err\": \"\", \"Volume\": {\"Name\": \"" + name + "\" }}";
      else
        return "{\"Err\": \"No such mount\"}";
    });
  _server->register_route("/VolumeDriver.Mount", reactor::http::Method::POST,
    [this] ROUTE_SIG {
      auto stream = elle::IOStream(data.istreambuf());
      auto json = boost::any_cast<elle::json::Object>(elle::json::read(stream));
      auto name = boost::any_cast<std::string>(json.at("Name"));
      auto it = _mount_count.find(name);
      if (it != _mount_count.end())
      {
        ELLE_TRACE("Already mounted");
        ++it->second;
      }
      else
      {
        _manager.start(name, {}, true, true);
        _mount_count.insert(std::make_pair(name, 1));
      }
      std::string res = "{\"Err\": \"\", \"Mountpoint\": \""
          + _manager.mountpoint(name) +"\"}";
      ELLE_TRACE("reply: %s", res);
      return res;
    });
  _server->register_route("/VolumeDriver.Unmount", reactor::http::Method::POST,
    [this] ROUTE_SIG {
      auto stream = elle::IOStream(data.istreambuf());
      auto json = boost::any_cast<elle::json::Object>(elle::json::read(stream));
      auto name = boost::any_cast<std::string>(json.at("Name"));
      auto it = _mount_count.find(name);
      if (it == _mount_count.end())
        return "{\"Err\": \"No such mount\"}";
      --it->second;
      if (it->second == 0)
      {
        _mount_count.erase(it);
        _manager.stop(name);
      }
      return "{\"Err\": \"\"}";
    });
  _server->register_route("/VolumeDriver.Path", reactor::http::Method::POST,
    [this] ROUTE_SIG {
      auto stream = elle::IOStream(data.istreambuf());
      auto json = boost::any_cast<elle::json::Object>(elle::json::read(stream));
      auto name = boost::any_cast<std::string>(json.at("Name"));
      return "{\"Err\": \"\", \"Mountpoint\": \""
          + _manager.mountpoint(name) +"\"}";
    });
  _server->register_route("/VolumeDriver.List", reactor::http::Method::POST,
    [this] ROUTE_SIG {
      auto list = _manager.list();
      std::string res("{\"Err\": \"\", \"Volumes\": [");
      for (auto const& n: list)
        res += "{\"Name\": \"" + n + "\"},";
      res = res.substr(0, res.size()-1);
      res += "]}";
      return res;
    });
}

int
main(int argc, char** argv)
{
  using boost::program_options::value;
  using boost::program_options::bool_switch;
  Modes modes {
    {
      "status",
      "Query daemon status",
      &status,
      "",
      {}
    },
    {
      "start",
      "Start daemon",
      &start,
      "",
      {
        { "foreground,f", bool_switch(), "do not daemonize" },
        { "log-level,l", value<std::string>(), "Log level to start volumes with"},
        { "log-path,d", value<std::string>(), "Store volume logs in given path"},
        { "docker-socket-tcp", bool_switch(), "Use a TCP socket for docker plugin"},
      }
    },
    {
      "stop",
      "Stop daemon",
      &stop,
      "",
      {}
    },
  };
  return infinit::main("Infinit daemon", modes, argc, argv);
}
