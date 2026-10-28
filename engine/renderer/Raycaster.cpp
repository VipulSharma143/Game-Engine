#include "engine/renderer/Raycaster.hpp"

#include <algorithm>
#include <cmath>

namespace ge {

RayHit cast_ray(const GridMap& map, Vec2 origin, Vec2 dir) noexcept {
    constexpr double kInfinity = 1e30;
    RayHit hit;
    int cx = static_cast<int>(std::floor(origin.x));
    int cy = static_cast<int>(std::floor(origin.y));

    if (map.is_solid(cx, cy)) {
        hit.hit = true;
        hit.cell_x = cx;
        hit.cell_y = cy;
        hit.wall_id = map.cell(cx, cy);
        return hit;
    }

    const double delta_x = dir.x == 0.0 ? kInfinity : std::fabs(1.0 / dir.x);
    const double delta_y = dir.y == 0.0 ? kInfinity : std::fabs(1.0 / dir.y);
    const int step_x = dir.x < 0.0 ? -1 : 1;
    const int step_y = dir.y < 0.0 ? -1 : 1;
    double side_x = dir.x < 0.0 ? (origin.x - cx) * delta_x : (cx + 1.0 - origin.x) * delta_x;
    double side_y = dir.y < 0.0 ? (origin.y - cy) * delta_y : (cy + 1.0 - origin.y) * delta_y;

    const int max_steps = map.width() + map.height() + 2;
    for (int i = 0; i < max_steps; ++i) {
        bool y_side;
        if (side_x < side_y) {
            side_x += delta_x;
            cx += step_x;
            y_side = false;
        } else {
            side_y += delta_y;
            cy += step_y;
            y_side = true;
        }
        if (map.is_solid(cx, cy)) {
            hit.hit = true;
            hit.cell_x = cx;
            hit.cell_y = cy;
            hit.wall_id = map.cell(cx, cy);
            hit.y_side = y_side;
            hit.distance = y_side ? side_y - delta_y : side_x - delta_x;
            return hit;
        }
    }
    return hit;
}

}  // namespace ge

namespace ge {

double wall_texture_u(Vec2 origin, Vec2 ray, const RayHit& hit) noexcept {
    const double along = hit.y_side ? origin.x + ray.x * hit.distance : origin.y + ray.y * hit.distance;
    double u = along - std::floor(along);
    // Screen-right is right_of(forward) = (-fy, fx) in this engine: facing -x the right side is -y,
    // facing +y it is -x. Flip in those two cases so u always grows towards screen-right.
    if ((!hit.y_side && ray.x < 0.0) || (hit.y_side && ray.y > 0.0)) u = 1.0 - u;
    return u < 1.0 ? u : 0.999999;
}

}  // namespace ge

namespace ge {

CrosshairTarget crosshair_target(const Level& level, const Camera& camera, double reach) noexcept {
    constexpr double kPickRadius = 0.3;  // tiles: how close to the ray an object must be to be picked
    CrosshairTarget out;
    const Vec2 dir = camera.direction;  // unit length, so distances along the ray are tiles
    const RayHit wall = cast_ray(level.map, camera.position, dir);
    const double wall_distance = wall.hit ? wall.distance : 1e30;
    if (wall.hit) {
        out.hit = true;
        out.cell_x = wall.cell_x;
        out.cell_y = wall.cell_y;
        out.distance = wall.distance;
    }

    double best = wall_distance;
    auto consider = [&](Vec2 p) {
        const Vec2 rel = p - camera.position;
        const double along = rel.x * dir.x + rel.y * dir.y;
        const double across = rel.x * dir.y - rel.y * dir.x;
        if (along <= 0.0 || along >= best || std::fabs(across) > kPickRadius) return;
        best = along;
        out.hit = out.is_object = true;
        out.cell_x = static_cast<int>(std::floor(p.x));
        out.cell_y = static_cast<int>(std::floor(p.y));
        out.distance = along;
    };
    consider(level.spawn);
    for (const Vec2& p : level.target_spawns) consider(p);
    for (const Vec2& p : level.enemy_spawns) consider(p);
    for (const PickupSpawn& p : level.pickups) consider(p.position);

    const double t = clamp(std::min(reach, wall_distance - 0.01), 0.0, reach);
    out.free_x = static_cast<int>(std::floor(camera.position.x + dir.x * t));
    out.free_y = static_cast<int>(std::floor(camera.position.y + dir.y * t));
    return out;
}

}  // namespace ge
