#include <elle/serialization/Serializer.hh>

#include <infinit/model/doughnut/HandshakeFailed.hh>

namespace infinit
{
  namespace model
  {
    namespace doughnut
    {
      HandshakeFailed::HandshakeFailed(std::string const& reason)
        : Super(elle::sprintf("Handshake failed: %s", reason))
      {}

      HandshakeFailed::HandshakeFailed(
        elle::serialization::SerializerIn& input)
        : Super(input)
      {}

      static const elle::serialization::Hierarchy<elle::Exception>::
      Register<HandshakeFailed> _register_serialization;
    }
  }
}