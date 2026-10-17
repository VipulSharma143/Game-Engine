#include "game/PlayerController.hpp"

#include <cmath>

#include "engine/physics/Movement.hpp"

namespace game {

void update_player(Player& player, const ge::InputState& input, const ge::GridMap& map, double dt) noexcept {
    player.angle += ge::clamp(input.turn, -1.0, 1.0) * player.turn_speed * dt;
    if (player.angle > ge::kPi) player.angle -= 2.0 * ge::kPi;
    if (player.angle < -ge::kPi) player.angle += 2.0 * ge::kPi;

    const double f = ge::clamp(input.forward, -1.0, 1.0);
    const double s = ge::clamp(input.strafe, -1.0, 1.0);
    const double len = std::sqrt(f * f + s * s);
    if (len <= 0.0) return;
    const double scale = (len > 1.0 ? 1.0 / len : 1.0) * player.move_speed * dt;

    const ge::Vec2 forward = ge::heading_vector(player.angle);
    const ge::Vec2 delta = (forward * f + ge::right_of(forward) * s) * scale;
    player.position = ge::move_with_collision(map, player.position, delta, player.half_extent);
}

ge::Camera camera_for(const Player& player, double fov_degrees) noexcept {
    return ge::Camera::from_angle(player.position, player.angle, fov_degrees);
}

}  // namespace game
