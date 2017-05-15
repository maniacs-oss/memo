#include <algorithm>

#include <infinit/cli/Network.hh>

#include <boost/algorithm/string/trim.hpp>
#include <boost/range/algorithm_ext/erase.hpp>

#include <elle/algorithm.hh>
#include <elle/make-vector.hh>
#include <elle/reactor/network/resolve.hh>

#include <infinit/Network.hh> // Storages
#include <infinit/cli/Infinit.hh>
#include <infinit/cli/utility.hh>
#include <infinit/cli/xattrs.hh>
#include <infinit/grpc/grpc.hh>
#include <infinit/model/MissingBlock.hh>
#include <infinit/model/MonitoringServer.hh>
#include <infinit/model/blocks/ACLBlock.hh>
#include <infinit/model/blocks/ImmutableBlock.hh>
#include <infinit/model/blocks/MutableBlock.hh>
#include <infinit/model/doughnut/Doughnut.hh>
#include <infinit/model/doughnut/NB.hh>
#include <infinit/model/doughnut/consensus/Paxos.hh>
#include <infinit/model/prometheus.hh>
#include <infinit/overlay/Kalimero.hh>
#include <infinit/overlay/kelips/Kelips.hh>
#include <infinit/overlay/kouncil/Configuration.hh>
#include <infinit/storage/Storage.hh>
#include <infinit/storage/Strip.hh>

#ifndef INFINIT_WINDOWS
# include <elle/reactor/network/unix-domain-socket.hh>
#endif

ELLE_LOG_COMPONENT("cli.network");

namespace infinit
{
  namespace cli
  {
    using Strings = Network::Strings;
    namespace bfs = boost::filesystem;
    namespace dnut = infinit::model::doughnut;

    Network::Network(Infinit& infinit)
      : Object(infinit)
      , create(*this,
               "Create a network",
               cli::name,
               cli::description = boost::none,
               cli::silo = Strings{},
               cli::port = boost::none,
               cli::replication_factor = 1,
               cli::eviction_delay = boost::none,
               cli::output = boost::none,
               cli::push_network = false,
               cli::push = false,
               cli::admin_r = Strings{},
               cli::admin_rw = Strings{},
               cli::peer = Strings{},
               // Consensus types.
               cli::paxos = false,
               cli::no_consensus = false,
               // Overlay types.
               cli::kelips = false,
               cli::kalimero = false,
               cli::kouncil = false,
               // Kelips options,
               cli::nodes = boost::none,
               cli::k = boost::none,
               cli::kelips_contact_timeout = boost::none,
               cli::encrypt = boost::none,
               // Generic options
               cli::protocol = boost::none,
               cli::tcp_heartbeat = boost::none,
               cli::disable_encrypt_at_rest = false,
               cli::disable_encrypt_rpc = false,
               cli::disable_signature = false)
      , delete_(*this,
                "Delete a network locally",
                cli::name,
                cli::pull = false,
                cli::purge = false,
                cli::unlink = false)
      , export_(*this,
                "Export a network",
                cli::name,
                cli::output = boost::none)
      , fetch(*this,
              "Fetch a network from {hub}",
              cli::name = boost::none)
      , import(*this,
               "Fetch a network",
               cli::input = boost::none)
#ifndef INFINIT_WINDOWS
      , inspect(*this,
                "Get information about a running network",
                cli::name,
                cli::output = boost::none,
                cli::status = false,
                cli::peers = false,
                cli::all = false,
                cli::redundancy = false)
#endif
      , link(*this,
             "Link this device to a network",
             cli::name,
             cli::silo = Strings{},
             cli::output = boost::none,
             cli::node_id = boost::none)
      , list(*this, "List networks")
      , list_services(*this,
                      "List network registered services",
                      cli::name,
                      cli::peer = Strings(),
                      cli::async = false,
                      cli::cache = false,
                      cli::cache_ram_size = boost::none,
                      cli::cache_ram_ttl = boost::none,
                      cli::cache_ram_invalidation = boost::none,
                      cli::cache_disk_size = boost::none,
                      cli::fetch_endpoints = false,
                      cli::fetch = false,
                      cli::push_endpoints = false,
                      cli::push = false,
                      cli::publish = false,
                      cli::endpoints_file = boost::none,
                      cli::port_file = boost::none,
                      cli::port = boost::none,
                      cli::peers_file = boost::none,
                      cli::listen = boost::none,
                      cli::fetch_endpoints_interval = boost::none,
                      cli::no_local_endpoints = false,
                      cli::no_public_endpoints = false,
                      cli::advertise_host = Strings())
      , list_silos(*this,
                   "List all silos contributed by this device to a network",
                   cli::name)
      , pull(*this,
             "Remove a network from {hub}",
             cli::name,
             cli::purge = false)
      , push(*this,
             "Push a network to {hub}",
             cli::name)
      , run(*this,
            "Run a network",
            cli::name,
            cli::input = boost::none,
#ifndef INFINIT_WINDOWS
            cli::daemon = false,
            cli::monitoring = true,
#endif
            cli::peer = Strings{},
            cli::async = false,
            cli::cache = false,
            cli::cache_ram_size = boost::none,
            cli::cache_ram_ttl = boost::none,
            cli::cache_ram_invalidation = boost::none,
            cli::cache_disk_size = boost::none,
            cli::fetch_endpoints = false,
            cli::fetch = false,
            cli::push_endpoints = false,
            cli::push = false,
            cli::publish = false,
            cli::endpoints_file = boost::none,
            cli::port_file = boost::none,
            cli::port = boost::none,
            cli::peers_file = boost::none,
            cli::listen = boost::none,
            cli::fetch_endpoints_interval = boost::none,
            cli::no_local_endpoints = false,
            cli::no_public_endpoints = false,
            cli::advertise_host = Strings{},
            cli::grpc = boost::none,
#if INFINIT_ENABLE_PROMETHEUS
            cli::prometheus = boost::none,
#endif
            cli::paxos_rebalancing_auto_expand = boost::none,
            cli::paxos_rebalancing_inspect = boost::none)
      , stat(*this,
              "Fetch stats of a network on {hub}",
              cli::name)
      , unlink(*this,
               "Unlink this device from a network",
               cli::name)
      , update(*this,
               "Update a network",
               cli::name,
               cli::description = boost::none,
               cli::port = boost::none,
               cli::output = boost::none,
               cli::push_network = false,
               cli::push = false,
               cli::admin_r = Strings{},
               cli::admin_rw = Strings{},
               cli::admin_remove = Strings{},
               cli::mountpoint = boost::none,
               cli::peer = Strings{},
               cli::protocol = boost::none)
    {}


    /*---------------.
    | Mode: create.  |
    `---------------*/

    namespace
    {
      auto
      make_kelips_config(boost::optional<int> nodes,
                         boost::optional<int> k,
                         boost::optional<std::string> const& timeout,
                         boost::optional<std::string> const& encrypt,
                         boost::optional<std::string> const& protocol)
      {
        auto res = std::make_unique<infinit::overlay::kelips::Configuration>();
        res->k = [&]{
          if (k)
            return *k;
          else if (nodes)
            {
              if (*nodes < 10)
                return 1;
              else if (*nodes < 25)
                return *nodes / 5;
              else
                return int(sqrt(*nodes));
            }
          else
            return 1;
        }();
        if (timeout)
          res->contact_timeout_ms =
            std::chrono::duration_from_string<std::chrono::milliseconds>(*timeout)
            .count();
        // encrypt support.
        {
          auto enc = encrypt.value_or("yes");
          if (enc == "no")
          {
            res->encrypt = false;
            res->accept_plain = true;
          }
          else if (enc == "lazy")
          {
            res->encrypt = true;
            res->accept_plain = true;
          }
          else if (enc == "yes")
          {
            res->encrypt = true;
            res->accept_plain = false;
          }
          else
            elle::err<CLIError>("'encrypt' must be 'no', 'lazy' or 'yes': %s",
                                enc);
        }
        if (protocol)
          res->rpc_protocol = protocol_get(protocol);
        return res;
      }

      /// Turn a list of addresses (e.g., `foo.bar.fr:http`) and/or
      /// filenames that contains such addresses, into a list of
      /// Endpoints.
      ///
      /// Yes, a list of Endpoints, not a list of Endpoint, because
      /// foo.bar.fr might actually denote several hosts, and we want
      /// to reach each one individually.
      std::vector<infinit::model::Endpoints>
      parse_peers(Strings const& peers)
      {
        auto res = std::vector<infinit::model::Endpoints>{};
        for (auto const& peer: peers)
        {
          auto const eps
            = bfs::exists(peer)
            ? model::endpoints_from_file(peer)
            : elle::reactor::network::resolve_udp_repr(peer);
          for (auto const& ep: eps)
            res.emplace_back(infinit::model::Endpoints{ep});
        }
        return res;
      }

      std::unique_ptr<infinit::storage::StorageConfig>
      make_silo_config(infinit::Infinit& ifnt,
                       Strings const& silos)
      {
        auto res = std::unique_ptr<infinit::storage::StorageConfig>{};
        if (silos.empty())
          return {};
        else
        {
          auto backends
            = elle::make_vector(silos,
                                [&](auto const& s) { return ifnt.silo_get(s); });
          if (backends.size() == 1)
            return std::move(backends[0]);
          else
            return std::make_unique<infinit::storage::StripStorageConfig>
              (std::move(backends));
        }
      }

      // Consensus
      auto
      make_consensus_config(bool paxos,
                            bool no_consensus,
                            int replication_factor,
                            boost::optional<std::string> const& eviction_delay)
        -> std::unique_ptr<dnut::consensus::Configuration>
      {
        if (replication_factor < 1)
          elle::err<CLIError>("replication factor must be greater than 0");
        if (!no_consensus)
          paxos = true;
        if (1 < no_consensus + paxos)
          elle::err<CLIError>("more than one consensus specified");
        if (paxos)
          return std::make_unique<
            dnut::consensus::Paxos::Configuration>(
              replication_factor,
              eviction_delay ?
              std::chrono::duration_from_string<std::chrono::seconds>(*eviction_delay) :
              std::chrono::seconds(10 * 60));
        else
        {
          if (replication_factor != 1)
            elle::err("without consensus, replication factor must be 1");
          return std::make_unique<
            dnut::consensus::Configuration>();
        }
      }

      auto
      make_admin_keys(infinit::Infinit& ifnt,
                      Strings const& admin_r,
                      Strings const& admin_rw)
        -> dnut::AdminKeys
      {
        auto res = dnut::AdminKeys{};
        auto add =
          [&res] (elle::cryptography::rsa::PublicKey const& key,
                  bool read, bool write)
          {
            if (read && !write)
              push_back_if_missing(res.r, key);
            // write implies rw.
            if (write)
              push_back_if_missing(res.w, key);
          };
        for (auto const& a: admin_r)
          add(ifnt.user_get(a).public_key, true, false);
        for (auto const& a: admin_rw)
          add(ifnt.user_get(a).public_key, true, true);
        return res;
      }
    }

    void
    Network::mode_create(
      std::string const& network_name,
      boost::optional<std::string> const& description,
      Strings const& silos_names,
      boost::optional<int> port,
      int replication_factor,
      boost::optional<std::string> const& eviction_delay,
      boost::optional<std::string> const& output_name,
      bool push_network,
      bool push,
      Strings const& admin_r,
      Strings const& admin_rw,
      Strings const& peer,
      // Consensus types.
      bool paxos,
      bool no_consensus,
      // Overlay types.
      bool kelips,
      bool kalimero,
      bool kouncil,
      // Kelips options,
      boost::optional<int> nodes,
      boost::optional<int> k,
      boost::optional<std::string> kelips_contact_timeout,
      boost::optional<std::string> encrypt,
      boost::optional<std::string> protocol,
      boost::optional<std::chrono::milliseconds> tcp_heartbeat,
      bool disable_encrypt_at_rest,
      bool disable_encrypt_rpc,
      bool disable_signature)
    {
      ELLE_TRACE_SCOPE("create");
      auto& cli = this->cli();
      auto& ifnt = cli.infinit();
      auto owner = cli.as_user();
      auto overlay_config =
        [&] () -> std::unique_ptr<infinit::overlay::Configuration>
        {
          if (1 < kalimero + kelips + kouncil)
            elle::err<CLIError>("only one overlay type must be specified");
          if (kalimero)
            return std::make_unique<infinit::overlay::KalimeroConfiguration>();
          else if (kelips)
            return make_kelips_config(nodes, k, kelips_contact_timeout,
                                      encrypt, protocol);
          else
            return std::make_unique<infinit::overlay::kouncil::Configuration>();
        }();
      if (protocol)
        overlay_config->rpc_protocol = protocol_get(protocol);
      auto silo = make_silo_config(ifnt, silos_names);
      auto consensus_config = make_consensus_config(paxos,
                                                    no_consensus,
                                                    replication_factor,
                                                    eviction_delay);
      auto admin_keys = make_admin_keys(ifnt, admin_r, admin_rw);
      infinit::model::doughnut::EncryptOptions encrypt_options(
        !disable_encrypt_at_rest,
        !disable_encrypt_rpc,
        !disable_signature);
      auto dht =
        std::make_unique<dnut::Configuration>(
          infinit::model::Address::random(0),
          std::move(consensus_config),
          std::move(overlay_config),
          std::move(silo),
          owner.keypair(),
          std::make_shared<elle::cryptography::rsa::PublicKey>(owner.public_key),
          dnut::Passport(
            owner.public_key,
            ifnt.qualified_name(network_name, owner),
            elle::cryptography::rsa::KeyPair(owner.public_key,
                                                owner.private_key.get())),
          owner.name,
          std::move(port),
          infinit::version(),
          admin_keys,
          parse_peers(peer),
          tcp_heartbeat,
          encrypt_options);
      {
        auto network = infinit::Network(ifnt.qualified_name(network_name, owner),
                                        std::move(dht),
                                        description);
        auto desc = [&] {
            if (output_name)
            {
              auto output = cli.get_output(output_name);
              elle::serialization::json::serialize(network, *output, false);
              return std::make_unique<infinit::NetworkDescriptor>(std::move(network));
            }
            else
            {
              ifnt.network_save(owner, network);
              auto res = std::make_unique<infinit::NetworkDescriptor>(std::move(network));
              ifnt.network_save(*res);
              return res;
            }
          }();
        if (push || push_network)
          ifnt.beyond_push("network", desc->name, *desc, owner);
      }
    }

    /*---------------.
    | Mode: delete.  |
    `---------------*/

    void
    Network::mode_delete(std::string const& network_name,
                         bool pull,
                         bool purge,
                         bool unlink)
    {
      ELLE_TRACE_SCOPE("delete");
      auto& cli = this->cli();
      auto& ifnt = cli.infinit();
      auto owner = cli.as_user();

      auto network = ifnt.network_get(network_name, owner, false);
      auto linked_users = ifnt.network_linked_users(network.name);
      if (linked_users.size() && !unlink)
      {
        auto user_names = elle::make_vector(linked_users,
                                            [](auto const& u) { return u.name; });
        elle::err("Network is still linked with this device by %s. "
                  "Please unlink it first or add the --unlink flag",
                  user_names);
      }
      if (purge)
      {
        auto volumes = ifnt.volumes_for_network(network.name);
        // FIXME: wouldn't a simple loop suffice?  With a
        // delete_volume function.  Actually, that's quite some
        // duplication with Volument::mode_delete, no?
        for (auto const& volume: volumes)
          for (auto const& drive: ifnt.drives_for_volume(volume.name))
            ifnt.drive_delete(drive);
        for (auto const& volume: volumes)
          ifnt.volume_delete(ifnt.volume_get(volume.name));
        for (auto const& user: ifnt.user_passports_for_network(network.name))
          ifnt.passport_delete(network.name, user);
      }
      if (pull)
        ifnt.beyond_delete("network", network.name, owner, true, purge);
      ifnt.network_delete(network.name, owner, unlink);
    }


    /*---------------.
    | Mode: export.  |
    `---------------*/

    void
    Network::mode_export(std::string const& network_name,
                         boost::optional<std::string> const& output_name)
    {
      ELLE_TRACE_SCOPE("export");
      auto& cli = this->cli();
      auto& ifnt = cli.infinit();
      auto owner = cli.as_user();
      auto name = ifnt.qualified_name(network_name, owner);
      auto desc = ifnt.network_descriptor_get(network_name, owner);
      auto output = cli.get_output(output_name);
      name = desc.name;
      elle::serialization::json::serialize(desc, *output, false);
      cli.report_exported(*output, "network", desc.name);
    }


    /*--------------.
    | Mode: fetch.  |
    `--------------*/

    void
    Network::mode_fetch(boost::optional<std::string> const& network_name)
    {
      ELLE_TRACE_SCOPE("export");
      auto& cli = this->cli();
      auto& ifnt = cli.infinit();
      auto owner = cli.as_user();

      auto save = [&ifnt,&owner,&cli] (infinit::NetworkDescriptor desc_) {
        // Save or update network descriptor.
        ifnt.network_save(desc_, true);
        for (auto const& u: ifnt.network_linked_users(desc_.name))
        {
          // Copy network descriptor.
          auto desc = desc_;
          auto network = ifnt.network_get(desc.name, u, false);
          if (network.model)
          {
            auto* d = dynamic_cast<dnut::Configuration*>(
              network.model.get()
            );
            auto updated_network = infinit::Network(
              desc.name,
              std::make_unique<dnut::Configuration>(
                d->id,
                std::move(desc.consensus),
                std::move(desc.overlay),
                // XXX[Storage]: dnut::Configuration::storage ?
                std::move(d->storage),
                u.keypair(),
                std::make_shared<elle::cryptography::rsa::PublicKey>(
                  desc.owner),
                d->passport,
                u.name,
                d->port,
                desc.version,
                desc.admin_keys,
                desc.peers,
                desc.tcp_heartbeat,
                desc.encrypt_options),
              desc.description);
            // Update linked network for user.
            ifnt.network_save(u, updated_network, true);
          }
        }
      };
      if (network_name)
      {
        auto name = ifnt.qualified_name(*network_name, owner);
        auto network = ifnt.beyond_fetch<infinit::NetworkDescriptor>(
          "network", name);
        save(network);
      }
      else // Fetch all networks for owner.
      {
        using Networks
          = std::unordered_map<std::string, std::vector<infinit::NetworkDescriptor>>;
        auto res =
          ifnt.beyond_fetch<Networks>(
            elle::sprintf("users/%s/networks", owner.name),
            "networks for user",
            owner.name,
            owner);
        for (auto const& n: res["networks"])
          save(n);
      }
    }


    /*---------------.
    | Mode: import.  |
    `---------------*/
    void
    Network::mode_import(boost::optional<std::string> const& input_name)
    {
      ELLE_TRACE_SCOPE("import");
      auto& cli = this->cli();
      auto& ifnt = cli.infinit();
      auto input = this->cli().get_input(input_name);
      auto desc =
        elle::serialization::json::deserialize<infinit::NetworkDescriptor>
        (*input, false);
      ifnt.network_save(desc);
      cli.report_imported("network", desc.name);
    }

#ifndef INFINIT_WINDOWS
    /*----------------.
    | Mode: inspect.  |
    `----------------*/

    void
    Network::mode_inspect(std::string const& network_name,
                          boost::optional<std::string> const& output_name,
                          bool status,
                          bool peers,
                          bool all,
                          bool redundancy)
    {
      ELLE_TRACE_SCOPE("inspect");
      auto& cli = this->cli();
      auto& ifnt = cli.infinit();
      auto owner = cli.as_user();
      auto network = ifnt.network_get(network_name, owner, false);
      auto s_path = network.monitoring_socket_path(owner);
      if (!bfs::exists(s_path))
        elle::err("network not running or monitoring disabled");
      elle::reactor::network::UnixDomainSocket socket(s_path);
      using Monitoring = infinit::model::MonitoringServer;
      using Query = infinit::model::MonitoringServer::MonitorQuery::Query;
      auto do_query = [&] (Query query_val)
        {
          auto query = Monitoring::MonitorQuery(query_val);
          elle::serialization::json::serialize(query, socket, false, false);
          auto json = boost::any_cast<elle::json::Object>(elle::json::read(socket));
          return Monitoring::MonitorResponse(std::move(json));
        };
      auto print_response = [&] (Monitoring::MonitorResponse const& response)
        {
          if (cli.script())
            elle::json::write(*cli.get_output(output_name), response.as_object());
          else
          {
            if (response.error)
              std::cout << "Error: " << response.error.get() << std::endl;
            if (response.result)
              std::cout << elle::json::pretty_print(response.result.get())
                        << std::endl;
            else
              std::cout << "Running" << std::endl;
          }
        };

      if (status)
        print_response(do_query(Query::Status));
      else if (all)
        print_response(do_query(Query::Stats));
      else if (redundancy)
      {
        auto res = do_query(Query::Stats);
        if (res.result)
        {
          auto redundancy =
            boost::any_cast<elle::json::Object>(res.result.get()["redundancy"]);
          if (cli.script())
            elle::json::write(*cli.get_output(output_name), redundancy);
          else
            std::cout << elle::json::pretty_print(redundancy) << std::endl;
        }
      }
      else if (peers)
      {
        auto res = do_query(Query::Stats);
        if (res.result)
        {
          auto peer_list =
            boost::any_cast<elle::json::Array>(res.result.get()["peers"]);
          if (cli.script())
            elle::json::write(*cli.get_output(output_name), peer_list);
          else
          {
            if (peer_list.empty())
              std::cout << "No peers" << std::endl;
            else
              for (auto obj: peer_list)
              {
                auto json = boost::any_cast<elle::json::Object>(obj);
                std::cout << boost::any_cast<std::string>(json["id"]);
                json.erase("id");
                if (json.size())
                  std::cout << ": " << elle::json::pretty_print(json) << std::endl;
              }
          }
        }
      }
      else
        elle::err<CLIError>("specify either \"--status\", \"--peers\","
                            " \"--redundancy\", or \"--all\"");
    }
#endif


    /*-------------.
    | Mode: link.  |
    `-------------*/

    void
    Network::mode_link(std::string const& network_name,
                       Strings const& silos_names,
                       boost::optional<std::string> const& output_name,
                       boost::optional<std::string> const& node_id)
    {
      ELLE_TRACE_SCOPE("link");
      auto& cli = this->cli();
      auto& ifnt = cli.infinit();
      auto owner = cli.as_user();

      {
        auto network = ifnt.network_get(network_name, owner, false);
        if (network.model)
          elle::err("%s is already linked with %s", network.name, owner.name);
      }

      auto silo = make_silo_config(ifnt, silos_names);
      auto desc = ifnt.network_descriptor_get(network_name, owner);
      auto const passport = [&] () -> infinit::Infinit::Passport
        {
          if (owner.public_key == desc.owner)
            return {owner.public_key, desc.name,
                    elle::cryptography::rsa::KeyPair(owner.public_key,
                                                        owner.private_key.get())};
          try
          {
            return ifnt.passport_get(desc.name, owner.name);
          }
          catch (infinit::MissingLocalResource const&)
          {
            elle::err("missing passport (%s: %s), "
                      "use infinit-passport to fetch or import",
                      desc.name, owner.name);
          }
        }();
      if (!passport.verify(passport.certifier() ? *passport.certifier() : desc.owner))
        elle::err("passport signature is invalid");
      if (silo && !passport.allow_storage())
        elle::err("passport does not allow storage");
      auto const id = [&]
        {
          if (node_id)
            {
              std::stringstream ss(elle::sprintf("\"%s\"", *node_id));
              namespace json = elle::serialization::json;
              return json::deserialize<infinit::model::Address>(ss, false);
            }
          else
            return infinit::model::Address::random(0); // FIXME
        }();
      auto network = infinit::Network(
        desc.name,
        std::make_unique<dnut::Configuration>(
          id,
          std::move(desc.consensus),
          std::move(desc.overlay),
          std::move(silo),
          owner.keypair(),
          std::make_shared<elle::cryptography::rsa::PublicKey>(desc.owner),
          std::move(passport),
          owner.name,
          boost::optional<int>(),
          desc.version,
          desc.admin_keys,
          desc.peers,
          desc.tcp_heartbeat,
          desc.encrypt_options),
        desc.description);
      if (output_name)
        ifnt.save(*cli.get_output(output_name), network, false);
      else
      {
        ifnt.network_save(owner, network, true);
        cli.report_action("linked", "device to network", desc.name);
      }
    }


    /*-------------.
    | Mode: list.  |
    `-------------*/

    void
    Network::mode_list()
    {
      ELLE_TRACE_SCOPE("list");
      auto& cli = this->cli();
      auto& ifnt = cli.infinit();
      auto owner = cli.as_user();

      if (cli.script())
      {
        auto l = elle::json::make_array(ifnt.networks_get(owner),
                                        [&] (auto const& network) {
          auto res = elle::json::Object
            {
              {"name", static_cast<std::string>(network.name)},
              {"linked", bool(network.model) && network.user_linked(owner)},
            };
          if (network.description)
            res["description"] = network.description.get();
          return res;
        });
        elle::json::write(std::cout, l);
      }
      else
      {
        for (auto const& network: ifnt.networks_get(owner))
        {
          std::cout << network.name;
          if (network.description)
            std::cout << " \"" << network.description.get() << "\"";
          if (network.model && network.user_linked(owner))
            std::cout << ": linked";
          else
            std::cout << ": not linked";
          std::cout << std::endl;
        }
      }
    }


    /*----------------------.
    | Mode: list_services.  |
    `----------------------*/

    namespace
    {
      /// Action to run by network_run.
      using Action =
        std::function<void (infinit::User& owner,
                            infinit::Network& network,
                            dnut::Doughnut& dht,
                            bool push)>;
      void
      network_run(Infinit& cli,
                  std::string const& network_name,
#ifndef INFINIT_WINDOWS
                  bool daemon = false,
                  bool monitoring = true,
#endif
                  Strings peer = {},
                  bool async = false,
                  bool cache = false,
                  boost::optional<int> cache_ram_size = {},
                  boost::optional<int> cache_ram_ttl = {},
                  boost::optional<int> cache_ram_invalidation = {},
                  boost::optional<uint64_t> cache_disk_size = {},
                  bool fetch = false,
                  bool push = false,
                  bool publish = false,
                  boost::optional<std::string> const& endpoints_file = {},
                  boost::optional<std::string> const& port_file = {},
                  boost::optional<int> port = {},
                  boost::optional<std::string> const& peers_file = {},
                  boost::optional<std::string> listen = {},
                  boost::optional<int> fetch_endpoints_interval = {},
                  bool no_local_endpoints = false,
                  bool no_public_endpoints = false,
                  Strings advertise_host = {},
                  boost::optional<std::string> grpc = {},
#if INFINIT_ENABLE_PROMETHEUS
                  boost::optional<std::string> prometheus = {},
#endif
                  boost::optional<bool> paxos_rebalancing_auto_expand = {},
                  boost::optional<bool> paxos_rebalancing_inspect = {},
                  Action const& action = {})
      {
        auto& ifnt = cli.infinit();
        auto owner = cli.as_user();
        auto network = ifnt.network_get(network_name, owner);
        if (paxos_rebalancing_auto_expand || paxos_rebalancing_inspect)
        {
          auto paxos = dynamic_cast<
            dnut::consensus::Paxos::Configuration*>(
              network.dht()->consensus.get());
          if (!paxos)
            elle::err<CLIError>("paxos options on non-paxos consensus");
          if (paxos_rebalancing_auto_expand)
            paxos->rebalance_auto_expand(*paxos_rebalancing_auto_expand);
          if (paxos_rebalancing_inspect)
            paxos->rebalance_inspect(*paxos_rebalancing_inspect);
        }
        network.ensure_allowed(owner, "run");
        cache |= (cache_ram_size || cache_ram_ttl
                  || cache_ram_invalidation || cache_disk_size);
        auto const listen_address
          = listen
          ? boost::asio::ip::address::from_string(*listen)
          : boost::optional<boost::asio::ip::address>{};
        auto dht = network.run(
          owner,
          false,
          cache, cache_ram_size, cache_ram_ttl, cache_ram_invalidation,
          async, cache_disk_size, cli.compatibility_version(), port,
          listen_address
#ifndef INFINIT_WINDOWS
          , monitoring
#endif
        );
        hook_stats_signals(*dht);
        elle::reactor::Thread::unique_ptr grpc_thread;
        if (grpc)
        {
          auto const eps = model::Endpoints{*grpc};
          auto const ep = *eps.begin();
          grpc_thread.reset(new elle::reactor::Thread("grpc",
            [dht=dht.get(), ep] {
              infinit::grpc::serve_grpc(*dht, boost::none, ep);
          }));
        }
#if INFINIT_ENABLE_PROMETHEUS
        if (prometheus)
          infinit::prometheus::endpoint(*prometheus);
#endif
        if (peers_file)
        {
          auto more_peers = hook_peer_discovery(*dht, *peers_file);
          ELLE_TRACE("Peer list file got %s peers", more_peers.size());
          if (!more_peers.empty())
            dht->overlay()->discover(more_peers);
        }
        if (!peer.empty())
          dht->overlay()->discover(parse_peers(peer));
        // Only push if we have are contributing storage.
        bool push_p = (push || publish) && dht->local() && dht->local()->storage();
        if (!dht->local() && push_p)
          elle::err("network %s is client only since no storage is attached", name);
        if (dht->local())
        {
          if (port_file)
            port_to_file(dht->local()->server_endpoint().port(), *port_file);
          if (endpoints_file)
            endpoints_to_file(dht->local()->server_endpoints(), *endpoints_file);
        }
#ifndef INFINIT_WINDOWS
        auto daemon_handle = daemon ? daemon_hold(0, 1) : daemon_invalid;
#endif
        auto run = [&, push_p]
          {
            elle::reactor::Thread::unique_ptr poll_thread;
            if (fetch || publish)
            {
              infinit::model::NodeLocations eps;
              network.beyond_fetch_endpoints(eps);
              dht->overlay()->discover(eps);
              if (fetch_endpoints_interval && *fetch_endpoints_interval > 0)
                poll_thread =
                  network.make_poll_beyond_thread(*dht, eps,
                                                  *fetch_endpoints_interval);
            }
#ifndef INFINIT_WINDOWS
            if (daemon)
            {
              ELLE_TRACE("releasing daemon");
              daemon_release(daemon_handle);
            }
#endif
            action(owner, network, *dht, push_p);
          };
        if (push_p)
          elle::With<InterfacePublisher>(
            ifnt,
            network, owner, dht->id(),
            dht->local()->server_endpoint().port(),
            advertise_host,
            no_local_endpoints,
            no_public_endpoints) << [&]
            {
              run();
            };
        else
          run();
      }
    }


    void
    Network::mode_list_services(std::string const& network_name,
                                Strings peer,
                                bool async,
                                bool cache,
                                boost::optional<int> cache_ram_size,
                                boost::optional<int> cache_ram_ttl,
                                boost::optional<int> cache_ram_invalidation,
                                boost::optional<uint64_t> cache_disk_size,
                                bool fetch_endpoints,
                                bool fetch,
                                bool push_endpoints,
                                bool push,
                                bool publish,
                                boost::optional<std::string> const& endpoints_file,
                                boost::optional<std::string> const& port_file,
                                boost::optional<int> port,
                                boost::optional<std::string> const& peers_file,
                                boost::optional<std::string> listen,
                                boost::optional<int> fetch_endpoints_interval,
                                bool no_local_endpoints,
                                bool no_public_endpoints,
                                Strings advertise_host)
    {
      ELLE_TRACE_SCOPE("list_services");
      auto& cli = this->cli();
      network_run
        (
         cli,
         network_name,
#ifndef INFINIT_WINDOWS
         // Passing explicitly the default values.  This is not nice.
         false,
         true,
#endif
         peer,
         async,
         cache,
         cache_ram_size,
         cache_ram_ttl,
         cache_ram_invalidation,
         cache_disk_size,
         fetch || fetch_endpoints,
         push || push_endpoints,
         publish,
         endpoints_file,
         port_file,
         port,
         peers_file,
         listen,
         fetch_endpoints_interval,
         no_local_endpoints,
         no_public_endpoints,
         advertise_host,
         {}, // grpc
#if INFINIT_ENABLE_PROMETHEUS
         {}, // prometheus
#endif
         {}, // paxos_rebalancing_auto_expand
         {}, // paxos_rebalancing_inspect
         [&] (infinit::User& owner,
              infinit::Network& network,
              dnut::Doughnut& dht,
              bool /*push*/)
         {
           auto services = dht.services();
           if (cli.script())
           {
             auto res = std::unordered_map<std::string, Strings>{};
             for (auto const& type: services)
               res.emplace(type.first,
                           elle::make_vector(type.second,
                                             [](auto const& service)
                                             {
                                               return service.first;
                                             }));
             elle::serialization::json::serialize(res, std::cout, false);
           }
           else
           {
             for (auto const& type: services)
             {
               std::cout << type.first << ":" << std::endl;
               for (auto const& service: type.second)
                 std::cout << "  " << service.first << std::endl;
             }
           }
         });
    }


    /*-------------------.
    | Mode: list_silos.  |
    `-------------------*/

    void
    Network::mode_list_silos(std::string const& network_name)
    {
      ELLE_TRACE_SCOPE("list_silos");
      auto& cli = this->cli();
      auto& ifnt = cli.infinit();
      auto owner = cli.as_user();

      auto network = ifnt.network_get(network_name, owner, true);
      // XXX[Storage]: Network::modem::storage
      if (network.model->storage)
      {
        if (auto strip = dynamic_cast<infinit::storage::StripStorageConfig*>(
            network.model->storage.get()))
          for (auto const& s: strip->storage)
            std::cout << s->name << "\n";
        else
          std::cout << network.model->storage->name;
        std::cout << std::endl;
      }
    }

    /*-------------.
    | Mode: pull.  |
    `-------------*/

    void
    Network::mode_pull(std::string const& network_name,
                       bool purge)
    {
      ELLE_TRACE_SCOPE("pull");
      auto& cli = this->cli();
      auto& ifnt = cli.infinit();
      auto owner = cli.as_user();
      auto name = ifnt.qualified_name(network_name, owner);
      ifnt.beyond_delete("network", name, owner, false, purge);
    }

    /*-------------.
    | Mode: push.  |
    `-------------*/

    void
    Network::mode_push(std::string const& network_name)
    {
      ELLE_TRACE_SCOPE("push");
      auto& cli = this->cli();
      auto& ifnt = cli.infinit();
      auto owner = cli.as_user();

      auto network = ifnt.network_get(network_name, owner);
      {
        auto& dht = *network.dht();
        auto owner_uid = infinit::User::uid(*dht.owner);
        auto desc = infinit::NetworkDescriptor(std::move(network));
        ifnt.beyond_push("network", desc.name, desc, owner, false, true);
      }
    }

    /*------------.
    | Mode: run.  |
    `------------*/

    namespace
    {
      /// An auxiliary struct to make reports.
      ///
      /// Respond
      ///   ("success", true)
      ///   ("value", block);
      ///
      /// serializes to std::cout.
      struct Respond
      {
        template <typename T>
        Respond(std::string const& k, T&& v)
          : response(std::cout, false, true)
        {
          response.serialize(k, std::forward<T>(v));
        }

        template <typename T>
        Respond&
        operator()(std::string const& k, T&& v)
        {
          response.serialize(k, std::forward<T>(v));
          return *this;
        }
        elle::serialization::json::SerializerOut response;
      };
    }

    void
    Network::mode_run(std::string const& network_name,
                      boost::optional<std::string> const& commands,
#ifndef INFINIT_WINDOWS
                      bool daemon,
                      bool monitoring,
#endif
                      Strings peer,
                      bool async,
                      bool cache,
                      boost::optional<int> cache_ram_size,
                      boost::optional<int> cache_ram_ttl,
                      boost::optional<int> cache_ram_invalidation,
                      boost::optional<uint64_t> cache_disk_size,
                      bool fetch_endpoints,
                      bool fetch,
                      bool push_endpoints,
                      bool push,
                      bool publish,
                      boost::optional<std::string> const& endpoints_file,
                      boost::optional<std::string> const& port_file,
                      boost::optional<int> port,
                      boost::optional<std::string> const& peers_file,
                      boost::optional<std::string> listen,
                      boost::optional<int> fetch_endpoints_interval,
                      bool no_local_endpoints,
                      bool no_public_endpoints,
                      Strings advertise_host,
                      boost::optional<std::string> grpc,
#if INFINIT_ENABLE_PROMETHEUS
                      boost::optional<std::string> prometheus,
#endif
                      boost::optional<bool> paxos_rebalancing_auto_expand,
                      boost::optional<bool> paxos_rebalancing_inspect)
    {
      ELLE_TRACE_SCOPE("run");
      auto& cli = this->cli();
      network_run
        (
         cli,
         network_name,
#ifndef INFINIT_WINDOWS
         daemon,
         monitoring,
#endif
         peer,
         async,
         cache,
         cache_ram_size,
         cache_ram_ttl,
         cache_ram_invalidation,
         cache_disk_size,
         fetch || fetch_endpoints,
         push || push_endpoints,
         publish,
         endpoints_file,
         port_file,
         port,
         peers_file,
         listen,
         fetch_endpoints_interval,
         no_local_endpoints,
         no_public_endpoints,
         advertise_host,
         grpc,
#if INFINIT_ENABLE_PROMETHEUS
         prometheus,
#endif
         paxos_rebalancing_auto_expand,
         paxos_rebalancing_inspect,
         [&] (infinit::User& owner,
              infinit::Network& network,
              dnut::Doughnut& dht,
              bool push)
         {
           auto stat_thread = elle::reactor::Thread::unique_ptr{};
           if (push)
             stat_thread = network.make_stat_update_thread(cli.infinit(), owner, dht);
           cli.report_action("running", "network", network.name);
           if (cli.script())
           {
             auto input = commands_input(commands);
             while (true)
             {
               try
               {
                 auto const json = boost::any_cast<elle::json::Object>(
                   elle::json::read(*input));
                 auto command
                   = elle::serialization::json::SerializerIn(json, false);
                 command.set_context<dnut::Doughnut*>(&dht);
                 auto const op = command.deserialize<std::string>("operation");
                 if (op == "fetch")
                 {
                   auto const address =
                     command.deserialize<infinit::model::Address>("address");
                   auto const block = dht.fetch(address);
                   ELLE_ASSERT(block);
                   Respond
                     ("success", true)
                     ("value", block);
                 }
                 else if (op == "insert" || op == "update")
                 {
                   auto block = command.deserialize<
                     std::unique_ptr<infinit::model::blocks::Block>>("value");
                   if (!block)
                     elle::err("missing field: value");
                   if (op == "insert")
                     dht.insert(std::move(block));
                   else
                     dht.update(std::move(block));
                   Respond("success", true);
                 }
                 else if (op == "write_immutable")
                 {
                   auto block = dht.make_block<infinit::model::blocks::ImmutableBlock>(
                     elle::Buffer(command.deserialize<std::string>("data")));
                   auto const addr = block->address();
                   dht.insert(std::move(block));
                   Respond
                     ("success", true)
                     ("address", addr);
                 }
                 else if (op == "read")
                 {
                   auto const block = dht.fetch(
                       command.deserialize<infinit::model::Address>("address"));
                   int const version = [&]{
                     if (auto mb =
                         dynamic_cast<infinit::model::blocks::MutableBlock*>(block.get()))
                       return mb->version();
                     else
                       return -1;
                   }();
                   Respond
                      ("success", true)
                      ("data", block->data().string())
                      ("version", version);
                 }
                 else if (op == "update_mutable")
                 {
                   auto const addr = command.deserialize<infinit::model::Address>("address");
                   auto const version = command.deserialize<int>("version");
                   auto const data = command.deserialize<std::string>("data");
                   auto block = dht.fetch(addr);
                   auto& mb = dynamic_cast<infinit::model::blocks::MutableBlock&>(*block);
                   if (mb.version() >= version)
                     elle::err("Current version is %s", mb.version());
                   mb.data(elle::Buffer(data));
                   dht.update(std::move(block));
                   Respond("success", true);
                 }
                 else if (op == "resolve_named")
                 {
                   auto const name = command.deserialize<std::string>("name");
                   bool const create = command.deserialize<bool>("create_if_missing");
                   auto const addr = dnut::NB::address(*dht.owner(),
                     name, dht.version());
                   auto res = [&] {
                     try
                     {
                       auto const block = dht.fetch(addr);
                       auto& nb = dynamic_cast<dnut::NB&>(*block);
                       return infinit::model::Address::from_string(nb.data().string());
                     }
                     catch (infinit::model::MissingBlock const& mb)
                     {
                       if (!create)
                         elle::err("NB %s does not exist", name);
                       auto ab = dht.make_block<infinit::model::blocks::ACLBlock>();
                       auto const addr = ab->address();
                       dht.insert(std::move(ab));
                       auto nb = dnut::NB(dht, name, elle::sprintf("%s", addr));
                       dht.seal_and_insert(nb);
                       return addr;
                     }
                   }();
                   Respond
                     ("success", true)
                     ("address", res);
                 }
                 else if (op == "remove")
                 {
                   auto const addr = command.deserialize<infinit::model::Address>("address");
                   dht.remove(addr);
                   Respond("success", true);
                 }
                 else
                   elle::err("invalid operation: %s", op);
               }
               catch (elle::Error const& e)
               {
                 if (input->eof())
                   return;
                 Respond
                   ("success", false)
                   ("message", e.what());
               }
             }
           }
           else
             elle::reactor::sleep();
         });
    }


    /*-------------.
    | Mode: stat.  |
    `-------------*/

    void
    Network::mode_stat(std::string const& network_name)
    {
      ELLE_TRACE_SCOPE("stat");
      auto& cli = this->cli();
      auto& ifnt = cli.infinit();
      auto owner = cli.as_user();
      auto name = ifnt.qualified_name(network_name, owner);
      auto res =
        ifnt.beyond_fetch<infinit::Storages>(
          elle::sprintf("networks/%s/stat", name),
          "stat",
          "stat",
          boost::none,
          infinit::Headers());

      // FIXME: write Storages::operator(std::ostream&)
      elle::fprintf(std::cout, "{\"usage\": %s, \"capacity\": %s}",
                    res.usage, res.capacity);
    }


    /*---------------.
    | Mode: unlink.  |
    `---------------*/

    void
    Network::mode_unlink(std::string const& network_name)
    {
      ELLE_TRACE_SCOPE("unlink");
      auto& cli = this->cli();
      auto& ifnt = cli.infinit();
      auto owner = cli.as_user();
      auto network = ifnt.network_get(network_name, owner, true);
      if (!network.model)
        elle::err("%s is not linked", network.name);
      boost::optional<std::string> node_id;
      if (auto model = dynamic_cast<dnut::Configuration*>(network.model.get()))
      {
        std::stringstream ss;
        {
          elle::serialization::json::SerializerOut s(ss);
          s.serialize_forward(model->id);
        }
        auto tmp = ss.str();
        boost::trim(tmp);
        boost::erase_all(tmp, "\"");
        node_id = tmp;
      }
      ifnt.network_unlink(network.name, owner);
      if (node_id)
        cli.report("if you relink this network, use \"--node-id %s\"", node_id);
    }


    /*---------------.
    | Mode: update.  |
    `---------------*/

    namespace
    {
      std::pair<elle::cryptography::rsa::PublicKey, bool>
      user_key(infinit::Infinit& ifnt,
               std::string name,
               boost::optional<std::string> const& mountpoint)
      {
        bool is_group = false;
        if (!name.empty() && name[0] == '@')
        {
          is_group = true;
          name = name.substr(1);
        }
        if (!name.empty() && name[0] == '{')
        {
          elle::Buffer buf(name);
          elle::IOStream is(buf.istreambuf());
          auto key = elle::serialization::json::deserialize
            <elle::cryptography::rsa::PublicKey>(is);
          return std::make_pair(key, is_group);
        }
        if (!is_group)
          return std::make_pair(ifnt.user_get(name).public_key, false);
        if (!mountpoint)
          elle::err("A mountpoint is required to fetch groups.");
        char buf[32768];
        int res = get_xattr(*mountpoint,
                            "infinit.group.control_key." + name,
                            buf, 16384, true);
        if (res <= 0)
          elle::err("Unable to fetch group %s", name);
        auto b = elle::Buffer(buf, res);
        elle::IOStream is(b.istreambuf());
        auto key = elle::serialization::json::deserialize
          <elle::cryptography::rsa::PublicKey>(is);
        return std::make_pair(key, is_group);
      }
    }

    void
    Network::mode_update(
      std::string const& network_name,
      boost::optional<std::string> const& description,
      boost::optional<int> port,
      boost::optional<std::string> const& output_name,
      bool push_network,
      bool push,
      Strings const& admin_r,
      Strings const& admin_rw,
      Strings const& admin_remove,
      boost::optional<std::string> const& mountpoint,
      Strings const& peer,
      boost::optional<std::string> const& protocol)
    {
      ELLE_TRACE_SCOPE("create");
      auto& cli = this->cli();
      auto& ifnt = cli.infinit();
      auto owner = cli.as_user();
      auto network = ifnt.network_get(network_name, owner);
      if (description)
        network.description = description;
      network.ensure_allowed(owner, "update");
      auto& dht = *network.dht();
      if (port)
        dht.port = *port;
      if (cli.compatibility_version())
        dht.version = *cli.compatibility_version();
      bool changed_admins = false;
      auto check_group_mount = [&] (bool group)
        {
          if (group && !mountpoint)
            elle::err<CLIError>("must specify mountpoint of volume on "
                                "network \"%s\" to edit group admins",
                                network.name);
        };
      auto add_admin = [&keys = dht.admin_keys, &changed_admins]
        (elle::cryptography::rsa::PublicKey const& key,
         bool group, bool read, bool write)
        {
          if (read && !write)
            push_back_if_missing(group ? keys.group_r : keys.r, key);
          if (write) // Implies RW.
            push_back_if_missing(group ? keys.group_w : keys.w, key);
          changed_admins = true;
        };
      for (auto const& u: admin_r)
      {
        auto const r = user_key(ifnt, u, mountpoint);
        check_group_mount(r.second);
        add_admin(r.first, r.second, true, false);
      }
      for (auto const& u: admin_rw)
      {
        auto const r = user_key(ifnt, u, mountpoint);
        check_group_mount(r.second);
        add_admin(r.first, r.second, true, true);
      }
      for (auto const& u: admin_remove)
      {
        auto const r = user_key(ifnt, u, mountpoint);
        check_group_mount(r.second);
        auto del = [&r](auto& cont)
          {
            boost::remove_erase(cont, r.first);
          };
        del(dht.admin_keys.r);
        del(dht.admin_keys.w);
        del(dht.admin_keys.group_r);
        del(dht.admin_keys.group_w);
        changed_admins = true;
      }
      if (!peer.empty())
        dht.peers = parse_peers(peer);
      if (protocol)
        dht.overlay->rpc_protocol = protocol_get(protocol);
      if (output_name)
      {
        auto output = cli.get_output(output_name);
        elle::serialization::json::serialize(network, *output, false);
      }
      else
        ifnt.network_save(owner, network, true);
      auto desc = std::make_unique<infinit::NetworkDescriptor>(std::move(network));
      if (push || push_network)
        ifnt.beyond_push("network", desc->name, *desc, owner, false, true);
      if (changed_admins && !output_name)
        std::cout << "INFO: Changes to network admins do not affect existing data:\n"
                  << "INFO: Admin access will be updated on the next write to each\n"
                  << "INFO: file or folder.\n";
    }
  }
}
