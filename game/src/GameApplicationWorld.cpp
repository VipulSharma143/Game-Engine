// Enemies, pickups and doors: the parts of GameApplication that manage world entities.

#include <algorithm>
#include <cmath>

#include "engine/renderer/Raycaster.hpp"
#include "game/Components.hpp"
#include "game/GameApplication.hpp"

namespace game {

namespace {
constexpr int kEnemyHealth = 60;           // two shots at 34 damage
constexpr double kPickupRadius = 0.55;
constexpr int kHealthPackAmount = 25;
constexpr double kUseDistance = 1.6;       // how far away a door can be opened
}  // namespace

void GameApplication::spawn_level_content() {
    for (const ge::PickupSpawn& p : level_.pickups) place_pickup(p.kind, p.position);
}

void GameApplication::spawn_enemies(int wave) {
    const std::size_t count = std::min(level_.enemy_spawns.size(), static_cast<std::size_t>(wave));
    for (std::size_t i = 0; i < count; ++i) place_enemy(level_.enemy_spawns[i]);
}

void GameApplication::place_enemy(ge::Vec2 position) {
    const ge::Entity e = entities_.create();
    Enemy brain;
    entities_.add(e, ge::Transform{position, 0.0});
    entities_.add(e, ge::Health{kEnemyHealth, kEnemyHealth});
    entities_.add(e, ge::Collider{brain.radius});
    entities_.add(e, brain);
    enemies_.push_back(e);
}

void GameApplication::place_pickup(ge::PickupKind kind, ge::Vec2 position) {
    const ge::Entity e = entities_.create();
    entities_.add(e, ge::Transform{position, 0.0});
    entities_.add(e, Pickup{kind});
    pickups_.push_back(e);
}

ge::Vec2 GameApplication::enemy_position(std::size_t i) const {
    // EntityWorld::get is non-const; look the transform up through a const-correct copy of the handle.
    return const_cast<ge::EntityWorld&>(entities_).get<ge::Transform>(enemies_.at(i))->position;
}

EnemyState GameApplication::enemy_state(std::size_t i) const {
    return const_cast<ge::EntityWorld&>(entities_).get<Enemy>(enemies_.at(i))->state;
}

void GameApplication::kill_enemy(std::size_t i) {
    entities_.destroy(enemies_[i]);
    enemies_.erase(enemies_.begin() + static_cast<std::ptrdiff_t>(i));
}

void GameApplication::remove_pickup(std::size_t i) {
    entities_.destroy(pickups_[i]);
    pickups_.erase(pickups_.begin() + static_cast<std::ptrdiff_t>(i));
}

void GameApplication::damage_player(int amount) {
    if (player_.health <= 0) return;
    player_.health = std::max(0, player_.health - amount);
    hurt_flash_ = 1.0;
    events_.publish(PlayerHit{amount, player_.health});
    if (player_.health == 0) {
        events_.publish(PlayerDied{score_});
        set_state(GameState::GameOver);
    }
}

void GameApplication::update_enemies(double dt) {
    if (enemies_.empty()) return;

    // Re-plan only when the player changes cell or a door changed the map.
    const int px = static_cast<int>(std::floor(player_.position.x));
    const int py = static_cast<int>(std::floor(player_.position.y));
    if (field_version_ != map_version_ || field_.goal_x() != px || field_.goal_y() != py) {
        field_.rebuild(level_.map, px, py);
        field_version_ = map_version_;
    }

    const bool alerted = alert_timer_ > 0.0;
    for (std::size_t i = 0; i < enemies_.size(); ++i) {
        auto* t = entities_.get<ge::Transform>(enemies_[i]);
        auto* en = entities_.get<Enemy>(enemies_[i]);
        en->flash = std::max(0.0, en->flash - dt * 5.0);
        const EnemyDecision d = enemy_think(*en, t->position, player_.position, level_.map, field_, alerted, dt);
        t->position = slide_move(level_.map, t->position, d.move, en->radius);
        if (d.attack) damage_player(en->damage);
        if (state_ != GameState::Playing) return;
    }

    // Keep enemies from stacking into one blob: push overlapping pairs apart.
    for (std::size_t i = 0; i < enemies_.size(); ++i)
        for (std::size_t j = i + 1; j < enemies_.size(); ++j) {
            auto* a = entities_.get<ge::Transform>(enemies_[i]);
            auto* b = entities_.get<ge::Transform>(enemies_[j]);
            const double dx = b->position.x - a->position.x, dy = b->position.y - a->position.y;
            const double dist = std::sqrt(dx * dx + dy * dy);
            const double min_dist = 0.55;
            if (dist >= min_dist) continue;
            const double nx = dist > 1e-6 ? dx / dist : 1.0, ny = dist > 1e-6 ? dy / dist : 0.0;
            const double push = (min_dist - dist) * 0.5;
            a->position = slide_move(level_.map, a->position, {-nx * push, -ny * push}, 0.3);
            b->position = slide_move(level_.map, b->position, {nx * push, ny * push}, 0.3);
        }
}

void GameApplication::update_pickups() {
    for (std::size_t i = 0; i < pickups_.size();) {
        const auto* t = entities_.get<ge::Transform>(pickups_[i]);
        const auto* pk = entities_.get<Pickup>(pickups_[i]);
        const double dx = t->position.x - player_.position.x, dy = t->position.y - player_.position.y;
        bool take = false;
        if (dx * dx + dy * dy <= kPickupRadius * kPickupRadius) {
            switch (pk->kind) {
                case ge::PickupKind::Health:
                    if (player_.health < player_.max_health) {  // a full-health player leaves it for later
                        player_.health = std::min(player_.max_health, player_.health + kHealthPackAmount);
                        take = true;
                    }
                    break;
                case ge::PickupKind::Ammo:
                    if (weapon_.ammo < weapon_.magazine || weapon_.reloading()) {
                        weapon_.ammo = weapon_.magazine;
                        weapon_.reload_left = 0.0;
                        take = true;
                    }
                    break;
                case ge::PickupKind::Key:
                    ++player_.keys;
                    take = true;
                    break;
            }
        }
        if (take) {
            events_.publish(PickupCollected{pk->kind, t->position});
            remove_pickup(i);
        } else {
            ++i;
        }
    }
}

void GameApplication::try_use_door() {
    const ge::RayHit hit = ge::cast_ray(level_.map, player_.position, ge::heading_vector(player_.angle));
    if (!hit.hit || hit.distance > kUseDistance) return;
    if (hit.wall_id != ge::kDoorId && hit.wall_id != ge::kLockedDoorId) return;
    if (hit.wall_id == ge::kLockedDoorId) {
        if (player_.keys <= 0) {
            events_.publish(DoorLocked{hit.cell_x, hit.cell_y});
            add_feed("LOCKED - NEED A KEY");
            return;
        }
        --player_.keys;
    }
    level_.map.set_cell(hit.cell_x, hit.cell_y, 0);
    ++map_version_;
    events_.publish(DoorOpened{hit.cell_x, hit.cell_y});
}

}  // namespace game
