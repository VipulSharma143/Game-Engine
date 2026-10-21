#pragma once
// Hitscan weapon: magazine, fire rate, reload, and a ray test against round targets that
// is blocked by walls.

#include <span>

#include "engine/core/Math.hpp"
#include "engine/world/World.hpp"

namespace game {

struct Weapon {
    int magazine = 12;          // rounds per magazine
    int ammo = 12;              // rounds currently loaded
    int damage = 34;
    double fire_interval = 0.22;   // seconds between shots
    double reload_time = 1.0;      // seconds
    double cooldown = 0.0;         // > 0: cannot fire yet
    double reload_left = 0.0;      // > 0: reloading
    double flash = 0.0;            // muzzle flash / recoil, decays to 0
    bool reloading() const noexcept { return reload_left > 0.0; }
};

struct ShotTarget {
    ge::Vec2 position;
    double radius;
};

struct ShotResult {
    int target = -1;               // index into the target list, -1 = none
    double distance = 0.0;         // distance to what the shot hit (target or wall)
};

// Casts a ray from `origin` along `angle`. A target counts only if it is closer than the wall.
ShotResult hitscan(const ge::GridMap& map, ge::Vec2 origin, double angle, std::span<const ShotTarget> targets);

// Advances timers and consumes ammo. Returns true if a round was fired on this step.
// An empty magazine starts a reload automatically; `reload` starts one on demand.
bool weapon_step(Weapon& weapon, bool fire, bool reload, double dt) noexcept;

}  // namespace game
