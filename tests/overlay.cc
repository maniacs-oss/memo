#include <boost/range/algorithm/count_if.hpp>

#include <elle/test.hh>

#include <elle/err.hh>

#include <reactor/network/udp-socket.hh>

#include <infinit/model/MissingBlock.hh>
#include <infinit/model/blocks/MutableBlock.hh>
#include <infinit/model/doughnut/ACB.hh>
#include <infinit/overlay/kelips/Kelips.hh>
#include <infinit/overlay/kouncil/Kouncil.hh>
#include <infinit/storage/MissingKey.hh>

#include "DHT.hh"

ELLE_LOG_COMPONENT("infinit.overlay.test");

using namespace infinit::model;
using namespace infinit::model::blocks;
using namespace infinit::model::doughnut;
using namespace infinit::overlay;

/// Make several attempts to run `f` before giving up.
template <typename Fun>
auto
persist(Fun f)
{
  auto num = 20;
  for (int i=0; i<num; ++i)
  {
    try
    {
      return f();
    }
    catch (elle::Error const&)
    {
      reactor::sleep(100_ms);
    }
  }
  elle::err("persist: failed after %s attempts", num);
}

class TestConflictResolver
  : public DummyConflictResolver
{};

inline std::unique_ptr<ConflictResolver>
tcr()
{
  return std::make_unique<TestConflictResolver>();
}

class UTPInstrument
{
public:
  UTPInstrument(Endpoint endpoint)
    : server()
    , _endpoint(std::move(endpoint))
    , _thread(new reactor::Thread(elle::sprintf("%s server", this),
                                  std::bind(&UTPInstrument::_serve, this)))
  {
    this->server.bind({});
    this->_transmission.open();
  }

  reactor::network::UDPSocket server;
  ELLE_ATTRIBUTE_RX(reactor::Barrier, transmission);

private:
  ELLE_LOG_COMPONENT("infinit.overlay.test.UTPInstrument");

  ELLE_ATTRIBUTE(Endpoint, endpoint);
  ELLE_ATTRIBUTE(Endpoint, client_endpoint);
  void
  _serve()
  {
    char buf[10000];
    while (true)
    {
      try
      {
        reactor::network::UDPSocket::EndPoint ep;
        auto sz = server.receive_from(elle::WeakBuffer(buf), ep);
        reactor::wait(_transmission);
        if (ep.port() != _endpoint.port())
        {
          _client_endpoint = ep;
          server.send_to(elle::ConstWeakBuffer(buf, sz), _endpoint.udp());
        }
        else
          server.send_to(elle::ConstWeakBuffer(buf, sz), _client_endpoint.udp());
      }
      catch (reactor::network::Exception const& e)
      {
        ELLE_LOG("ignoring exception %s", e);
      }
    }
  }

  ELLE_ATTRIBUTE(reactor::Thread::unique_ptr, thread);
};

void
discover(DHT& dht, DHT& target, bool anonymous, bool onlyfirst=false)
{
  Endpoints eps;
  if (onlyfirst)
    eps = Endpoints {target.dht->local()->server_endpoints()[0]};
  else
    eps = target.dht->local()->server_endpoints();
  if (anonymous)
    dht.dht->overlay()->discover(eps);
  else
    dht.dht->overlay()->discover(
      NodeLocation(target.dht->id(), eps));
}

static
std::vector<infinit::model::Address>
peers(DHT& client)
{
  std::vector<infinit::model::Address> res;
  auto stats = client.dht->overlay()->query("stats", {});
  auto ostats = boost::any_cast<elle::json::Object>(stats);
  try
  {
    auto cts = boost::any_cast<elle::json::Array>(ostats["contacts"]);
    ELLE_DUMP("%s", elle::json::pretty_print(ostats["contacts"]));
    for (auto& c: cts)
      res.push_back(infinit::model::Address::from_string(
        boost::any_cast<std::string>(
          boost::any_cast<elle::json::Object>(c).at("id"))));
  }
  catch (boost::bad_any_cast const&)
  {
    auto cts = boost::any_cast<elle::json::Array>(ostats["peers"]);
    ELLE_DUMP("%s", elle::json::pretty_print(ostats["peers"]));
    for (auto& c: cts)
      res.push_back(infinit::model::Address::from_string(
        boost::any_cast<std::string>(
          boost::any_cast<elle::json::Object>(c).at("id"))));
  }
  return res;
}

static
int
peer_count(DHT& client, bool discovered = false)
{
  auto stats = client.dht->overlay()->query("stats", {});
  int res = -1;
  auto ostats = boost::any_cast<elle::json::Object>(stats);
  try
  {
    auto cts = boost::any_cast<elle::json::Array>(ostats["contacts"]);
    ELLE_DEBUG("%s", elle::json::pretty_print(ostats["contacts"]));
    int count = 0;
    ELLE_TRACE("checking %s candidates", cts.size());
    for (auto& c: cts)
    {
      if (!discovered || boost::any_cast<bool>(
        boost::any_cast<elle::json::Object>(c).at("discovered")))
        ++count;
    }
    res = count;
  }
  catch (boost::bad_any_cast const&)
  {
    res = boost::any_cast<elle::json::Array>(ostats["peers"]).size();
  }
  ELLE_TRACE("counted %s peers for %s", res, client.dht);
  return res;
}

static
void
kouncil_wait_pasv(DHT& s, int n_servers)
{
  while (true)
  {
    std::vector<infinit::model::Address> res;
    auto stats = s.dht->overlay()->query("stats", {});
    auto ostats = boost::any_cast<elle::json::Object>(stats);
    auto cts = boost::any_cast<elle::json::Array>(ostats["peers"]);
    ELLE_DEBUG("%s", elle::json::pretty_print(ostats["peers"]));
    for (auto& c: cts)
    {
      if (boost::any_cast<bool>(
          boost::any_cast<elle::json::Object>(c).at("connected")))
        res.push_back(infinit::model::Address::from_string(
          boost::any_cast<std::string>(
            boost::any_cast<elle::json::Object>(c).at("id"))));
    }

    if (res.size() >= unsigned(n_servers))
      return;
    ELLE_TRACE("%s/%s", res.size(), n_servers);
    reactor::sleep(50_ms);
  }
}

// Wait until s sees n_servers and can make RPC calls to all of them
// If or_more is true, accept extra non-working peers
static
void
hard_wait(DHT& s, int n_servers,
  infinit::model::Address client = infinit::model::Address::null,
  bool or_more = false,
  infinit::model::Address blacklist = infinit::model::Address::null)
{
  int attempts = 0;
  while (true)
  {
    if (++attempts > 50 && !(attempts % 40))
    {
      auto stats = s.dht->overlay()->query("stats", boost::none);
      std::cerr << elle::json::pretty_print(stats) << std::endl;
    }
    bool ok = true;
    auto peers = ::peers(s);
    int hit = 0;
    if (peers.size() >= unsigned(n_servers))
    {
      for (auto const& pa: peers)
      {
        if (pa == client || pa == blacklist)
          continue;
        try
        {
          auto p = s.dht->overlay()->lookup_node(pa);
          p.lock()->fetch(infinit::model::Address::random(), boost::none);
        }
        catch (infinit::storage::MissingKey const& mb)
        { // FIXME why do we need this?
          ++hit;
        }
        catch (infinit::model::MissingBlock const& mb)
        {
          ++hit;
        }
        catch (elle::Error const& e)
        {
          ELLE_TRACE("hard_wait %f: %s", pa, e);
          if (!or_more)
            ok = false;
        }
      }
    }
    if ( (hit == n_servers || (or_more && hit >n_servers)) && ok)
      break;
    reactor::sleep(50_ms);
  }
  ELLE_DEBUG("hard_wait exiting");
}

static
void
wait_until_ready(DHT& client, int n_servers = 0)
{
  persist([&client]
    {
      auto block = client.dht->make_block<ACLBlock>(std::string("block"));
      client.dht->store(std::move(block), STORE_INSERT, tcr());
    });

  if (n_servers)
  {
    while (true)
    {
      int pc = peer_count(client);
      if (pc == n_servers)
        break;
      else
        ELLE_LOG("%s/%s", pc, n_servers);
      reactor::sleep(100_ms);
    }
  }
}

ELLE_TEST_SCHEDULED(
  basics, (Doughnut::OverlayBuilder, builder), (bool, anonymous))
{
  auto keys = infinit::cryptography::rsa::keypair::generate(512);
  auto storage = infinit::storage::Memory::Blocks();
  auto id = infinit::model::Address::random();
  auto make_dht_a = [&]
    {
      return DHT(
        ::id = id,
        ::keys = keys,
        ::storage = std::make_unique<infinit::storage::Memory>(storage),
        make_overlay = builder);
    };
  auto disk = [&]
    {
      auto dht = make_dht_a();
      auto b = dht.dht->make_block<MutableBlock>(std::string("disk"));
      dht.dht->store(*b, STORE_INSERT, tcr());
      return b;
    }();
  auto dht_a = make_dht_a();
  auto before = dht_a.dht->make_block<MutableBlock>(std::string("before"));
  dht_a.dht->store(*before, STORE_INSERT, tcr());
  DHT dht_b(
    ::keys = keys, make_overlay = builder, ::storage = nullptr);
  if (anonymous)
    dht_b.dht->overlay()->discover(dht_a.dht->local()->server_endpoints());
  else
    dht_b.dht->overlay()->discover(
      NodeLocation(dht_a.dht->id(), dht_a.dht->local()->server_endpoints()));
  hard_wait(dht_b, 1);
  auto after = dht_a.dht->make_block<MutableBlock>(std::string("after"));
  dht_a.dht->store(*after, STORE_INSERT, tcr());
  ELLE_LOG("check non-existent block")
    BOOST_CHECK_THROW(
      dht_b.dht->overlay()->lookup(Address::random()),
      MissingBlock);
  ELLE_LOG("check block loaded from disk")
    BOOST_CHECK_EQUAL(
      dht_b.dht->overlay()->lookup(disk->address()).lock()->id(),
      dht_a.dht->id());
  ELLE_LOG("check block present before connection")
    BOOST_CHECK_EQUAL(
      dht_b.dht->overlay()->lookup(before->address()).lock()->id(),
      dht_a.dht->id());
  ELLE_LOG("check block inserted after connection")
    BOOST_CHECK_EQUAL(
      dht_b.dht->overlay()->lookup(after->address()).lock()->id(),
      dht_a.dht->id());
}

ELLE_TEST_SCHEDULED(
  dead_peer, (Doughnut::OverlayBuilder, builder), (bool, anonymous))
{
  auto keys = infinit::cryptography::rsa::keypair::generate(512);
  auto dht_a = DHT(::keys = keys, make_overlay = builder, paxos = false);
  elle::With<UTPInstrument>(
    Endpoint("127.0.0.1",
             dht_a.dht->local()->server_endpoints()[0].port())) <<
    [&] (UTPInstrument& instrument)
    {
      auto dht_b = DHT(::keys = keys,
                       make_overlay = builder,
                       paxos = false,
                       ::storage = nullptr);
      infinit::model::Endpoints ep = {
        Endpoint("127.0.0.1", instrument.server.local_endpoint().port()),
      };
      if (anonymous)
        dht_b.dht->overlay()->discover(ep);
      else
        dht_b.dht->overlay()->discover(NodeLocation(dht_a.dht->id(), ep));
      // Ensure one request can go through.
      {
        auto block = dht_a.dht->make_block<MutableBlock>(std::string("block"));
        ELLE_LOG("store block")
          dht_a.dht->store(*block, STORE_INSERT, tcr());
        ELLE_LOG("lookup block")
        {
          persist([&] {
            dht_b.dht->overlay()->lookup(block->address()).lock();
          });
          BOOST_CHECK_EQUAL(
            dht_b.dht->overlay()->lookup(block->address()).lock()->id(),
            dht_a.dht->id());
        }
      }
      // Partition peer
      instrument.transmission().close();
      // Ensure we don't deadlock
      {
        auto block = dht_a.dht->make_block<MutableBlock>(std::string("block"));
        ELLE_LOG("store block")
          dht_a.dht->store(*block, STORE_INSERT, tcr());
    }
  };
}

ELLE_TEST_SCHEDULED(
  discover_endpoints, (Doughnut::OverlayBuilder, builder), (bool, anonymous))
{
  auto keys = infinit::cryptography::rsa::keypair::generate(512);
  auto id_a = infinit::model::Address::random();
  auto dht_a = std::make_unique<DHT>(
    ::id = id_a, ::keys = keys, make_overlay = builder, paxos = false);
  Address old_address;
  ELLE_LOG("store first block")
  {
    auto block = dht_a->dht->make_block<MutableBlock>(std::string("block"));
    dht_a->dht->store(*block, STORE_INSERT, tcr());
    old_address = block->address();
  }
  DHT dht_b(
    ::keys = keys, make_overlay = builder, ::storage = nullptr);
  discover(dht_b, *dht_a, anonymous);
  hard_wait(dht_b, 1, dht_b.dht->id());
  ELLE_LOG("lookup block")
  {
    persist([&] {
        dht_b.dht->overlay()->lookup(old_address).lock();
    });
    BOOST_CHECK_EQUAL(
        dht_b.dht->overlay()->lookup(old_address).lock()->id(),
        id_a);
  }
  ELLE_LOG("restart first DHT")
  {
    dht_a.reset();
    dht_a = std::make_unique<DHT>(
      ::id = id_a, ::keys = keys, make_overlay = builder, paxos = false);
  }
  Address new_address;
  ELLE_LOG("store second block")
  {
    auto block = dht_a->dht->make_block<MutableBlock>(std::string("nblock"));
    dht_a->dht->store(*block, STORE_INSERT, tcr());
    new_address = block->address();
  }
  ELLE_LOG("lookup second block")
    BOOST_CHECK_THROW(
      dht_b.dht->overlay()->lookup(new_address).lock()->id(),
      MissingBlock);
  ELLE_LOG("discover new endpoints")
    discover(dht_b, *dht_a, anonymous);
  hard_wait(dht_b, 1, dht_b.dht->id());
  ELLE_LOG("lookup second block")
  ELLE_LOG("lookup block")
  {
    persist([&] {
        dht_b.dht->overlay()->lookup(new_address).lock();
    });
    BOOST_CHECK_EQUAL(
      dht_b.dht->overlay()->lookup(new_address).lock()->id(),
      id_a);
  }
}

ELLE_TEST_SCHEDULED(
  key_cache_invalidation, (Doughnut::OverlayBuilder, builder), (bool, anonymous))
{
  infinit::storage::Memory::Blocks blocks;
  std::unique_ptr<infinit::storage::Storage> s(new infinit::storage::Memory(blocks));
  auto keys = infinit::cryptography::rsa::keypair::generate(512);
  auto id_a = infinit::model::Address::random();
  auto dht_a = std::make_unique<DHT>(
    ::id = id_a, ::keys = keys, make_overlay = builder, paxos = false,
    ::protocol = infinit::model::doughnut::Protocol::utp,
    ::storage = std::move(s));
  int port = dht_a->dht->local()->server_endpoints()[0].port();
  DHT dht_b(
    ::keys = keys, make_overlay = builder, ::storage = nullptr,
    ::paxos = false,
    ::protocol = infinit::model::doughnut::Protocol::utp);
  discover(dht_b, *dht_a, anonymous);
  hard_wait(dht_b, 1, dht_b.dht->id());
  auto block = dht_a->dht->make_block<ACLBlock>(std::string("block"));
  auto& acb = dynamic_cast<infinit::model::doughnut::ACB&>(*block);
  acb.set_permissions(infinit::cryptography::rsa::keypair::generate(512).K(),
    true, true);
  acb.set_permissions(infinit::cryptography::rsa::keypair::generate(512).K(),
    true, true);
  dht_a->dht->store(*block, STORE_INSERT, tcr());
  auto b2 = dht_b.dht->fetch(block->address());
  dynamic_cast<MutableBlock*>(b2.get())->data(elle::Buffer("foo"));
  dht_b.dht->store(*b2, STORE_UPDATE, tcr());
  // brutal restart of a
  ELLE_LOG("disconnect A");
  dht_a->dht->local()->utp_server()->socket()->close();
  s.reset(new infinit::storage::Memory(blocks));
  ELLE_LOG("recreate A");
  auto dht_aa = std::make_unique<DHT>(
    ::id = id_a, ::keys = keys, make_overlay = builder, paxos = false,
    ::protocol = infinit::model::doughnut::Protocol::utp,
    ::storage = std::move(s),
    ::port = port);
  // rebind local somewhere else or we get EBADF from local_endpoint
  dht_a->dht->local()->utp_server()->socket()->bind(
    boost::asio::ip::udp::endpoint(
      boost::asio::ip::address::from_string("127.0.0.1"), 0));
  // reconnect the Remote
  ELLE_LOG("reconnect to A");
  auto& peer = dht_b.dht->dock().peer_cache().begin()->second;
  ELLE_LOG("peer is %s", *peer);
  // force a reconnect
  dynamic_cast<infinit::model::doughnut::Remote&>(*peer).reconnect();
  // wait for it
  dynamic_cast<infinit::model::doughnut::Remote&>(*peer).connect();
  ELLE_LOG("re-store block");
  dynamic_cast<MutableBlock*>(b2.get())->data(elle::Buffer("foo"));
  BOOST_CHECK_NO_THROW(dht_b.dht->store(*b2, STORE_UPDATE, tcr()));
  ELLE_LOG("test end");
}

ELLE_TEST_SCHEDULED(
  data_spread, (Doughnut::OverlayBuilder, builder), (bool, anonymous), (bool, pax))
{
  infinit::storage::Memory::Blocks b1, b2;
  std::unique_ptr<infinit::storage::Storage> s1(new infinit::storage::Memory(b1));
  std::unique_ptr<infinit::storage::Storage> s2(new infinit::storage::Memory(b2));
  auto keys = infinit::cryptography::rsa::keypair::generate(512);
  auto id_a = infinit::model::Address::random();
  auto dht_a = std::make_unique<DHT>(
    ::id = id_a, ::keys = keys, make_overlay = builder, paxos = pax,
    dht::consensus::rebalance_auto_expand = false,
    ::storage = std::move(s1));
  auto dht_b = std::make_unique<DHT>(
    ::keys = keys, make_overlay = builder,
    dht::consensus::rebalance_auto_expand = false,
    ::paxos = pax,
    ::storage = std::move(s2));
  discover(*dht_b, *dht_a, anonymous);

  // client. Hard-coded replication_factor=3 if paxos is enabled
  auto client = std::make_unique<DHT>(
    ::keys = keys, make_overlay = builder,
    ::paxos = pax,
    ::storage = nullptr);
  discover(*client, *dht_a, anonymous);
  discover(*client, *dht_b, anonymous);
  hard_wait(*client, 2, client->dht->id());
  std::vector<infinit::model::Address> addrs;
  for (int a=0; a<10; ++a)
  {
    for (int i=0; i<50; ++i)
    {
      auto block = dht_a->dht->make_block<ACLBlock>(std::string("block"));
      addrs.push_back(block->address());
    client->dht->store(std::move(block), STORE_INSERT, tcr());
    }
    if (b1.size() >=5 && b2.size() >=5)
      break;
  }
  ELLE_LOG("stores: %s %s", b1.size(), b2.size());
  BOOST_CHECK_GE(b1.size(), 5);
  BOOST_CHECK_GE(b2.size(), 5);
  for (auto const& a: addrs)
    client->dht->fetch(a);
}

ELLE_TEST_SCHEDULED(
  chain_connect, (Doughnut::OverlayBuilder, builder), (bool, anonymous), (bool, pax))
{
  infinit::storage::Memory::Blocks b1, b2, b3;
  std::unique_ptr<infinit::storage::Storage> s1(new infinit::storage::Memory(b1));
  std::unique_ptr<infinit::storage::Storage> s2(new infinit::storage::Memory(b2));
  std::unique_ptr<infinit::storage::Storage> s3(new infinit::storage::Memory(b3));
  auto keys = infinit::cryptography::rsa::keypair::generate(512);
  auto id_a = infinit::model::Address::random();
  auto dht_a = std::make_unique<DHT>(
    ::id = id_a, ::keys = keys, make_overlay = builder, paxos = pax,
    dht::consensus::rebalance_auto_expand = false,
    ::storage = std::move(s1));
  auto dht_b = std::make_unique<DHT>(
    ::keys = keys, make_overlay = builder,
    dht::consensus::rebalance_auto_expand = false,
    ::paxos = pax,
    ::storage = std::move(s2));
  auto dht_c = std::make_unique<DHT>(
    ::keys = keys, make_overlay = builder,
    dht::consensus::rebalance_auto_expand = false,
    ::paxos = pax,
    ::storage = std::move(s3));
  discover(*dht_b, *dht_a, anonymous);
  discover(*dht_c, *dht_b, anonymous);
  unsigned int pa=0, pb=0, pc=0;
  for (auto tgt: std::vector<DHT*>{dht_a.get(), dht_b.get(), dht_c.get()})
  {
    auto client = std::make_unique<DHT>(
      ::keys = keys, make_overlay = builder,
      ::paxos = pax,
      ::storage = nullptr);
    discover(*client, *tgt, anonymous);
    // writes will fail until it connects
    wait_until_ready(*client);
    std::vector<infinit::model::Address> addrs;
    for (int a=0; a<10; ++a)
    {
      try
      {
        for (int i=0; i<50; ++i)
        {
          auto block = client->dht->make_block<ACLBlock>(std::string("block"));
          addrs.push_back(block->address());
          client->dht->store(std::move(block), STORE_INSERT, tcr());
        }
      }
      catch (elle::Error const& e)
      {
        ELLE_ERR("Exception storing blocks: %s", e);
        throw;
      }
      if (b1.size()-pa >= 5 && b2.size()-pb >=5 && b3.size()-pc >=5)
        break;
    }
    ELLE_LOG("stores: %s %s %s", b1.size(), b2.size(), b3.size());
    BOOST_CHECK_GE(b1.size()-pa, 5);
    BOOST_CHECK_GE(b2.size()-pb, 5);
    BOOST_CHECK_GE(b3.size()-pc, 5);
    pa = b1.size();
    pb = b2.size();
    pc = b3.size();
    for (auto const& a: addrs)
      client->dht->fetch(a);
    ELLE_LOG("teardown client");
  }
  ELLE_LOG("teardown");
}


ELLE_TEST_SCHEDULED(
  data_spread2, (Doughnut::OverlayBuilder, builder), (bool, anonymous), (bool, pax))
{
  infinit::storage::Memory::Blocks b1, b2;
  std::unique_ptr<infinit::storage::Storage> s1(new infinit::storage::Memory(b1));
  std::unique_ptr<infinit::storage::Storage> s2(new infinit::storage::Memory(b2));
  auto keys = infinit::cryptography::rsa::keypair::generate(512);
  auto id_a = infinit::model::Address::random();
  auto dht_a = std::make_unique<DHT>(
    ::id = id_a, ::keys = keys, make_overlay = builder, paxos = pax,
    dht::consensus::rebalance_auto_expand = false,
    ::storage = std::move(s1));
  auto dht_b = std::make_unique<DHT>(
    ::keys = keys, make_overlay = builder,
    dht::consensus::rebalance_auto_expand = false,
    ::paxos = pax,
    ::storage = std::move(s2));

  auto client = std::make_unique<DHT>(
    ::keys = keys, make_overlay = builder,
    ::paxos = pax,
    ::storage = nullptr);
  discover(*client, *dht_a, anonymous);
  discover(*dht_a, *dht_b, anonymous);
  hard_wait(*client, 2, client->dht->id());
  std::vector<infinit::model::Address> addrs;
  for (int a=0; a<10; ++a)
  {
    for (int i=0; i<50; ++i)
    {
      auto block = dht_a->dht->make_block<ACLBlock>(std::string("block"));
      addrs.push_back(block->address());
    client->dht->store(std::move(block), STORE_INSERT, tcr());
    }
    if (b1.size() >= 5 && b2.size() >= 5)
      break;
  }
  ELLE_LOG("stores: %s %s", b1.size(), b2.size());
  BOOST_CHECK_GE(b1.size(), 5);
  BOOST_CHECK_GE(b2.size(), 5);
  for (auto const& a: addrs)
    client->dht->fetch(a);
  ELLE_LOG("teardown");
}

ELLE_TEST_SCHEDULED(
  storm, (Doughnut::OverlayBuilder, builder),
  (bool, pax), (int, nservers), (int, nclients), (int, nactions))
{
  auto keys = infinit::cryptography::rsa::keypair::generate(512);
  bool is_kelips = false;
  // Set servers up.
  auto servers = std::vector<std::unique_ptr<DHT>>{};
  for (int i=0; i<nservers; ++i)
  {
    auto dht = std::make_unique<DHT>(
      ::keys = keys, make_overlay = builder, paxos = pax,
      dht::consensus::rebalance_auto_expand = false
    );
    if (auto kelips = dynamic_cast<infinit::overlay::kelips::Node*>(
      dht->dht->overlay().get()))
    {
      is_kelips = true;
      kelips->config().query_put_retries = 6;
      kelips->config().query_timeout_ms = valgrind(2000, 4);
      kelips->config().contact_timeout_ms = valgrind(100000,20);
      kelips->config().ping_interval_ms = valgrind(500, 10);
      kelips->config().ping_timeout_ms = valgrind(2000, 20);
    }
    servers.emplace_back(std::move(dht));
  }
  if (is_kelips)
  {
    for (int i=1; i<nservers; ++i)
      discover(*servers[i], *servers[0], false, true);
  }
  else
    for (int i=1; i<nservers; ++i)
    {
      for (int j=0; j<i; ++j)
        discover(*servers[i], *servers[j], false, true);
      hard_wait(*servers[i], i);
    }
  ELLE_LOG("waiting for servers");
  for (int i=0; i<nservers; ++i)
    hard_wait(*servers[i], nservers-1);
  ELLE_LOG("clients");
  // Set clients up.
  auto clients = std::vector<std::unique_ptr<DHT>>{};
  for (int i=0; i<nclients; ++i)
  {
    auto dht = std::make_unique<DHT>(
      ::keys = keys, make_overlay = builder, paxos = pax, ::storage = nullptr,
      dht::consensus::rebalance_auto_expand = false
    );
    if (auto kelips = dynamic_cast<infinit::overlay::kelips::Node*>(
      dht->dht->overlay().get()))
    {
      kelips->config().query_put_retries = 6;
      kelips->config().query_get_retries = 20;
      kelips->config().query_timeout_ms = valgrind(2000, 4);
      kelips->config().contact_timeout_ms = valgrind(100000,20);
      kelips->config().ping_interval_ms = valgrind(500, 10);
      kelips->config().ping_timeout_ms = valgrind(2000, 20);
    }
    clients.emplace_back(std::move(dht));
  }
  for (int i=0; i<nservers; ++i)
  {
    for (int j=0; j<nclients; ++j)
    {
      discover(*clients[j], *servers[i], false, true);
      reactor::yield(); reactor::yield(); reactor::yield();
    }
  }
  for (int i=0; i<nclients; ++i)
    hard_wait(*clients[i], nservers);

  auto addrs = std::vector<infinit::model::Address>{};
  elle::With<reactor::Scope>() << [&](reactor::Scope& s)
  {
    for (auto& c: clients)
      s.run_background(elle::sprintf("storm %s", c), [&] {
        try
        {
          for (int i=0; i<nactions; ++i)
          {
            int r = rand()%100;
            ELLE_TRACE_SCOPE("action %s: %s, with %s addrs",
              i, r, addrs.size());
            if (r < 20 && !addrs.empty())
            { // delete
              int p = rand()%addrs.size();
              auto addr = addrs[p];
              ELLE_DEBUG("deleting %f", addr);
              std::swap(addrs[p], addrs[addrs.size()-1]);
              addrs.pop_back();
              try
              {
                c->dht->remove(addr);
              }
              catch (infinit::model::MissingBlock const& mb)
              {
              }
              catch (athena::paxos::TooFewPeers const& tfp)
              {
              }
              ELLE_DEBUG("deleted %f", addr);
            }
            else if (r < 50 || addrs.empty())
            { // create
              ELLE_DEBUG("creating");
              auto block = c->dht->make_block<ACLBlock>(std::string("block"));
              auto a = block->address();
              try
              {
                c->dht->store(std::move(block), STORE_INSERT, tcr());
              }
              catch (elle::Error const& e)
              {
                ELLE_ERR("insertiong of %s failed with %s", a, e);
                throw;
              }
              ELLE_DEBUG("created %f", a);
              addrs.push_back(a);
            }
            else
            { // read
              int p = rand()%addrs.size();
              auto addr = addrs[p];
              ELLE_DEBUG_SCOPE("reading %f", addr);
              std::exception_ptr except;
              try
              {
                auto block = c->dht->fetch(addr);
                if (r < 80)
                {
                  // Update.
                  ELLE_DEBUG("updating %f", addr);
                  auto aclb
                    = dynamic_cast<infinit::model::blocks::ACLBlock*>(block.get());
                  aclb->data(elle::Buffer("coincoin"));
                  try
                  {
                    c->dht->store(std::move(block), STORE_UPDATE, tcr());
                  }
                  catch (elle::Error const& e)
                  {
                    ELLE_ERR("update of %s failed with %s", addr, e);
                    throw;
                  }
                  ELLE_DEBUG("updated %f", addr);
                }
              }
              catch (infinit::model::MissingBlock const& mb)
              {
                except = std::current_exception();
              }
              catch (athena::paxos::TooFewPeers const& tfp)
              {
                except = std::current_exception();
              }
              if (except)
              {
                // This can be legit if a delete crossed our path
                if (std::find(addrs.begin(), addrs.end(), addr)
                    != addrs.end())
                {
                  ELLE_ERR("exception on supposedly live block %f: %s",
                           addr, elle::exception_string(except));
                  std::rethrow_exception(except);
                }
              }
              ELLE_DEBUG("read %f", addr);
            }
            ELLE_TRACE("terminated action %s", i);
          }
        }
        catch (elle::Error const& e)
        {
          ELLE_ERR("%s: exception %s at %s", c, e, e.backtrace());
          throw;
        }
      });
    reactor::wait(s);
    ELLE_TRACE("exiting scope");
  };
  ELLE_TRACE("teardown");
}

ELLE_TEST_SCHEDULED(
  paxos_3_1, (Doughnut::OverlayBuilder, builder))
{
  auto keys = infinit::cryptography::rsa::keypair::generate(512);
  auto dht_a = std::make_unique<DHT>(
    ::keys = keys, make_overlay = builder, paxos = true);
  auto dht_b = std::make_unique<DHT>(
    ::keys = keys, make_overlay = builder, paxos = true);
  auto dht_c = std::make_unique<DHT>(
    ::keys = keys, make_overlay = builder, paxos = true);
  discover(*dht_a, *dht_b, false);
  discover(*dht_a, *dht_c, false);
  auto client = std::make_unique<DHT>(
    ::keys = keys, make_overlay = builder,
    ::paxos = true,
    ::storage = nullptr);
  discover(*client, *dht_a, false);
  wait_until_ready(*client);
  std::vector<infinit::model::Address> addrs;
  for (int i=0; i<10; ++i)
  {
    auto block = dht_a->dht->make_block<ACLBlock>(std::string("block"));
    addrs.push_back(block->address());
    client->dht->store(std::move(block), STORE_INSERT, tcr());
  }
  dht_c.reset();
  wait_until_ready(*client);
  // can we read blocks?
  for (auto const& a: addrs)
  {
    client->dht->fetch(a);
  }
  addrs.clear();
  // can we write blocks?
  for (int i=0; i<10; ++i)
  {
    auto block = dht_a->dht->make_block<ACLBlock>(std::string("block"));
    addrs.push_back(block->address());
    client->dht->store(std::move(block), STORE_INSERT, tcr());
  }
  // can we read those?
  for (auto const& a: addrs)
  {
    client->dht->fetch(a);
  }
}

ELLE_TEST_SCHEDULED(
  parallel_discover, (Doughnut::OverlayBuilder, builder), (bool, anonymous))
{
  constexpr auto nservers = 5;
  constexpr auto npeers = nservers - 1;

  auto keys = infinit::cryptography::rsa::keypair::generate(512);
  auto servers = std::vector<std::unique_ptr<DHT>>{};
  for (int i=0; i<nservers; ++i)
  {
    auto dht = std::make_unique<DHT>(
      ::keys = keys, make_overlay = builder, paxos = false);
    servers.emplace_back(std::move(dht));
  }
  elle::With<reactor::Scope>() << [&](reactor::Scope& s)
  {
    for (int i=0; i<rand()%5; ++i)
      reactor::yield();
    for (int i=1; i<nservers; ++i)
      s.run_background("discover", [&,i] {
          discover(*servers[i], *servers[0], anonymous);
      });
    reactor::wait(s);
  };

  // Number of servers that know all their peers.
  auto c = 0;
  // Previously we limit ourselves to 50 attempts.  When run
  // repeatedly, it did happen to fail for lack of time.  Raise the
  // limit to 100 attempts.
  for (auto i = 0; i < 100 && c != nservers; ++i)
  {
    reactor::sleep(100_ms);
    using boost::range::count_if;
    c = count_if(servers,
                 [npeers](auto&& s) { return peer_count(*s) == npeers; });
  }
  BOOST_CHECK_EQUAL(c, nservers);
}

ELLE_TEST_SCHEDULED(churn, (Doughnut::OverlayBuilder, builder),
  (bool, keep_port), (bool, wait_disconnect), (bool, wait_connect))
{
  static const int n = 5;
  auto keys = infinit::cryptography::rsa::keypair::generate(512);
  infinit::model::Address ids[n];
  unsigned short ports[n];
  infinit::storage::Memory::Blocks blocks[n];
  std::vector<std::unique_ptr<DHT>> servers;
  for (int i=0; i<n; ++i)
  {
    ids[i] = infinit::model::Address::random();
    auto dht = elle::make_unique<DHT>(
      ::id = ids[i],
      ::keys = keys, make_overlay = builder, paxos = true,
      ::storage = elle::make_unique<infinit::storage::Memory>(blocks[i])
    );
    ports[i] = dht->dht->dock().utp_server().local_endpoint().port();
    servers.emplace_back(std::move(dht));
  }
  for (int i=0; i<n; ++i)
    for (int j=i+1; j<n; ++j)
      discover(*servers[i], *servers[j], false);
  std::unique_ptr<DHT> client;
  auto spawn_client = [&] {
    client = elle::make_unique<DHT>(
      ::keys = keys, make_overlay = builder, paxos = true, ::storage = nullptr);
    if (auto kelips = dynamic_cast<infinit::overlay::kelips::Node*>(
      client->dht->overlay().get()))
    {
      kelips->config().query_put_retries = 6;
      kelips->config().query_timeout_ms = valgrind(1000, 4);
    }
    discover(*client, servers[0] ? *servers[0] : *servers[1], false);
  };
  spawn_client();
  for (auto& s: servers)
    hard_wait(*s, n-1, client->dht->id());
  hard_wait(*client, n, client->dht->id());

  std::vector<infinit::model::Address> addrs;
  int down = -1;
  try
  {
  for (int i=1; i < 500 / valgrind(1, 2); ++i)
  {
    if (!(i%100))
    {
      ELLE_LOG("bringing node %s up with %s/%s block",
               down, blocks[down].size(), addrs.size());
      servers[down].reset(new DHT(
        ::keys = keys, make_overlay = builder,
        paxos = true,
        ::id = ids[down],
        ::port = keep_port ? ports[down] : 0,
        ::storage = elle::make_unique<infinit::storage::Memory>(blocks[down])));
      for (int s=0; s< n; ++s)
        if (s != down)
        {
          discover(*servers[down], *servers[s], false);
          // cheating a bit...
          discover(*servers[s], *servers[down], false);
        }
      if (wait_connect)
      {
        for (auto& s: servers)
          hard_wait(*s, n-1, client->dht->id());
        //spawn_client();
        hard_wait(*client, n, client->dht->id());
        ELLE_LOG("resuming");
      }
      down = -1;
    }
    else if (!(i%50))
    {
      ELLE_ASSERT(down == -1);
      down = rand()%n;
      ELLE_LOG("bringing node %s down with %s/%s blocks",
               down, blocks[down].size(), addrs.size());
      servers[down].reset();
      if (wait_disconnect)
      {
        for (auto& s: servers)
          if (s)
            hard_wait(*s, n-2, client->dht->id(), true, ids[down]);
          //spawn_client();
          hard_wait(*client, n-1, client->dht->id(), true, ids[down]);
      }
      ELLE_LOG("resuming");
    }
    if (addrs.empty() || !(i%5))
    {
      auto block = client->dht->make_block<ACLBlock>(std::string("block"));
      auto a = block->address();
      client->dht->store(std::move(block), STORE_INSERT, tcr());
      ELLE_DEBUG("created %f", a);
      addrs.push_back(a);
    }
    auto a = addrs[rand()%addrs.size()];
    auto block = client->dht->fetch(a);
    if (i%2)
    {
      dynamic_cast<infinit::model::blocks::ACLBlock*>(block.get())->data(
        elle::Buffer("coincoin"));
      client->dht->store(std::move(block), STORE_UPDATE, tcr());
    }
  }
  }
  catch (...)
  {
    ELLE_ERR("Exception from test: %s", elle::exception_string());
    throw;
  }
}

template<typename C>
typename C::value_type
get_n(C& c, int idx)
{
  auto it = c.begin();
  while (idx--) ++it;
  return *it;
}


void test_churn_socket(Doughnut::OverlayBuilder builder, bool pasv)
{
  static const int n = 5;
  auto keys = infinit::cryptography::rsa::keypair::generate(512);
  infinit::model::Address ids[n];
  unsigned short ports[n];
  infinit::storage::Memory::Blocks blocks[n];
  std::vector<std::unique_ptr<DHT>> servers;
  for (int i=0; i<n; ++i)
  {
    ids[i] = infinit::model::Address::random();
    auto dht = elle::make_unique<DHT>(
      ::id = ids[i],
      ::keys = keys, make_overlay = builder, paxos = true,
      ::storage = elle::make_unique<infinit::storage::Memory>(blocks[i])
    );
    ports[i] = dht->dht->dock().utp_server().local_endpoint().port();
    servers.emplace_back(std::move(dht));
  }
  for (int i=0; i<n; ++i)
    for (int j=i+1; j<n; ++j)
      discover(*servers[i], *servers[j], false);
  for (auto& s: servers)
    hard_wait(*s, n-1);
  std::unique_ptr<DHT> client = elle::make_unique<DHT>(
      ::keys = keys, make_overlay = builder, paxos = true, ::storage = nullptr);
  if (auto kelips = dynamic_cast<infinit::overlay::kelips::Node*>(
    client->dht->overlay().get()))
  {
    kelips->config().query_put_retries = 6;
    kelips->config().query_timeout_ms = valgrind(1000, 4);
  }
  discover(*client, *servers[0], false);
  hard_wait(*client, n, client->dht->id());

  // write some blocks
  std::vector<infinit::model::Address> addrs;
  for (int i=0; i<50; ++i)
  {
    auto block = client->dht->make_block<ACLBlock>(std::string("block"));
    auto a = block->address();
    client->dht->store(std::move(block), STORE_INSERT, tcr());
    ELLE_DEBUG("created %f", a);
    addrs.push_back(a);
  }

  for (int k=0; k < 3 / valgrind(1, 3); ++k)
  {
    ELLE_TRACE("shooting connections");
    // shoot some connections
    for (int i = 0; i < 5; ++i)
    {
      auto& peers = servers[i]->dht->local()->peers();
      for (int l = 0; l < 3; ++l)
      {
        auto peer = get_n(peers, rand() % peers.size());
        if (auto* s = dynamic_cast<reactor::network::TCPSocket*>(
              peer->stream().get()))
          s->close();
        else if (auto* s = dynamic_cast<reactor::network::UTPSocket*>(
                   peer->stream().get()))
          s->close();
        else
          BOOST_FAIL(
            elle::sprintf("could not obtain socket pointer for %s", peer));
      }
    }
    if (!pasv)
    {
      ELLE_TRACE("hard_wait servers");
      for (auto& s: servers)
        hard_wait(*s, n-1);
      ELLE_TRACE("hard_wait client");
      hard_wait(*client, n, client->dht->id());
    }
    else
    {
      // give it time to notice sockets went down
      for (int i = 0; i < 10; ++i)
        reactor::yield();
      ELLE_TRACE("hard_wait servers");
      for (auto& s: servers)
        kouncil_wait_pasv(*s, n-1);
      ELLE_TRACE("hard_wait client");
      kouncil_wait_pasv(*client, n);
    }
    ELLE_TRACE("checking");
    for (auto const& a: addrs)
      client->dht->fetch(a);
   }
   BOOST_CHECK(true);
}

ELLE_TEST_SCHEDULED(churn_socket, (Doughnut::OverlayBuilder, builder))
{
  test_churn_socket(builder, false);
}

ELLE_TEST_SCHEDULED(churn_socket_pasv)
{
  auto const builder =
    [] (Doughnut& dht, std::shared_ptr<Local> local)
    {
      return std::make_unique<kouncil::Kouncil>(&dht, local);
    };
  test_churn_socket(builder, true);
}


ELLE_TEST_SUITE()
{
  elle::os::setenv("INFINIT_CONNECT_TIMEOUT", "1", 1);
  elle::os::setenv("INFINIT_SOFTFAIL_TIMEOUT", "1", 1);
  elle::os::setenv("INFINIT_KOUNCIL_WATCHER_INTERVAL", "1", 1);
  auto& master = boost::unit_test::framework::master_test_suite();
  auto const kelips_builder =
    [] (Doughnut& dht, std::shared_ptr<Local> local)
    {
      auto conf = kelips::Configuration();
      int factor =
#ifdef INFINIT_WINDOWS
        5;
#else
        1;
#endif
      conf.query_get_retries = 4;
      conf.query_put_retries = 4;
      conf.query_timeout_ms = valgrind(1000, 4);
      conf.contact_timeout_ms = factor * valgrind(2000,20);
      conf.ping_interval_ms = factor * valgrind(100, 10);
      conf.ping_timeout_ms = factor * valgrind(1000, 20);
      return std::make_unique<kelips::Node>(
        conf, local, &dht);
    };
  auto const kouncil_builder =
    [] (Doughnut& dht, std::shared_ptr<Local> local)
    {
      return std::make_unique<kouncil::Kouncil>(&dht, local);
    };
#define BOOST_NAMED_TEST_CASE(name,  test_function)                     \
  boost::unit_test::make_test_case(boost::function<void ()>(test_function), \
                                   name,                                \
                                   __FILE__, __LINE__ )

#define TEST_(Overlay, Name, Timeout, Function, ...)                    \
  Overlay                                                               \
    ->add(BOOST_NAMED_TEST_CASE(Name,                                   \
                                std::bind(::Function,                   \
                                          BOOST_PP_CAT(Overlay, _builder), \
                                          ##__VA_ARGS__)),              \
          0, valgrind(Timeout));

#define TEST_ANON(overlay, tname, timeout, ...)                         \
  TEST_(overlay, #tname "_named", timeout, tname, false, ##__VA_ARGS__); \
  TEST_(overlay, #tname "_anon",  timeout, tname, true, ##__VA_ARGS__)

#define TEST(overlay, tname, timeout, ...)              \
  TEST_(overlay, #tname, timeout, tname, ##__VA_ARGS__)

#define TEST_NAMED(overlay, tname, tfunc, timeout, ...) \
  TEST_(overlay, #tname, timeout, tfunc, ##__VA_ARGS__)

#define OVERLAY(Name)                           \
  auto Name = BOOST_TEST_SUITE(#Name);          \
  master.add(Name);                             \
  TEST_ANON(Name, basics, 5);                   \
  TEST_ANON(Name, dead_peer, 5);                \
  TEST_ANON(Name, discover_endpoints, 10);      \
  TEST_ANON(Name, key_cache_invalidation, 10);  \
  TEST_ANON(Name, data_spread, 30, false);      \
  TEST_ANON(Name, data_spread2, 30, false);     \
  TEST_ANON(Name, chain_connect, 30, false);    \
  /* too slow TEST(Name, paxos_3_1, 30);*/      \
  TEST_ANON(Name, parallel_discover, 20);       \
  TEST_NAMED(Name, storm_paxos, storm, 60, true, 5, 5, 100);  \
  TEST_NAMED(Name, storm,       storm, 60, false, 5, 5, 200); \
  TEST_NAMED(Name, churn, churn, 600, false, true, true);     \
  TEST_NAMED(Name, churn_socket, churn_socket, 600);

  OVERLAY(kelips);
  OVERLAY(kouncil);

  kouncil->add(BOOST_TEST_CASE(churn_socket_pasv), 0, valgrind(120));
#undef OVERLAY
}

// int main()
// {
//   auto const kelips_builder =
//     [] (Doughnut& dht, std::shared_ptr<Local> local)
//     {
//       return std::make_unique<kelips::Node>(
//         kelips::Configuration(), local, &dht);
//     };
//   dead_peer(kelips_builder, false);
// }
