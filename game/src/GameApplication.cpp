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
    spawn_targets(8);
    return ge::Status::success();
}

void GameApplication::spawn_targets(std::size_t wanted) {
    // Deterministic: walk the open cells in row order and take every 4th one that is not
    // next to the player spawn, so the same level always yields the same targets.
    std::size_t seen = 0;
    for (int y = 0; y < level_.map.height() && targets_.size() < wanted; ++y) {
        for (int x = 0; x < level_.map.width() && targets_.size() < wanted; ++x) {
            if (level_.map.is_solid(x, y)) continue;
            const ge::Vec2 p{x + 0.5, y + 0.5};
            const ge::Vec2 d = p - player_.position;
            if (d.x * d.x + d.y * d.y < 2.0 || (seen++ % 4) != 0) continue;
            const std::size_t i = targets_.size();
            const ge::Vec2 v{0.6 + 0.1 * static_cast<double>(i), 0.45 - 0.07 * static_cast<double>(i)};
            const ge::Entity e = entities_.create();
            entities_.add(e, ge::Transform{p, 0.0});
            entities_.add(e, ge::Health{});
            entities_.add(e, ge::Collider{0.2});
            motion_.add(p, v);
            targets_.push_back(e);
        }
    }
}

void GameApplication::fixed_update(const ge::InputState& input, double dt) {
    if (input.quit) {
        state_ = GameState::Quit;
        return;
    }
    update_player(player_, input, level_.map, dt);

    motion_.step(dt, level_.map);
    for (std::size_t i = 0; i < targets_.size(); ++i)
        if (auto* t = entities_.get<ge::Transform>(targets_[i])) t->position = motion_.position(i);
}

void GameApplication::render(ge::Framebuffer& target) {
    renderer_.render(level_.map, camera_for(player_, fov_degrees_), target);
}

}  // namespace game
