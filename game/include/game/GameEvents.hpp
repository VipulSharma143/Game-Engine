#pragma once
// Events published on GameApplication::events(). Other systems (HUD feed, audio, stats)
// subscribe instead of being called directly.

#include <cstddef>

#include "engine/core/Math.hpp"
#include "engine/world/World.hpp"

namespace game {

enum class GameState { Menu, Playing, Paused, Victory, GameOver, Quit };

// Published at the end of every fixed update step (also while paused or in a menu), after
// everything that step triggered, so a sound started in a step is mixed in that same step.
struct FixedTick { double dt = 0.0; };
struct ShotFired { int ammo_left = 0; int weapon = 0; };  // weapon: a WeaponId
struct TargetHit { ge::Vec2 position; int damage = 0; bool killed = false; };
struct TargetDestroyed { ge::Vec2 position; int points = 0; };
struct WaveStarted { int wave = 0; std::size_t targets = 0; };
struct WaveCleared { int wave = 0; int bonus = 0; };
struct ReloadStarted {};
struct StateChanged { GameState from = GameState::Menu; GameState to = GameState::Menu; };
struct PlayerHit { int amount = 0; int health_left = 0; };
struct PlayerDied { int score = 0; };
struct EnemyKilled { ge::Vec2 position; int points = 0; };
struct PickupCollected { ge::PickupKind kind = ge::PickupKind::Health; ge::Vec2 position; };
struct DoorOpened { int x = 0; int y = 0; };
struct DoorLocked { int x = 0; int y = 0; };
struct EnemyShot { ge::Vec2 position; };                 // an enemy fired a bolt
struct Explosion { ge::Vec2 position; double radius = 0.0; };
struct WeaponSwitched { int weapon = 0; };              // a WeaponId
struct WeaponPickedUp { int weapon = 0; int ammo_added = 0; bool first_time = false; };
struct GameWon { int score = 0; unsigned shots = 0; unsigned hits = 0; };

}  // namespace game
