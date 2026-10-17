#pragma once

#include "engine/core/Math.hpp"
#include "engine/world/World.hpp"

namespace ge {

// Moves `position` by `delta`, resolving each axis separately so the mover
// slides along walls. Large deltas are sub-stepped so they cannot tunnel.
Vec2 move_with_collision(const GridMap& map, Vec2 position, Vec2 delta, double half_extent) noexcept;

}  // namespace ge
