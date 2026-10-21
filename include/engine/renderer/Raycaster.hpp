#pragma once

#include <cstdint>

#include "engine/core/Math.hpp"
#include "engine/world/World.hpp"

namespace ge {

struct RayHit {
    bool hit = false;
    int cell_x = 0;
    int cell_y = 0;
    std::uint8_t wall_id = 0;
    bool y_side = false;   // true when the ray crossed a horizontal grid line last
    double distance = 0.0; // ray parameter t: hit point = origin + dir * t
};

// Grid DDA. With dir = camera.direction + camera.plane * k, `distance` is the
// perpendicular distance to the camera plane (no fisheye correction needed).
RayHit cast_ray(const GridMap& map, Vec2 origin, Vec2 dir) noexcept;

// Horizontal texture coordinate in [0, 1) of a wall hit, oriented so textures are never
// mirrored whichever face or direction the ray arrives from.
double wall_texture_u(Vec2 origin, Vec2 ray, const RayHit& hit) noexcept;

}  // namespace ge
