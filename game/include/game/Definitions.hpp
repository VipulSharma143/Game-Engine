#pragma once
// The numbers that make each weapon and each enemy what it is, in one place. Gameplay code asks these tables
// instead of hard-coding values, so a new weapon or enemy is a new row here plus its behaviour.

#include "engine/world/World.hpp"

namespace game {

enum class WeaponId { Pistol, Shotgun, Chaingun, Launcher };
constexpr int kWeaponCount = 4;

struct WeaponDef {
    const char* name;
    int magazine;            // rounds loaded at once
    int damage;              // per pellet or bullet; for the launcher the damage at the centre of the blast
    int pellets;             // rays fired per shot (the shotgun fires several)
    double spread;           // half-angle of the cone the rays are spread over, radians (0 = perfectly accurate)
    double fire_interval;    // seconds between shots
    double reload_time;      // seconds
    int start_reserve;       // spare rounds when the weapon is first picked up (-1 = unlimited)
    int max_reserve;         // spare rounds you can carry (-1 = unlimited)
    int ammo_pickup;         // spare rounds an ammo pack adds
    double falloff_range;    // damage fades to `falloff_min` of full at this distance (0 = no falloff)
    double falloff_min;
    bool projectile;         // fires a rocket that flies and explodes instead of hitting at once
    double projectile_speed; // cells per second
    double splash_radius;    // cells
    unsigned char slot;      // 1-based key that selects it
};

struct EnemyDef {
    const char* name;
    int health;
    double speed;            // cells per second
    double sight;            // notices the player in line of sight within this many cells
    double melee_range;      // bites from this close (melee enemies)
    double melee_interval;   // seconds between bites
    int melee_damage;
    double radius;           // body radius: collision and being hit
    double height;           // drawn height in wall heights
    int points;
    bool ranged;             // keeps its distance and shoots instead of biting
    double min_range;        // ranged: backs away when the player is closer than this
    double max_range;        // ranged: stands and shoots within this distance
    double shoot_interval;
    int shot_damage;
    double shot_speed;       // cells per second
};

const WeaponDef& weapon_def(WeaponId id) noexcept;
const EnemyDef& enemy_def(ge::EnemyKind kind) noexcept;

}  // namespace game
