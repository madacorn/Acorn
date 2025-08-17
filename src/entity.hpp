#pragma once
#include <cstdint>

namespace acorn
{
  struct Entity
  {
    uint32_t index{UINT32_MAX};
    uint32_t generation{UINT32_MAX};

    friend constexpr bool operator==(Entity lhs, Entity rhs) noexcept
    {
      return lhs.index == rhs.index && lhs.generation == rhs.generation;
    }

    friend constexpr bool operator!=(Entity lhs, Entity rhs) noexcept
    {
      return !(lhs == rhs);
    }

    static constexpr Entity null() noexcept { return {UINT32_MAX, UINT32_MAX}; }

    constexpr bool is_null() const noexcept
    {
      return *this == null();
    }
  };
}
