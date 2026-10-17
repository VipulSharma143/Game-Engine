#pragma once

#include "engine/core/Application.hpp"
#include "engine/renderer/Renderer.hpp"
#include "engine/world/World.hpp"
#include "engine/physics/Collision.hpp"
#include "game/PlayerController.hpp"

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

private:
    ge::Level level_;
    Player player_;
    ge::Renderer renderer_;
    double fov_degrees_ = 66.0;
    GameState state_ = GameState::Playing;
};

}  // namespace game
