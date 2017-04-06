#pragma once

#include <infinit/model/blocks/Block.hh>

namespace infinit
{
  namespace model
  {
    namespace blocks
    {
      class ImmutableBlock
        : public Block
        , private InstanceTracker<ImmutableBlock>
      {
      /*------.
      | Types |
      `------*/
      public:
        using Self = ImmutableBlock;
        using Super = Block;
        static char const* type;

      /*-------------.
      | Construction |
      `-------------*/
      public:
        ImmutableBlock(ImmutableBlock&&) = default;

      protected:
        ImmutableBlock(Address address);
        ImmutableBlock(Address address, elle::Buffer data);
        ImmutableBlock(ImmutableBlock const& other);
        friend class infinit::model::Model;

      /*-------.
      | Clone  |
      `-------*/
      public:

      std::unique_ptr<Block>
      clone() const override;


      /*--------------.
      | Serialization |
      `--------------*/
      public:
        ImmutableBlock(elle::serialization::Serializer& input,
                       elle::Version const& version);
      };
    }
  }
}
