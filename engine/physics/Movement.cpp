#include "engine/physics/Movement.hpp"

#include <algorithm>
#include <cmath>

#include "engine/physics/Collision.hpp"

namespace ge {

Vec2 move_with_collision(const GridMap& map, Vec2 position, Vec2 delta, double half_extent) noexcept {
    constexpr double kMaxSubstep = 0.25;
    constexpr int kMaxSubsteps = 64;
    const double longest = std::max(std::fabs(delta.x), std::fabs(delta.y));
    const int steps = std::clamp(static_cast<int>(std::ceil(longest / kMaxSubstep)), 1, kMaxSubsteps);
    const Vec2 part = delta * (1.0 / steps);
    for (int i = 0; i < steps; ++i) {
        if (!collides_aabb(map, {position.x + part.x, position.y}, half_extent)) position.x += part.x;
        if (!collides_aabb(map, {position.x, position.y + part.y}, half_extent)) position.y += part.y;
    }
    return position;
}

}  // namespace ge
