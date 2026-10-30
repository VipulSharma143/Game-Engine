#pragma once
// Things that fly: an enemy gunner's bolt and the player's rocket. Pure movement and hit tests, so the rules are
// unit-testable without the game loop.

#include <cstddef>
#include <span>

#include "engine/core/Math.hpp"
#include "engine/world/World.hpp"
#include "game/WeaponSystem.hpp"

namespace game {

enum class ProjectileKind { Bolt, Rocket };

struct Projectile {
    ProjectileKind kind = ProjectileKind::Bolt;
    ge::Vec2 position{};
    ge::Vec2 velocity{};     // cells per second
    int damage = 10;
    double radius = 0.12;    // hit radius
    double life = 4.0;       // seconds left; at 0 it fizzles at its current position
};

enum class ImpactKind { None, Wall, Target, Player };

struct Impact {
    ImpactKind kind = ImpactKind::None;
    std::size_t index = 0;   // Target: index into the target list passed to step_projectile
    ge::Vec2 point{};        // where it stopped
};

// Moves `p` forward by dt in steps of at most 0.15 cells (so nothing is skipped through). A Bolt hits the player
// (a circle of `player_radius`) or a wall; a Rocket hits the first of `targets` it touches or a wall. Running out of
// life counts as a Wall impact. Returns ImpactKind::None while it is still flying.
Impact step_projectile(Projectile& p, const ge::GridMap& map, double dt, std::span<const ShotTarget> targets,
                       ge::Vec2 player, double player_radius) noexcept;

}  // namespace game
