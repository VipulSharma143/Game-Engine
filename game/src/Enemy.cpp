#include "game/Enemy.hpp"

#include <cmath>
#include <queue>

#include "engine/physics/Collision.hpp"
#include "engine/renderer/Raycaster.hpp"
#include "game/Definitions.hpp"

namespace game {

bool line_of_sight(const ge::GridMap& map, ge::Vec2 a, ge::Vec2 b) noexcept {
    const ge::Vec2 d = b - a;
    const double dist = std::sqrt(d.x * d.x + d.y * d.y);
    if (dist < 1e-9) return true;
    const ge::RayHit hit = ge::cast_ray(map, a, {d.x / dist, d.y / dist});
    return !hit.hit || hit.distance >= dist;
}

namespace {
constexpr int kDx[8] = {1, -1, 0, 0, 1, 1, -1, -1};  // orthogonal neighbours first, then diagonals
constexpr int kDy[8] = {0, 0, 1, -1, 1, -1, 1, -1};

// A diagonal step is allowed only if both orthogonal cells are open (no cutting corners).
bool step_allowed(const ge::GridMap& m, int x, int y, int k) noexcept {
    const int nx = x + kDx[k], ny = y + kDy[k];
    if (m.is_solid(nx, ny)) return false;
    if (k >= 4 && (m.is_solid(x + kDx[k], y) || m.is_solid(x, y + kDy[k]))) return false;
    return true;
}
}  // namespace

void DistanceField::rebuild(const ge::GridMap& map, int goal_x, int goal_y) {
    width_ = map.width();
    height_ = map.height();
    goal_x_ = goal_x;
    goal_y_ = goal_y;
    map_ = &map;
    dist_.assign(static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_), -1);
    if (goal_x < 0 || goal_y < 0 || goal_x >= width_ || goal_y >= height_) return;
    std::queue<std::pair<int, int>> q;
    dist_[static_cast<std::size_t>(goal_y * width_ + goal_x)] = 0;  // the goal counts even if it is a doorway
    q.push({goal_x, goal_y});
    while (!q.empty()) {
        const auto [x, y] = q.front();
        q.pop();
        const int d = dist_[static_cast<std::size_t>(y * width_ + x)];
        for (int k = 0; k < 8; ++k) {
            if (!step_allowed(map, x, y, k)) continue;
            const int nx = x + kDx[k], ny = y + kDy[k];
            int& nd = dist_[static_cast<std::size_t>(ny * width_ + nx)];
            if (nd >= 0) continue;
            nd = d + 1;
            q.push({nx, ny});
        }
    }
}

int DistanceField::distance(int x, int y) const noexcept {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) return -1;
    return dist_[static_cast<std::size_t>(y * width_ + x)];
}

ge::Vec2 DistanceField::direction_from(ge::Vec2 p) const noexcept {
    if (!map_) return {};
    const int cx = static_cast<int>(std::floor(p.x)), cy = static_cast<int>(std::floor(p.y));
    const int here = distance(cx, cy);
    if (here <= 0) return {};  // in the goal cell, or no route
    int best = here, best_k = -1;
    for (int k = 0; k < 8; ++k) {
        if (!step_allowed(*map_, cx, cy, k)) continue;
        const int d = distance(cx + kDx[k], cy + kDy[k]);
        if (d >= 0 && d < best) { best = d; best_k = k; }
    }
    if (best_k < 0) return {};
    const ge::Vec2 target{cx + kDx[best_k] + 0.5, cy + kDy[best_k] + 0.5};
    const ge::Vec2 v = target - p;
    const double len = std::sqrt(v.x * v.x + v.y * v.y);
    return len < 1e-9 ? ge::Vec2{} : ge::Vec2{v.x / len, v.y / len};
}

Enemy make_enemy(ge::EnemyKind kind) noexcept {
    const EnemyDef& d = enemy_def(kind);
    Enemy e;
    e.kind = kind;
    e.speed = d.speed;
    e.sight = d.sight;
    e.attack_range = d.melee_range;
    e.attack_interval = d.melee_interval;
    e.damage = d.melee_damage;
    e.radius = d.radius;
    e.ranged = d.ranged;
    e.min_range = d.min_range;
    e.max_range = d.max_range;
    e.shoot_interval = d.shoot_interval;
    e.shot_damage = d.shot_damage;
    e.shot_speed = d.shot_speed;
    return e;
}

EnemyDecision enemy_think(Enemy& e, ge::Vec2 pos, ge::Vec2 player, const ge::GridMap& map, const DistanceField& field,
                          bool alerted, double dt) noexcept {
    EnemyDecision out;
    e.attack_cooldown = e.attack_cooldown > dt ? e.attack_cooldown - dt : 0.0;
    const ge::Vec2 to = player - pos;
    const double dist = std::sqrt(to.x * to.x + to.y * to.y);
    const bool los = line_of_sight(map, pos, player);

    if (e.ranged) {
        // Gunner: wake up like everyone else, then walk until the player is in range with a clear line, stand
        // there sidestepping and firing, and back away if the player closes in.
        e.shoot_cooldown = e.shoot_cooldown > dt ? e.shoot_cooldown - dt : 0.0;
        e.strafe_timer -= dt;
        if (e.strafe_timer <= 0.0) { e.strafe_sign = -e.strafe_sign; e.strafe_timer = 1.1; }
        if (e.state == EnemyState::Idle && ((los && dist <= e.sight) || alerted)) e.state = EnemyState::Chase;
        if (e.state != EnemyState::Idle) e.state = (los && dist <= e.max_range) ? EnemyState::Attack : EnemyState::Chase;

        if (e.state == EnemyState::Chase) {
            ge::Vec2 dir{};
            if (los && dist < 6.0 && dist > 1e-9) dir = {to.x / dist, to.y / dist};
            else dir = field.direction_from(pos);
            if (dir.x == 0.0 && dir.y == 0.0 && dist > 1e-9) dir = {to.x / dist, to.y / dist};
            out.move = {dir.x * e.speed * dt, dir.y * e.speed * dt};
        } else if (e.state == EnemyState::Attack && dist > 1e-9) {
            const ge::Vec2 toward{to.x / dist, to.y / dist};
            if (dist < e.min_range) out.move = {-toward.x * e.speed * dt, -toward.y * e.speed * dt};  // back away
            else out.move = {-toward.y * e.strafe_sign * e.speed * 0.5 * dt, toward.x * e.strafe_sign * e.speed * 0.5 * dt};
            if (e.shoot_cooldown <= 0.0) {
                out.fire = true;
                out.aim = toward;
                e.shoot_cooldown = e.shoot_interval;
            }
        }
        return out;
    }

    if (e.state == EnemyState::Idle && ((los && dist <= e.sight) || alerted)) e.state = EnemyState::Chase;
    if (e.state == EnemyState::Chase && los && dist <= e.attack_range) e.state = EnemyState::Attack;
    else if (e.state == EnemyState::Attack && (!los || dist > e.attack_range * 1.35)) e.state = EnemyState::Chase;

    if (e.state == EnemyState::Chase) {
        ge::Vec2 dir{};
        if (los && dist < 6.0 && dist > 1e-9) dir = {to.x / dist, to.y / dist};  // close and visible: head straight in
        else dir = field.direction_from(pos);
        if (dir.x == 0.0 && dir.y == 0.0 && dist > 1e-9) dir = {to.x / dist, to.y / dist};  // same cell as the goal
        out.move = {dir.x * e.speed * dt, dir.y * e.speed * dt};
    } else if (e.state == EnemyState::Attack && e.attack_cooldown <= 0.0) {
        out.attack = true;
        e.attack_cooldown = e.attack_interval;
    }
    return out;
}

ge::Vec2 slide_move(const ge::GridMap& map, ge::Vec2 p, ge::Vec2 delta, double half) noexcept {
    ge::Vec2 next{p.x + delta.x, p.y};
    if (!ge::collides_aabb(map, next, half)) p.x = next.x;
    next = {p.x, p.y + delta.y};
    if (!ge::collides_aabb(map, next, half)) p.y = next.y;
    return p;
}

}  // namespace game
