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
constexpr double kFeedSeconds = 2.5;
constexpr std::size_t kMaxFeed = 4;
}  // namespace

GameApplication::GameApplication(ge::Level level) : level_(std::move(level)) {
    player_.position = level_.spawn;
    player_.angle = level_.spawn_angle;
    subscribe_feed();
}

void GameApplication::subscribe_feed() {
    events_.subscribe<TargetDestroyed>([this](const TargetDestroyed& e) { add_feed("TARGET DESTROYED +" + std::to_string(e.points)); });
    events_.subscribe<WaveStarted>([this](const WaveStarted& e) { add_feed("WAVE " + std::to_string(e.wave) + " - " + std::to_string(e.targets) + " TARGETS"); });
    events_.subscribe<WaveCleared>([this](const WaveCleared& e) { add_feed("WAVE CLEARED +" + std::to_string(e.bonus)); });
    events_.subscribe<ReloadStarted>([this](const ReloadStarted&) { add_feed("RELOADING"); });
}

void GameApplication::add_feed(std::string text) {
    feed_.push_back({std::move(text), kFeedSeconds});
    while (feed_.size() > kMaxFeed) feed_.pop_front();
}

ge::Status GameApplication::initialize(const ge::EngineConfig& config) {
    if (level_.map.width() <= 0 || level_.map.height() <= 0) return ge::Status::failure("level map is empty");
    if (ge::collides_aabb(level_.map, player_.position, player_.half_extent))
        return ge::Status::failure("player spawn is inside a wall");
    fov_degrees_ = config.fov_degrees;
    player_.mouse_sensitivity = config.mouse_sensitivity;
    if (config.textured) {
        textures_ = ge::TextureSet::make_procedural();
        renderer_.set_textures(&textures_);
    }
    start_in_menu_ = config.start_in_menu;
    state_ = start_in_menu_ ? GameState::Menu : GameState::Playing;
    start_wave(1);
    return ge::Status::success();
}

void GameApplication::set_state(GameState next) {
    if (next == state_) return;
    const GameState from = state_;
    state_ = next;
    events_.publish(StateChanged{from, next});
}

void GameApplication::restart() {
    while (!targets_.empty()) kill_target(targets_.size() - 1);
    player_.position = level_.spawn;
    player_.angle = level_.spawn_angle;
    weapon_ = Weapon{};
    score_ = 0;
    shots_ = hits_ = kills_ = 0;
    hit_marker_ = 0.0;
    feed_.clear();
    start_wave(1);
}

void GameApplication::start_wave(int wave) {
    wave_ = wave;
    between_waves_ = false;
    spawn_targets(static_cast<std::size_t>(6 + 2 * wave), 1.0 + 0.15 * (wave - 1));
    events_.publish(WaveStarted{wave, targets_.size()});
}

void GameApplication::spawn_targets(std::size_t wanted, double speed_scale) {
    auto add_target = [&](ge::Vec2 p) {
        const double i = static_cast<double>(targets_.size());
        const ge::Vec2 v{(0.6 + 0.1 * i) * speed_scale, (0.45 - 0.07 * i) * speed_scale};
        const ge::Entity e = entities_.create();
        entities_.add(e, ge::Transform{p, 0.0});
        entities_.add(e, ge::Health{});
        entities_.add(e, ge::Collider{kTargetRadius});
        motion_.add(p, v);
        targets_.push_back(e);
        flash_.push_back(0.0);
    };

    // Level-designed target spawns ('T' in the map) come first.
    for (const ge::Vec2& p : level_.target_spawns) {
        if (targets_.size() >= wanted) break;
        add_target(p);
    }

    // Deterministic fill: walk the open cells in row order and take every 4th one that is not
    // next to the player, so the same level and player position always yield the same targets.
    std::size_t seen = 0;
    for (int y = 0; y < level_.map.height() && targets_.size() < wanted; ++y) {
        for (int x = 0; x < level_.map.width() && targets_.size() < wanted; ++x) {
            if (level_.map.is_solid(x, y)) continue;
            const ge::Vec2 p{x + 0.5, y + 0.5};
            const ge::Vec2 d = p - player_.position;
            if (d.x * d.x + d.y * d.y < 2.0 || (seen++ % 4) != 0) continue;
            add_target(p);
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
    events_.publish(ShotFired{weapon_.ammo});
    std::vector<ShotTarget> shot_targets;
    shot_targets.reserve(targets_.size());
    for (std::size_t i = 0; i < targets_.size(); ++i) shot_targets.push_back({motion_.position(i), kTargetRadius * kHitRadiusScale});

    const ShotResult r = hitscan(level_.map, player_.position, player_.angle, shot_targets);
    if (r.target < 0) return;
    const auto i = static_cast<std::size_t>(r.target);
    ++hits_;
    hit_marker_ = 0.15;
    flash_[i] = 1.0;
    const ge::Vec2 where = motion_.position(i);
    bool killed = false;
    if (auto* hp = entities_.get<ge::Health>(targets_[i])) {
        hp->current -= weapon_.damage;
        killed = hp->dead();
    }
    events_.publish(TargetHit{where, weapon_.damage, killed});
    if (killed) {
        ++kills_;
        score_ += kPointsPerKill;
        kill_target(i);
        events_.publish(TargetDestroyed{where, kPointsPerKill});
    }
}

void GameApplication::fixed_update(const ge::InputState& input, double dt) {
    struct TickOnExit {
        ge::EventBus& bus;
        double dt;
        ~TickOnExit() { bus.publish(FixedTick{dt}); }
    } tick_on_exit{events_, dt};
    if (input.quit) {
        set_state(GameState::Quit);
        return;
    }
    if (input.toggle_debug) debug_ = !debug_;
    for (FeedMessage& m : feed_) m.ttl -= dt;
    while (!feed_.empty() && feed_.front().ttl <= 0.0) feed_.pop_front();

    switch (state_) {
        case GameState::Menu:
            if (input.menu_quit) set_state(GameState::Quit);
            else if (input.confirm) { restart(); set_state(GameState::Playing); }
            break;
        case GameState::Playing:
            if (input.pause) { set_state(GameState::Paused); break; }
            update_playing(input, dt);
            break;
        case GameState::Paused:
            if (input.menu_quit) set_state(GameState::Quit);
            else if (input.pause || input.confirm) set_state(GameState::Playing);
            break;
        case GameState::Victory:
            if (input.menu_quit) set_state(GameState::Quit);
            else if (input.confirm) { restart(); set_state(GameState::Playing); }
            break;
        case GameState::Quit:
            break;
    }
}

void GameApplication::update_playing(const ge::InputState& input, double dt) {
    update_player(player_, input, level_.map, dt);

    const bool was_reloading = weapon_.reloading();
    if (weapon_step(weapon_, input.fire, input.reload, dt)) fire_shot();
    if (!was_reloading && weapon_.reloading()) events_.publish(ReloadStarted{});
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
            events_.publish(WaveCleared{wave_, kPointsPerWaveBonus});
            if (wave_ >= level_.waves_to_win) {
                events_.publish(GameWon{score_, shots_, hits_});
                set_state(GameState::Victory);
            }
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
    hud.waves_to_win = level_.waves_to_win;
    hud.targets_left = static_cast<int>(targets_.size());
    hud.between_waves = between_waves_ && state_ != GameState::Victory;  // the win screen replaces the banner
    hud.debug = debug_;
    hud.frame_ms = frame_ms_;
    hud.fps = frame_ms_ > 0.0 ? 1000.0 / frame_ms_ : 0.0;
    hud.entities = entities_.size();
    hud.backend = ge::opt::asm_enabled() ? "NASM" : "C++";
    hud.level_name = level_.name.c_str();
    for (const FeedMessage& m : feed_) hud.feed.push_back(m.text);

    draw_weapon_view(target, weapon_.flash, weapon_.reloading());
    draw_hud(target, hud);

    switch (state_) {
        case GameState::Menu: draw_menu_screen(target, hud); break;
        case GameState::Paused: draw_pause_screen(target); break;
        case GameState::Victory:
            hud.shots = shots_;
            hud.hits = hits_;
            draw_victory_screen(target, hud);
            break;
        default: break;
    }
}

}  // namespace game
