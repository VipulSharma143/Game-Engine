#pragma once
// Entity handle: index + generation, so a stale handle to a destroyed entity is detectable.

#include <cstdint>

namespace ge {

struct Entity {
    std::uint32_t index = 0xFFFFFFFFu;
    std::uint32_t generation = 0;

    constexpr bool valid() const noexcept { return index != 0xFFFFFFFFu; }
    friend constexpr bool operator==(Entity a, Entity b) noexcept {
        return a.index == b.index && a.generation == b.generation;
    }
};

inline constexpr Entity kNullEntity{};

}  // namespace ge
