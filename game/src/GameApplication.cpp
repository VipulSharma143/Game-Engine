#include "game/GameApplication.hpp"

namespace game {

GameApplication::GameApplication(ge::Level level) : level_(std::move(level)) {
    player_.position = level_.spawn;
    player_.angle = level_.spawn_angle;
}

ge::Status GameApplication::initialize(const ge::EngineConfig& config) {
    if (level_.map.width() <= 0 || level_.map.height() <= 0) return ge::Status::failure("level map is empty");
    if (ge::collides_aabb(level_.map, player_.position, player_.half_extent))
        return ge::Status::failure("player spawn is inside a wall");
    fov_degrees_ = config.fov_degrees;
    return ge::Status::success();
}

void GameApplication::fixed_update(const ge::InputState& input, double dt) {
    if (input.quit) {
        state_ = GameState::Quit;
        return;
    }
    update_player(player_, input, level_.map, dt);
}

void GameApplication::render(ge::Framebuffer& target) {
    renderer_.render(level_.map, camera_for(player_, fov_degrees_), target);
}

}  // namespace game
