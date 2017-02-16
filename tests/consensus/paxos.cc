#include <elle/test.hh>

#include "../DHT.hh"

ELLE_LOG_COMPONENT("infinit.model.doughnut.consensus.Paxos.test");

ELLE_TEST_SCHEDULED(availability_2)
{
  auto a = std::make_unique<DHT>();
  auto b = std::make_unique<DHT>();
  a->overlay->connect(*b->overlay);
  auto block = a->dht->make_block<infinit::model::blocks::MutableBlock>();
  block->data(elle::Buffer("foo"));
  a->dht->store(*block, infinit::model::STORE_INSERT);
  block->data(elle::Buffer("foobar"));
  a->dht->store(*block, infinit::model::STORE_UPDATE);
  b.reset();
  block->data(elle::Buffer("foobarbaz"));
  BOOST_CHECK_EQUAL(a->dht->fetch(block->address())->data(), "foobar");
  BOOST_CHECK_THROW(a->dht->store(*block, infinit::model::STORE_UPDATE),
                                  athena::paxos::TooFewPeers);
}

ELLE_TEST_SCHEDULED(availability_3)
{
  auto a = std::make_unique<DHT>();
  auto b = std::make_unique<DHT>();
  auto c = std::make_unique<DHT>();
  a->overlay->connect(*b->overlay);
  a->overlay->connect(*c->overlay);
  b->overlay->connect(*c->overlay);
  auto block = a->dht->make_block<infinit::model::blocks::MutableBlock>();
  ELLE_LOG("store block")
  {
    block->data(elle::Buffer("foo"));
    a->dht->store(*block, infinit::model::STORE_INSERT);
    block->data(elle::Buffer("foobar"));
    a->dht->store(*block, infinit::model::STORE_UPDATE);
  }
  ELLE_LOG("test 2/3 nodes")
  {
    c.reset();
    BOOST_CHECK_EQUAL(b->dht->fetch(block->address())->data(), "foobar");
    block->data(elle::Buffer("foobarbaz"));
    a->dht->store(*block, infinit::model::STORE_UPDATE);
  }
  ELLE_LOG("test 1/3 nodes")
  {
    b.reset();
    BOOST_CHECK_THROW(a->dht->fetch(block->address())->data(),
                      athena::paxos::TooFewPeers);
    block->data(elle::Buffer("foobarbazquux"));
    BOOST_CHECK_THROW(a->dht->store(*block, infinit::model::STORE_UPDATE),
                      athena::paxos::TooFewPeers);
  }
}

ELLE_TEST_SUITE()
{
  auto& suite = boost::unit_test::framework::master_test_suite();
  suite.add(BOOST_TEST_CASE(availability_2), 0, 10);
  suite.add(BOOST_TEST_CASE(availability_3), 0, 10);
}
