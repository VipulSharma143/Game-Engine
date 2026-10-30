#pragma once

#include <array>
#include <deque>
#include <string>
#include <vector>

#include "engine/core/Application.hpp"
#include "engine/core/EventBus.hpp"
#include "engine/physics/Collision.hpp"
#include "engine/renderer/Particles.hpp"
#include "engine/renderer/Renderer.hpp"
#include "engine/renderer/SpriteRenderer.hpp"
#include "engine/world/MotionBatch.hpp"
#include "engine/world/World.hpp"
#include "engine/world/WorldSystem.hpp"
#include "game/Definitions.hpp"
#include "game/Enemy.hpp"
#include "game/GameEvents.hpp"
#include "game/PlayerController.hpp"
#include "game/Projectiles.hpp"
#include "game/SpriteArt.hpp"
#include "game/WeaponSystem.hpp"

namespace game {

// The playable scene: walk the level and shoot the drifting targets, wave after wave.
class GameApplication final : public ge::Application {
public:
    explicit GameApplication(ge::Level level);
    GameApplication(const GameApplication&) = delete;             // the renderer points at textures_
    GameApplication& operator=(const GameApplication&) = delete;

    ge::Status initialize(const ge::EngineConfig& config) override;
    void fixed_update(const ge::InputState& input, double dt) override;
    void render(ge::Framebuffer& target) override;
    void shutdown() noexcept override {}
    bool wants_quit() const noexcept override { return state_ == GameState::Quit; }
    bool wants_mouse_capture() const noexcept override { return state_ == GameState::Playing; }

    const Player& player() const noexcept { return player_; }
    GameState state() const noexcept { return state_; }
    Player& mutable_player() noexcept { return player_; }
    const ge::Level& level() const noexcept { return level_; }
    ge::Camera camera() const { return camera_for(player_, fov_degrees_); }
    ge::EventBus& events() noexcept { return events_; }

    // Starts a fresh run: wave 1, full ammo, player back at the spawn. Does not change the state.
    void restart();

    // Drifting target entities (Transform + Health + Collider), moved in bulk each fixed step.
    std::size_t target_count() const noexcept { return targets_.size(); }
    ge::Vec2 target_position(std::size_t i) const noexcept { return motion_.position(i); }
    const ge::EntityWorld& entities() const noexcept { return entities_; }
    std::uint64_t target_checksum() const noexcept { return motion_.checksum(); }

    // Gameplay state
    const Weapon& weapon() const noexcept { return weapons_[static_cast<std::size_t>(current_weapon_)]; }  // the one in hand
    WeaponId weapon_id() const noexcept { return static_cast<WeaponId>(current_weapon_); }
    const Weapon& weapon_state(WeaponId id) const noexcept { return weapons_[static_cast<std::size_t>(id)]; }
    bool owns_weapon(WeaponId id) const noexcept { return ((owned_weapons_ >> static_cast<unsigned>(id)) & 1u) != 0; }
    std::size_t projectile_count() const noexcept { return projectiles_.size(); }
    const ge::ParticleSystem& particles() const noexcept { return particles_; }
    ge::EnemyKind enemy_kind(std::size_t i) const;
    int enemy_health(std::size_t i) const;
    int score() const noexcept { return score_; }
    int wave() const noexcept { return wave_; }
    unsigned shots_fired() const noexcept { return shots_; }
    unsigned shots_hit() const noexcept { return hits_; }
    unsigned targets_destroyed() const noexcept { return kills_; }
    std::size_t enemy_count() const noexcept { return enemies_.size(); }
    std::size_t pickup_count() const noexcept { return pickups_.size(); }
    ge::Vec2 enemy_position(std::size_t i) const;
    EnemyState enemy_state(std::size_t i) const;
    unsigned enemies_killed() const noexcept { return enemy_kills_; }
    int player_health() const noexcept { return player_.health; }
    int player_keys() const noexcept { return player_.keys; }

    // Picking up a weapon: owning it, its starting ammo (or a top-up if already owned), and switching to it.
    void give_weapon(WeaponId id);
    // Puts the weapon in hand; false if it is not owned.
    bool select_weapon(WeaponId id);

    // Test hooks. place_single_target replaces all targets AND enemies with one stationary target.
    void place_single_target(ge::Vec2 position);
    void place_enemy(ge::Vec2 position, ge::EnemyKind kind = ge::EnemyKind::Grunt);
    void place_pickup(ge::PickupKind kind, ge::Vec2 position);

private:
    ge::Level level_;
    ge::Level initial_level_;          // pristine copy: restart() restores doors and pickups from it
    Player player_;
    ge::TextureSet textures_;
    ge::Renderer renderer_;
    ge::EntityWorld entities_;
    ge::MotionBatch motion_;
    std::vector<ge::Entity> targets_;  // targets_[i] <-> motion_ point i <-> flash_[i]
    std::vector<double> flash_;
    std::vector<ge::Entity> enemies_;  // Transform + Health + Collider + Enemy
    std::vector<ge::Entity> pickups_;  // Transform + Pickup
    DistanceField field_;              // distances to the player's cell, for enemy pathing
    unsigned map_version_ = 0;         // bumped when a door opens
    unsigned field_version_ = ~0u;
    double hurt_flash_ = 0.0;
    double alert_timer_ = 0.0;         // > 0: gunfire was heard, idle enemies wake up
    unsigned enemy_kills_ = 0;
    std::vector<ge::Sprite> sprites_;  // reused every frame
    std::array<Weapon, kWeaponCount> weapons_;
    unsigned owned_weapons_ = 1;       // bit per WeaponId; the pistol from the start
    int current_weapon_ = 0;
    std::vector<Projectile> projectiles_;
    ge::ParticleSystem particles_;
    SpriteArt art_ = SpriteArt::make();
    std::vector<ShotTarget> shot_scratch_;  // reused by every shot
    std::uint32_t spread_state_ = 0x9E3779B9u;  // deterministic spread for shotgun pellets and chaingun jitter
    double anim_time_ = 0.0;           // walking animation clock
    double blast_flash_ = 0.0;         // 0..1 warm screen flash after an explosion nearby
    ge::EventBus events_;
    struct FeedMessage { std::string text; double ttl; };
    std::deque<FeedMessage> feed_;      // short-lived messages shown at the bottom right
    double fov_degrees_ = 66.0;
    GameState state_ = GameState::Playing;
    bool start_in_menu_ = false;
    int score_ = 0;
    int wave_ = 0;
    unsigned shots_ = 0, hits_ = 0, kills_ = 0;
    bool between_waves_ = false;
    double wave_timer_ = 0.0;
    double hit_marker_ = 0.0;
    bool debug_ = false;
    double last_render_time_ = 0.0;
    double frame_ms_ = 0.0;

    void start_wave(int wave);
    void spawn_level_content();        // pickups from the level
    void spawn_enemies(int wave);
    void update_enemies(double dt);
    void update_pickups();
    void try_use_door();
    void damage_player(int amount);
    void kill_enemy(std::size_t index);
    void remove_pickup(std::size_t index);
    void spawn_targets(std::size_t wanted, double speed_scale);
    void reset_weapons();
    void fire_weapon();
    double spread_random() noexcept;                 // [-1, 1)
    bool damage_thing(std::size_t combined, int damage, ge::Vec2 where);  // targets first, then enemies; true if killed
    void build_shot_targets();
    void update_projectiles(double dt);
    int explode(ge::Vec2 position, double radius, int damage);  // returns how many things it damaged
    void spawn_enemy_bolt(std::size_t enemy_index, ge::Vec2 aim);
    void handle_weapon_input(const ge::InputState& input);
    void effect_sparks(ge::Vec2 where, double z, int count);
    void effect_blood(ge::Vec2 where, int count);
    void kill_target(std::size_t index);
    void set_state(GameState next);
    void update_playing(const ge::InputState& input, double dt);
    void add_feed(std::string text);
    void subscribe_feed();
};

}  // namespace game
