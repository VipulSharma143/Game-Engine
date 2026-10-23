#pragma once
// Enemy behaviour: pure functions and small classes with no dependency on the game loop, so
// every rule can be unit-tested.

#include <vector>

#include "engine/core/Math.hpp"
#include "engine/world/World.hpp"

namespace game {

enum class EnemyState { Idle, Chase, Attack };

// Component attached to enemy entities (together with Transform, Health and Collider).
struct Enemy {
    EnemyState state = EnemyState::Idle;
    double speed = 1.7;           // cells per second
    double sight = 9.0;           // notices the player in line of sight within this distance
    double attack_range = 0.95;   // bites when this close
    double attack_interval = 0.9; // seconds between bites
    double attack_cooldown = 0.0;
    int damage = 10;
    double radius = 0.3;          // body radius (collision and hit detection)
    double flash = 0.0;           // 0..1 hit feedback
};

// True if no wall lies between `a` and `b`.
bool line_of_sight(const ge::GridMap& map, ge::Vec2 a, ge::Vec2 b) noexcept;

// Breadth-first distances (in steps, 8-connected, no corner cutting) from every open cell to a
// goal cell. Doors and walls are solid, so enemies path around closed doors or wait behind them.
class DistanceField {
public:
    void rebuild(const ge::GridMap& map, int goal_x, int goal_y);
    int goal_x() const noexcept { return goal_x_; }
    int goal_y() const noexcept { return goal_y_; }
    // Steps to the goal, or -1 if the cell is solid, unreachable or outside the map.
    int distance(int x, int y) const noexcept;
    // Unit vector from `position` towards the centre of the neighbouring cell that is closest to
    // the goal. Zero vector if the position is in the goal cell or cannot reach it.
    ge::Vec2 direction_from(ge::Vec2 position) const noexcept;

private:
    int width_ = 0, height_ = 0, goal_x_ = -1, goal_y_ = -1;
    std::vector<int> dist_;
    const ge::GridMap* map_ = nullptr;
};

struct EnemyDecision {
    ge::Vec2 move{};      // displacement for this step (before collision)
    bool attack = false;  // bites the player this step
};

// Advances the state machine and returns what the enemy wants to do. `alerted` means the player
// just made noise (fired); an alerted idle enemy starts chasing even without line of sight.
EnemyDecision enemy_think(Enemy& enemy, ge::Vec2 position, ge::Vec2 player, const ge::GridMap& map,
                          const DistanceField& field, bool alerted, double dt) noexcept;

// Moves a square of half-size `half` by `delta`, sliding along walls (axes are resolved separately).
ge::Vec2 slide_move(const ge::GridMap& map, ge::Vec2 position, ge::Vec2 delta, double half) noexcept;

}  // namespace game
