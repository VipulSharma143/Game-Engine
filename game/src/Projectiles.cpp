#include "game/Projectiles.hpp"

#include <algorithm>
#include <cmath>

namespace game {

Impact step_projectile(Projectile& p, const ge::GridMap& map, double dt, std::span<const ShotTarget> targets,
                       ge::Vec2 player, double player_radius) noexcept {
    constexpr double kMaxStep = 0.15;
    const double speed = std::sqrt(p.velocity.x * p.velocity.x + p.velocity.y * p.velocity.y);
    const int steps = std::max(1, static_cast<int>(std::ceil(speed * dt / kMaxStep)));
    const double sub = dt / steps;
    for (int i = 0; i < steps; ++i) {
        const ge::Vec2 next{p.position.x + p.velocity.x * sub, p.position.y + p.velocity.y * sub};
        if (map.is_solid(static_cast<int>(std::floor(next.x)), static_cast<int>(std::floor(next.y)))) return {ImpactKind::Wall, 0, p.position};
        p.position = next;
        if (p.kind == ProjectileKind::Bolt) {
            const double dx = p.position.x - player.x, dy = p.position.y - player.y, reach = player_radius + p.radius;
            if (dx * dx + dy * dy <= reach * reach) return {ImpactKind::Player, 0, p.position};
        } else {
            for (std::size_t t = 0; t < targets.size(); ++t) {
                const double dx = p.position.x - targets[t].position.x, dy = p.position.y - targets[t].position.y;
                const double reach = targets[t].radius + p.radius;
                if (dx * dx + dy * dy <= reach * reach) return {ImpactKind::Target, t, p.position};
            }
        }
    }
    p.life -= dt;
    if (p.life <= 0.0) return {ImpactKind::Wall, 0, p.position};
    return {};
}

}  // namespace game
