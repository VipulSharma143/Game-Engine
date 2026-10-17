#include "engine/physics/Collision.hpp"

#include <cmath>

namespace ge {

bool collides_aabb(const GridMap& map, Vec2 center, double half_extent) noexcept {
    const int x0 = static_cast<int>(std::floor(center.x - half_extent));
    const int x1 = static_cast<int>(std::floor(center.x + half_extent));
    const int y0 = static_cast<int>(std::floor(center.y - half_extent));
    const int y1 = static_cast<int>(std::floor(center.y + half_extent));
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x)
            if (map.is_solid(x, y)) return true;
    return false;
}

}  // namespace ge
