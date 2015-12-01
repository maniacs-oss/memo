#include <infinit/model/doughnut/Remote.hh>

#include <elle/log.hh>
#include <elle/os/environ.hh>
#include <elle/utils.hh>
#include <elle/bench.hh>

#include <reactor/thread.hh>
#include <reactor/scheduler.hh>

#include <infinit/RPC.hh>

ELLE_LOG_COMPONENT("infinit.model.doughnut.Remote")

#define BENCH(name)                                      \
  static elle::Bench bench("bench.remote." name, 10000_sec); \
  elle::Bench::BenchScope bs(bench)

namespace infinit
{
  namespace model
  {
    namespace doughnut
    {
      /*-------------.
      | Construction |
      `-------------*/

      Remote::Remote(Doughnut& doughnut, Address id,
                     std::string const& host, int port)
        : Super(std::move(id))
        , _doughnut(doughnut)
        , _socket()
        , _serializer()
        , _channels()
        , _connection_thread()
        , _fast_fail(false)
      {
        this->_connect(
          elle::sprintf("%s:%s", host, port),
          [host, port, this] () -> std::iostream&
          {
            this->_socket.reset(
              new reactor::network::TCPSocket(host, port));
            return *this->_socket;
          });
      }

      Remote::Remote(Doughnut& doughnut, Address id,
                     boost::asio::ip::tcp::endpoint endpoint)
        : Super(std::move(id))
        , _doughnut(doughnut)
        , _socket(nullptr)
        , _serializer()
        , _channels()
        , _connection_thread()
        , _fast_fail(false)
      {
        this->initiate_connect(endpoint);
      }

      void
      Remote::initiate_connect(boost::asio::ip::tcp::endpoint endpoint)
      {
        this->_connect(
          elle::sprintf("%s", endpoint),
          [endpoint, this] () -> std::iostream&
          {
            this->_socket.reset(
              new reactor::network::TCPSocket(endpoint));
            return *this->_socket;
          });
      }

      Remote::Remote(Doughnut& doughnut, Address id,
                     boost::asio::ip::udp::endpoint endpoint,
                     reactor::network::UTPServer& server)
        : Super(std::move(id))
        , _doughnut(doughnut)
        , _utp_socket(nullptr)
        , _serializer()
        , _channels()
        , _connection_thread()
        , _fast_fail(false)
      {
        this->_connect(
          elle::sprintf("%s", endpoint),
          [this, endpoint, &server] () -> std::iostream&
          {
            this->_utp_socket.reset(
              new reactor::network::UTPSocket(
                server, endpoint.address().to_string(), endpoint.port()));
            return *this->_utp_socket;
          });
      }

      Remote::Remote(Doughnut& doughnut, Address id,
                     std::vector<boost::asio::ip::udp::endpoint> endpoints,
                     std::string const& peer_id,
                     reactor::network::UTPServer& server)
        : Super(std::move(id))
        , _doughnut(doughnut)
        , _utp_socket(nullptr)
        , _serializer()
        , _channels()
        , _connection_thread()
        , _fast_fail(false)
      {
        this->initiate_connect(endpoints, peer_id, server);
      }

      void
      Remote::initiate_connect(std::vector<boost::asio::ip::udp::endpoint> endpoints,
                               std::string const& peer_id,
                               reactor::network::UTPServer& server)
      {
        this->_connect(
          elle::sprintf("%s", endpoints),
          [this, endpoints, peer_id, &server] () -> std::iostream&
          {
            this->_utp_socket.reset(new reactor::network::UTPSocket(server));
            this->_utp_socket->connect(peer_id, endpoints);
            return *this->_utp_socket;
          });
      }

      Remote::~Remote()
      {}

      /*-----------.
      | Networking |
      `-----------*/

      void
      Remote::_connect(
        std::string endpoint,
        std::function <std::iostream& ()> const& socket)
      {
        ELLE_TRACE_SCOPE("%s: connect", *this);
        this->_connector = socket;
        this->_endpoint = endpoint;
        this->_connection_thread.reset(
          new reactor::Thread(
            elle::sprintf("%s connection", *this),
            [this, endpoint, socket]
            {
              try
              {
                _connected = false;
                this->_serializer.reset(
                  new protocol::Serializer(socket(), false));
                this->_channels.reset(
                  new protocol::ChanneledStream(*this->_serializer));
                static bool disable_key = getenv("INFINIT_RPC_DISABLE_CRYPTO");
                if (!disable_key)
                  this->_key_exchange();
                ELLE_TRACE("connected");
                _connected = true;
              }
              catch (reactor::network::Exception const&)
              { // Upper layers may retry on network::Exception
                throw;
              }
              catch (elle::Error const&)
              {
                elle::throw_with_nested(
                  elle::Error(
                    elle::sprintf("connection failed to %s", endpoint)));
              }
            },
            reactor::Thread::managed = true));
      }

      void
      Remote::connect(elle::DurationOpt timeout)
      {
        do
        {
          auto start = boost::posix_time::microsec_clock::universal_time();
          if (!reactor::wait(*this->_connection_thread, timeout))
            throw reactor::network::TimeOut();
          // Either connect succeeded, or it was restarted
          if (timeout)
          {
            timeout = *timeout -
              (boost::posix_time::microsec_clock::universal_time() - start);
            if (timeout->is_negative() && !_connected)
              throw reactor::network::TimeOut();
          }
        }
        while (!_connected);
      }

      void
      Remote::reconnect(elle::DurationOpt timeout)
      {
        this->_credentials = {};
        if (!retry_connect() || !retry_connect()(*this))
          _connect(this->_endpoint, this->_connector);
        connect(timeout);
      }

      /*-------.
      | Blocks |
      `-------*/

      void
      Remote::_key_exchange()
      {
        ELLE_TRACE_SCOPE("%s: exchange keys", *this);
        // challenge, token
        typedef std::pair<elle::Buffer, elle::Buffer> Challenge;
        RPC<std::pair<Challenge, std::unique_ptr<Passport>>(Passport const&)>
          auth_syn("auth_syn", *this->_channels, nullptr);
        auto challenge_passport = auth_syn(this->_doughnut.passport());
        auto& remote_passport = challenge_passport.second;
        ELLE_ASSERT(remote_passport);
        // validate res
        bool check = remote_passport->verify(this->_doughnut.owner());
        if (!check)
        {
          ELLE_LOG("Passport validation failed.");
          throw elle::Error("Passport validation failed");
        }
        ELLE_DEBUG("got valid remote passport");
        // sign the challenge
        auto signed_challenge = this->_doughnut.keys().k().sign(
          challenge_passport.first.first,
          infinit::cryptography::rsa::Padding::pss,
          infinit::cryptography::Oneway::sha256);
        // generate, seal
        // dont set _key yet so that our 2 rpcs are in cleartext
        auto key = infinit::cryptography::secretkey::generate(256);
        elle::Buffer password = key.password();
        auto sealed_key = remote_passport->user().seal(password,
          infinit::cryptography::Cipher::aes256,
          infinit::cryptography::Mode::cbc);
        ELLE_DEBUG("acknowledge authentication")
        {
          RPC<bool (elle::Buffer const&,
                    elle::Buffer const&,
                    elle::Buffer const&)>
            auth_ack("auth_ack", *this->_channels, nullptr);
          auth_ack(sealed_key,
                   challenge_passport.first.second,
                   signed_challenge);
          this->_credentials = std::move(password);
        }
      }

      void
      Remote::store(blocks::Block const& block, StoreMode mode)
      {
        BENCH("store");
        ELLE_ASSERT(&block);
        ELLE_TRACE_SCOPE("%s: store %f", *this, block);
        auto store = make_rpc<void (blocks::Block const&, StoreMode)>("store");
        store.set_context<Doughnut*>(&this->_doughnut);
        store(block, mode);
      }

      std::unique_ptr<blocks::Block>
      Remote::_fetch(Address address,
                    boost::optional<int> local_version) const
      {
        BENCH("fetch");
        auto fetch = elle::unconst(this)->make_rpc<
          std::unique_ptr<blocks::Block>(Address,
                                         boost::optional<int>)>("fetch");
        fetch.set_context<Doughnut*>(&this->_doughnut);
        return fetch(std::move(address), std::move(local_version));
      }

      void
      Remote::remove(Address address)
      {
        BENCH("remove");
        ELLE_TRACE_SCOPE("%s: remove %x", *this, address);
        auto remove = make_rpc<void (Address)>("remove");
        remove(address);
      }

      /*----------.
      | Printable |
      `----------*/

      void
      Remote::print(std::ostream& stream) const
      {
        auto name = elle::type_info(*this).name();
        if (this->_socket)
          elle::fprintf(stream, "%s(%s)", name, *this->_socket);
        else if (this->_utp_socket)
          elle::fprintf(stream, "%s(%s)", name, *this->_utp_socket);
        else
          elle::fprintf(stream, "%s(%s)", name, this);
      }
    }
  }
}
