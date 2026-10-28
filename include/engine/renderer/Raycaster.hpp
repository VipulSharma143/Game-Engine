#pragma once

#include <cstdint>

#include "engine/core/Math.hpp"
#include "engine/renderer/Camera.hpp"
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

// What the crosshair points at, for editing: the first object (target, enemy, pickup, player start) the centre
// ray passes close to, otherwise the wall it hits. `free_*` is the open cell where an insert or move lands: the cell
// `reach` tiles along the ray, or the cell in front of the wall if that is nearer.
struct CrosshairTarget {
    bool hit = false;        // something is under the crosshair
    bool is_object = false;  // an object, not a wall
    int cell_x = 0;
    int cell_y = 0;
    int free_x = 0;
    int free_y = 0;
    double distance = 0.0;
};
CrosshairTarget crosshair_target(const Level& level, const Camera& camera, double reach) noexcept;

}  // namespace ge
