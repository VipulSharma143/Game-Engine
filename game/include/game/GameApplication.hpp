#pragma once

#include "engine/core/Application.hpp"
#include "engine/renderer/Renderer.hpp"
#include "engine/world/MotionBatch.hpp"
#include "engine/world/World.hpp"
#include "engine/world/WorldSystem.hpp"
#include "engine/physics/Collision.hpp"
#include "game/PlayerController.hpp"

#include <vector>

namespace game {

enum class GameState { Playing, Quit };

// The playable test scene: walk around the built-in level.
class GameApplication final : public ge::Application {
public:
    explicit GameApplication(ge::Level level);

    ge::Status initialize(const ge::EngineConfig& config) override;
    void fixed_update(const ge::InputState& input, double dt) override;
    void render(ge::Framebuffer& target) override;
    void shutdown() noexcept override {}
    bool wants_quit() const noexcept override { return state_ == GameState::Quit; }

    const Player& player() const noexcept { return player_; }
    GameState state() const noexcept { return state_; }

    // Drifting target entities (Transform + Health + Collider), moved in bulk each fixed step.
    std::size_t target_count() const noexcept { return targets_.size(); }
    ge::Vec2 target_position(std::size_t i) const noexcept { return motion_.position(i); }
    const ge::EntityWorld& entities() const noexcept { return entities_; }
    std::uint64_t target_checksum() const noexcept { return motion_.checksum(); }

private:
    ge::Level level_;
    Player player_;
    ge::Renderer renderer_;
    ge::EntityWorld entities_;
    ge::MotionBatch motion_;
    std::vector<ge::Entity> targets_;  // targets_[i] <-> motion_ point i
    double fov_degrees_ = 66.0;
    GameState state_ = GameState::Playing;

    void spawn_targets(std::size_t wanted);
};

}  // namespace game
