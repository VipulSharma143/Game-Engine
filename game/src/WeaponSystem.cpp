#include "game/WeaponSystem.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "engine/renderer/Raycaster.hpp"

namespace game {

ShotResult hitscan(const ge::GridMap& map, ge::Vec2 origin, double angle, std::span<const ShotTarget> targets) {
    const ge::Vec2 dir = ge::heading_vector(angle);
    const ge::RayHit wall = ge::cast_ray(map, origin, dir);
    ShotResult result;
    result.distance = wall.hit ? wall.distance : std::numeric_limits<double>::infinity();

    double best = result.distance;
    for (std::size_t i = 0; i < targets.size(); ++i) {
        const ge::Vec2 to = targets[i].position - origin;
        const double along = to.x * dir.x + to.y * dir.y;
        if (along <= 0.0) continue;  // behind the shooter
        const double perp2 = to.x * to.x + to.y * to.y - along * along;
        const double r2 = targets[i].radius * targets[i].radius;
        if (perp2 > r2) continue;
        const double enter = std::max(along - std::sqrt(r2 - perp2), 0.0);
        if (enter < best) {
            best = enter;
            result.target = static_cast<int>(i);
            result.distance = enter;
        }
    }
    return result;
}

bool weapon_step(Weapon& w, bool fire, bool reload, double dt) noexcept {
    w.cooldown = std::max(0.0, w.cooldown - dt);
    w.flash = std::max(0.0, w.flash - dt * 6.0);
    if (w.reloading()) {
        w.reload_left -= dt;
        if (w.reload_left <= 0.0) {
            w.reload_left = 0.0;
            const int need = w.magazine - w.ammo;
            const int take = w.reserve < 0 ? need : std::min(need, w.reserve);
            w.ammo += take;
            if (w.reserve >= 0) w.reserve -= take;
        }
        return false;
    }
    if (w.out_of_ammo()) return false;  // nothing left to fire or load
    if ((reload && w.can_reload()) || w.ammo == 0) {
        w.reload_left = w.reload_time;
        return false;
    }
    if (fire && w.cooldown <= 0.0) {
        --w.ammo;
        w.cooldown = w.fire_interval;
        w.flash = 1.0;
        return true;
    }
    return false;
}

}  // namespace game
