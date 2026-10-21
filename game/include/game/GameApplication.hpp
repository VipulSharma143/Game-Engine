#pragma once

#include <vector>

#include "engine/core/Application.hpp"
#include "engine/physics/Collision.hpp"
#include "engine/renderer/Renderer.hpp"
#include "engine/renderer/SpriteRenderer.hpp"
#include "engine/world/MotionBatch.hpp"
#include "engine/world/World.hpp"
#include "engine/world/WorldSystem.hpp"
#include "game/PlayerController.hpp"
#include "game/WeaponSystem.hpp"

namespace game {

enum class GameState { Playing, Quit };

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

    const Player& player() const noexcept { return player_; }
    GameState state() const noexcept { return state_; }
    Player& mutable_player() noexcept { return player_; }

    // Drifting target entities (Transform + Health + Collider), moved in bulk each fixed step.
    std::size_t target_count() const noexcept { return targets_.size(); }
    ge::Vec2 target_position(std::size_t i) const noexcept { return motion_.position(i); }
    const ge::EntityWorld& entities() const noexcept { return entities_; }
    std::uint64_t target_checksum() const noexcept { return motion_.checksum(); }

    // Gameplay state
    const Weapon& weapon() const noexcept { return weapon_; }
    int score() const noexcept { return score_; }
    int wave() const noexcept { return wave_; }
    unsigned shots_fired() const noexcept { return shots_; }
    unsigned shots_hit() const noexcept { return hits_; }
    unsigned targets_destroyed() const noexcept { return kills_; }
    // Test hook: replace all targets with one stationary target at `position`.
    void place_single_target(ge::Vec2 position);

private:
    ge::Level level_;
    Player player_;
    ge::TextureSet textures_;
    ge::Renderer renderer_;
    ge::EntityWorld entities_;
    ge::MotionBatch motion_;
    std::vector<ge::Entity> targets_;  // targets_[i] <-> motion_ point i <-> flash_[i]
    std::vector<double> flash_;
    std::vector<ge::Sprite> sprites_;  // reused every frame
    Weapon weapon_;
    double fov_degrees_ = 66.0;
    GameState state_ = GameState::Playing;
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
    void spawn_targets(std::size_t wanted, double speed_scale);
    void fire_shot();
    void kill_target(std::size_t index);
};

}  // namespace game
