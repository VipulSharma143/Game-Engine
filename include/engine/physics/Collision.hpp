#pragma once

#include "engine/core/Math.hpp"
#include "engine/world/World.hpp"

namespace ge {

// True if the axis-aligned square centred at `center` with the given half extent
// overlaps any solid cell (cells outside the map count as solid).
bool collides_aabb(const GridMap& map, Vec2 center, double half_extent) noexcept;

}  // namespace ge
