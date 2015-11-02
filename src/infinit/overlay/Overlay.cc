#include <elle/log.hh>

#include <infinit/overlay/Overlay.hh>
#include <infinit/model/MissingBlock.hh>

ELLE_LOG_COMPONENT("infinit.overlay.Overlay");

namespace infinit
{
  namespace overlay
  {
    /*-------------.
    | Construction |
    `-------------*/

    Overlay::Overlay(model::Address node_id)
      : _node_id(std::move(node_id))
      , _doughnut(nullptr)
    {}

    elle::json::Json
    Overlay::query(std::string const& k, boost::optional<std::string> const& v)
    {
      return {};
    }

    /*-------.
    | Lookup |
    `-------*/

    void
    Overlay::register_local(std::shared_ptr<model::doughnut::Local> local)
    {}

    reactor::Generator<Overlay::Member>
    Overlay::lookup(model::Address address, int n, Operation op) const
    {
      ELLE_TRACE_SCOPE("%s: lookup %s nodes for %s", *this, n, address);
      return this->_lookup(address, n, op);
    }

    Overlay::Member
    Overlay::lookup(model::Address address, Operation op) const
    {
      ELLE_DEBUG("address %7.s", address);
      auto gen = this->lookup(address, 1, op);
      for (auto res: gen)
        return res;
      throw model::MissingBlock(address);
    }

    Overlay::Member
    Overlay::lookup_node(model::Address address)
    {
      return this->_lookup_node(address);
    }

    void
    Configuration::join()
    {
      this->_node_id = model::Address::random();
    }

    void
    Configuration::serialize(elle::serialization::Serializer& s)
    {
      s.serialize("node_id", this->_node_id);
    }
  }
}
