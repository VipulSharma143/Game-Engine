#include "game/GameApplication.hpp"

#include <algorithm>

#include "engine/core/Time.hpp"
#include "engine/optimized/OptimizedRoutines.hpp"
#include "game/Hud.hpp"

namespace game {

namespace {
constexpr double kTargetRadius = 0.2;      // collider radius; the drawn sphere is 2.5x that in diameter
constexpr double kHitRadiusScale = 1.25;   // shots hit the visible sphere, not just the collider
constexpr int kPointsPerKill = 100;
constexpr int kPointsPerWaveBonus = 250;
constexpr double kBetweenWaves = 2.0;
}  // namespace

GameApplication::GameApplication(ge::Level level) : level_(std::move(level)) {
    player_.position = level_.spawn;
    player_.angle = level_.spawn_angle;
}

ge::Status GameApplication::initialize(const ge::EngineConfig& config) {
    if (level_.map.width() <= 0 || level_.map.height() <= 0) return ge::Status::failure("level map is empty");
    if (ge::collides_aabb(level_.map, player_.position, player_.half_extent))
        return ge::Status::failure("player spawn is inside a wall");
    fov_degrees_ = config.fov_degrees;
    start_wave(1);
    return ge::Status::success();
}

void GameApplication::start_wave(int wave) {
    wave_ = wave;
    between_waves_ = false;
    spawn_targets(static_cast<std::size_t>(6 + 2 * wave), 1.0 + 0.15 * (wave - 1));
}

void GameApplication::spawn_targets(std::size_t wanted, double speed_scale) {
    // Deterministic: walk the open cells in row order and take every 4th one that is not
    // next to the player, so the same level and player position always yield the same targets.
    std::size_t seen = 0;
    for (int y = 0; y < level_.map.height() && targets_.size() < wanted; ++y) {
        for (int x = 0; x < level_.map.width() && targets_.size() < wanted; ++x) {
            if (level_.map.is_solid(x, y)) continue;
            const ge::Vec2 p{x + 0.5, y + 0.5};
            const ge::Vec2 d = p - player_.position;
            if (d.x * d.x + d.y * d.y < 2.0 || (seen++ % 4) != 0) continue;
            const double i = static_cast<double>(targets_.size());
            const ge::Vec2 v{(0.6 + 0.1 * i) * speed_scale, (0.45 - 0.07 * i) * speed_scale};
            const ge::Entity e = entities_.create();
            entities_.add(e, ge::Transform{p, 0.0});
            entities_.add(e, ge::Health{});
            entities_.add(e, ge::Collider{kTargetRadius});
            motion_.add(p, v);
            targets_.push_back(e);
            flash_.push_back(0.0);
        }
    }
}

void GameApplication::place_single_target(ge::Vec2 position) {
    while (!targets_.empty()) kill_target(targets_.size() - 1);
    kills_ = 0;
    score_ = 0;
    const ge::Entity e = entities_.create();
    entities_.add(e, ge::Transform{position, 0.0});
    entities_.add(e, ge::Health{});
    entities_.add(e, ge::Collider{kTargetRadius});
    motion_.add(position, {0.0, 0.0});
    targets_.push_back(e);
    flash_.push_back(0.0);
    between_waves_ = false;
}

void GameApplication::kill_target(std::size_t i) {
    entities_.destroy(targets_[i]);
    motion_.remove(i);
    targets_[i] = targets_.back();
    flash_[i] = flash_.back();
    targets_.pop_back();
    flash_.pop_back();
}

void GameApplication::fire_shot() {
    ++shots_;
    std::vector<ShotTarget> shot_targets;
    shot_targets.reserve(targets_.size());
    for (std::size_t i = 0; i < targets_.size(); ++i) shot_targets.push_back({motion_.position(i), kTargetRadius * kHitRadiusScale});

    const ShotResult r = hitscan(level_.map, player_.position, player_.angle, shot_targets);
    if (r.target < 0) return;
    const auto i = static_cast<std::size_t>(r.target);
    ++hits_;
    hit_marker_ = 0.15;
    flash_[i] = 1.0;
    if (auto* hp = entities_.get<ge::Health>(targets_[i])) {
        hp->current -= weapon_.damage;
        if (hp->dead()) {
            ++kills_;
            score_ += kPointsPerKill;
            kill_target(i);
        }
    }
}

void GameApplication::fixed_update(const ge::InputState& input, double dt) {
    if (input.quit) {
        state_ = GameState::Quit;
        return;
    }
    if (input.toggle_debug) debug_ = !debug_;
    update_player(player_, input, level_.map, dt);

    if (weapon_step(weapon_, input.fire, input.reload, dt)) fire_shot();
    hit_marker_ = std::max(0.0, hit_marker_ - dt);
    for (double& f : flash_) f = std::max(0.0, f - dt * 5.0);

    motion_.step(dt, level_.map);
    for (std::size_t i = 0; i < targets_.size(); ++i)
        if (auto* t = entities_.get<ge::Transform>(targets_[i])) t->position = motion_.position(i);

    if (targets_.empty()) {
        if (!between_waves_) {
            between_waves_ = true;
            wave_timer_ = kBetweenWaves;
            score_ += kPointsPerWaveBonus;
        } else {
            wave_timer_ -= dt;
            if (wave_timer_ <= 0.0) start_wave(wave_ + 1);
        }
    }
}

void GameApplication::render(ge::Framebuffer& target) {
    const ge::Camera cam = camera_for(player_, fov_degrees_);
    renderer_.render(level_.map, cam, target);

    sprites_.clear();
    for (std::size_t i = 0; i < targets_.size(); ++i) {
        const auto* hp = entities_.get<ge::Health>(targets_[i]);
        const double t = hp ? static_cast<double>(hp->current) / static_cast<double>(hp->max) : 1.0;
        // Healthy = cyan, damaged = orange.
        const auto lerp = [t](unsigned a, unsigned b) { return static_cast<unsigned>(a + (b - a) * t); };
        sprites_.push_back({motion_.position(i), kTargetRadius * 2.5,
                            ge::pack_rgb(lerp(255, 60), lerp(120, 220), lerp(40, 230)), flash_[i]});
    }
    ge::draw_sprites(cam, renderer_.depth(), sprites_, target);

    // Frame timing is only used by the optional debug overlay.
    const double now = ge::monotonic_seconds();
    if (last_render_time_ > 0.0) frame_ms_ = (now - last_render_time_) * 1000.0;
    last_render_time_ = now;

    HudData hud;
    hud.ammo = weapon_.ammo;
    hud.magazine = weapon_.magazine;
    hud.reloading = weapon_.reloading();
    hud.reload_fraction = hud.reloading ? 1.0 - weapon_.reload_left / weapon_.reload_time : 0.0;
    hud.weapon_flash = weapon_.flash;
    hud.hit_marker = hit_marker_ > 0.0 ? 1.0 : 0.0;
    hud.score = score_;
    hud.wave = wave_;
    hud.targets_left = static_cast<int>(targets_.size());
    hud.between_waves = between_waves_;
    hud.debug = debug_;
    hud.frame_ms = frame_ms_;
    hud.fps = frame_ms_ > 0.0 ? 1000.0 / frame_ms_ : 0.0;
    hud.entities = entities_.size();
    hud.backend = ge::opt::asm_enabled() ? "NASM" : "C++";
    draw_weapon_view(target, weapon_.flash, weapon_.reloading());
    draw_hud(target, hud);
}

}  // namespace game
