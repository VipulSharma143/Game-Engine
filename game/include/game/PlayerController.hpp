#pragma once

#include "engine/core/Math.hpp"
#include "engine/platform/Platform.hpp"
#include "engine/renderer/Camera.hpp"
#include "engine/world/World.hpp"

namespace game {

struct Player {
    ge::Vec2 position{1.5, 1.5};
    double angle = 0.0;           // radians, see ge::heading_vector
    double half_extent = 0.25;    // collision half size, in cells
    double move_speed = 3.0;      // cells per second
    double turn_speed = 2.2;      // radians per second
};

// Applies one fixed-step of input to the player: turning, strafing and walking
// with wall collision. Diagonal movement is normalised.
void update_player(Player& player, const ge::InputState& input, const ge::GridMap& map, double dt) noexcept;

ge::Camera camera_for(const Player& player, double fov_degrees) noexcept;

}  // namespace game
