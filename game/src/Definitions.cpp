#include "game/Definitions.hpp"

namespace game {

namespace {

//                                 name        mag dmg pel spread  interval reload  start max  pack falloff min   proj  speed splash slot
constexpr WeaponDef kWeapons[kWeaponCount] = {
    /* Pistol   */ {"PISTOL",       12, 34, 1, 0.000, 0.22, 1.0, -1, -1, 0, 0.0, 1.0, false, 0.0, 0.0, 1},
    /* Shotgun  */ {"SHOTGUN",       6, 14, 8, 0.110, 0.85, 1.6, 12, 36, 6, 9.0, 0.3, false, 0.0, 0.0, 2},
    /* Chaingun */ {"CHAINGUN",     40, 14, 1, 0.035, 0.07, 2.2, 80, 240, 40, 0.0, 1.0, false, 0.0, 0.0, 3},
    /* Launcher */ {"ROCKET LAUNCHER", 1, 110, 1, 0.000, 0.90, 1.2, 5, 15, 3, 0.0, 1.0, true, 9.0, 2.2, 4},
};

//                                 name      hp  speed sight melee  mint  dmg  radius height pts  ranged min  max  shoot dmg  shotspeed
constexpr EnemyDef kEnemies[ge::kEnemyKindCount] = {
    /* Grunt   */ {"GRUNT",   60, 1.7, 9.0,  0.95, 0.9, 10, 0.30, 0.65, 150, false, 0.0, 0.0, 0.0, 0,  0.0},
    /* Gunner  */ {"GUNNER",  50, 1.5, 11.0, 0.0,  0.0, 0,  0.30, 0.70, 220, true,  3.5, 8.5, 1.6, 12, 5.5},
    /* Brute   */ {"BRUTE",  170, 1.15, 9.0, 1.20, 1.3, 28, 0.42, 1.00, 400, false, 0.0, 0.0, 0.0, 0,  0.0},
    /* Skitter */ {"SKITTER", 24, 3.6, 8.0,  0.70, 0.5, 5,  0.20, 0.40, 90,  false, 0.0, 0.0, 0.0, 0,  0.0},
};

}  // namespace

const WeaponDef& weapon_def(WeaponId id) noexcept { return kWeapons[static_cast<int>(id) % kWeaponCount]; }
const EnemyDef& enemy_def(ge::EnemyKind kind) noexcept { return kEnemies[static_cast<int>(kind) % ge::kEnemyKindCount]; }

}  // namespace game
