#include "game/GameApplication.hpp"

#include <algorithm>

#include <cmath>

#include "engine/core/Time.hpp"
#include "engine/optimized/OptimizedRoutines.hpp"
#include "game/Hud.hpp"
#include "game/Components.hpp"

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

GameApplication::GameApplication(ge::Level level) : level_(std::move(level)), initial_level_(level_) {
    player_.position = level_.spawn;
    player_.angle = level_.spawn_angle;
    reset_weapons();
    subscribe_feed();
}

void GameApplication::subscribe_feed() {
    events_.subscribe<TargetDestroyed>([this](const TargetDestroyed& e) { add_feed("TARGET DESTROYED +" + std::to_string(e.points)); });
    events_.subscribe<WaveStarted>([this](const WaveStarted& e) { add_feed("WAVE " + std::to_string(e.wave) + " - " + std::to_string(e.targets) + " TARGETS"); });
    events_.subscribe<WaveCleared>([this](const WaveCleared& e) { add_feed("WAVE CLEARED +" + std::to_string(e.bonus)); });
    events_.subscribe<ReloadStarted>([this](const ReloadStarted&) { add_feed("RELOADING"); });
    events_.subscribe<WeaponPickedUp>([this](const WeaponPickedUp& e) {
        const WeaponDef& d = weapon_def(static_cast<WeaponId>(e.weapon));
        add_feed(std::string(d.name) + (e.first_time ? " ACQUIRED" : " +" + std::to_string(e.ammo_added)));
    });
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
        textures_ = ge::TextureSet::make_procedural(level_.theme);
        renderer_.set_textures(&textures_);
    }
    start_in_menu_ = config.start_in_menu;
    state_ = start_in_menu_ ? GameState::Menu : GameState::Playing;
    spawn_level_content();
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
    while (!enemies_.empty()) kill_enemy(enemies_.size() - 1);
    while (!pickups_.empty()) remove_pickup(pickups_.size() - 1);
    level_ = initial_level_;  // closed doors again
    ++map_version_;
    const double sensitivity = player_.mouse_sensitivity;
    player_ = Player{};
    player_.mouse_sensitivity = sensitivity;
    player_.position = level_.spawn;
    player_.angle = level_.spawn_angle;
    hurt_flash_ = alert_timer_ = 0.0;
    enemy_kills_ = 0;
    spawn_level_content();
    reset_weapons();
    projectiles_.clear();
    particles_.clear();
    spread_state_ = 0x9E3779B9u;
    blast_flash_ = 0.0;
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
    spawn_enemies(wave);
    events_.publish(WaveStarted{wave, targets_.size() + enemies_.size()});
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
    while (!enemies_.empty()) kill_enemy(enemies_.size() - 1);
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

void GameApplication::reset_weapons() {
    for (int i = 0; i < kWeaponCount; ++i) {
        const WeaponDef& d = weapon_def(static_cast<WeaponId>(i));
        Weapon w;
        w.magazine = d.magazine;
        w.ammo = d.magazine;
        w.reserve = d.start_reserve;
        w.damage = d.damage;
        w.fire_interval = d.fire_interval;
        w.reload_time = d.reload_time;
        weapons_[static_cast<std::size_t>(i)] = w;
    }
    owned_weapons_ = 1u;  // the pistol
    current_weapon_ = 0;
}

double GameApplication::spread_random() noexcept {
    spread_state_ ^= spread_state_ << 13;
    spread_state_ ^= spread_state_ >> 17;
    spread_state_ ^= spread_state_ << 5;
    return static_cast<double>(spread_state_ >> 8) / 8388608.0 - 1.0;
}

void GameApplication::build_shot_targets() {
    shot_scratch_.clear();
    for (std::size_t i = 0; i < targets_.size(); ++i) shot_scratch_.push_back({motion_.position(i), kTargetRadius * kHitRadiusScale});
    for (ge::Entity e : enemies_) {
        const auto* t = entities_.get<ge::Transform>(e);
        const auto* en = entities_.get<Enemy>(e);
        shot_scratch_.push_back({t->position, en->radius});
    }
}

void GameApplication::effect_sparks(ge::Vec2 where, double z, int count) {
    ge::Particle p;
    p.color = ge::pack_rgb(255, 215, 110);
    p.size = 0.045;
    p.life = 0.45;
    p.gravity = 2.2;
    p.glow = true;
    particles_.burst(where, z, count, 2.2, 1.1, p);
}

void GameApplication::effect_blood(ge::Vec2 where, int count) {
    ge::Particle p;
    p.color = ge::pack_rgb(190, 30, 30);
    p.size = 0.06;
    p.life = 0.6;
    p.gravity = 2.6;
    particles_.burst(where, 0.0, count, 1.8, 1.0, p);
}

// Damages the thing at `combined` (drifting targets first, then enemies). Returns true if it died. The lists are
// reshuffled by a death, so callers must not reuse indices afterwards.
bool GameApplication::damage_thing(std::size_t combined, int damage, ge::Vec2 where) {
    if (combined >= targets_.size()) {  // an enemy
        const std::size_t ei = combined - targets_.size();
        auto* en = entities_.get<Enemy>(enemies_[ei]);
        auto* hp = entities_.get<ge::Health>(enemies_[ei]);
        const ge::EnemyKind kind = en->kind;
        en->flash = 1.0;
        if (en->state == EnemyState::Idle) en->state = EnemyState::Chase;  // being hurt wakes it up
        hp->current -= damage;
        const bool killed = hp->dead();
        events_.publish(TargetHit{where, damage, killed});
        effect_blood(where, killed ? 16 : 5);
        if (killed) {
            const int points = enemy_def(kind).points;
            ++enemy_kills_;
            score_ += points;
            kill_enemy(ei);
            events_.publish(EnemyKilled{where, points});
            // Drops: gunners carry ammo, brutes carry health, and every third kill drops health.
            if (kind == ge::EnemyKind::Gunner) place_pickup(ge::PickupKind::Ammo, where);
            else if (kind == ge::EnemyKind::Brute || enemy_kills_ % 3 == 0) place_pickup(ge::PickupKind::Health, where);
        }
        return killed;
    }
    const std::size_t i = combined;
    flash_[i] = 1.0;
    bool killed = false;
    if (auto* hp = entities_.get<ge::Health>(targets_[i])) {
        hp->current -= damage;
        killed = hp->dead();
    }
    events_.publish(TargetHit{where, damage, killed});
    effect_sparks(where, 0.0, killed ? 14 : 4);
    if (killed) {
        ++kills_;
        score_ += kPointsPerKill;
        kill_target(i);
        events_.publish(TargetDestroyed{where, kPointsPerKill});
    }
    return killed;
}

void GameApplication::fire_weapon() {
    const WeaponDef& def = weapon_def(weapon_id());
    ++shots_;
    alert_timer_ = 0.6;  // gunfire wakes idle enemies
    events_.publish(ShotFired{weapon().ammo, current_weapon_});

    if (def.projectile) {
        const ge::Vec2 dir = ge::heading_vector(player_.angle);
        Projectile rocket;
        rocket.kind = ProjectileKind::Rocket;
        rocket.position = {player_.position.x + dir.x * 0.4, player_.position.y + dir.y * 0.4};
        rocket.velocity = {dir.x * def.projectile_speed, dir.y * def.projectile_speed};
        rocket.damage = def.damage;
        rocket.radius = 0.15;
        rocket.life = 3.0;
        projectiles_.push_back(rocket);
        return;
    }

    bool any_hit = false;
    for (int pellet = 0; pellet < def.pellets; ++pellet) {
        const double angle = player_.angle + (def.spread > 0.0 ? spread_random() * def.spread : 0.0);
        build_shot_targets();  // rebuilt per pellet: a kill reshuffles the lists
        const ShotResult r = hitscan(level_.map, player_.position, angle, shot_scratch_);
        const ge::Vec2 dir = ge::heading_vector(angle);
        if (r.target < 0) {  // hit a wall: sparks just in front of it
            if (std::isfinite(r.distance)) {
                const double back = std::max(0.0, r.distance - 0.1);
                effect_sparks({player_.position.x + dir.x * back, player_.position.y + dir.y * back}, 0.0, 3);
            }
            continue;
        }
        any_hit = true;
        double scale = 1.0;
        if (def.falloff_range > 0.0) scale = 1.0 + (def.falloff_min - 1.0) * std::min(1.0, r.distance / def.falloff_range);
        const int damage = std::max(1, static_cast<int>(std::lround(def.damage * scale)));
        damage_thing(static_cast<std::size_t>(r.target), damage, shot_scratch_[static_cast<std::size_t>(r.target)].position);
    }
    if (any_hit) {
        ++hits_;
        hit_marker_ = 0.15;
    }
}

// A blast: splash damage that fades with distance (blocked by walls), fire and smoke, a flash if you are close.
int GameApplication::explode(ge::Vec2 position, double radius, int damage) {
    events_.publish(Explosion{position, radius});
    ge::Particle fire;
    fire.color = ge::pack_rgb(255, 150, 40);
    fire.size = 0.14;
    fire.life = 0.7;
    fire.gravity = -0.3;
    fire.glow = true;
    particles_.burst(position, 0.0, 28, 3.6, 1.6, fire);
    ge::Particle smoke;
    smoke.color = ge::pack_rgb(90, 90, 95);
    smoke.size = 0.2;
    smoke.life = 1.3;
    smoke.gravity = -0.15;
    particles_.burst(position, -0.1, 12, 1.0, 0.7, smoke);

    const double to_player = std::hypot(player_.position.x - position.x, player_.position.y - position.y);
    if (to_player < 6.0) blast_flash_ = std::max(blast_flash_, 1.0 - to_player / 6.0);

    int damaged = 0;
    build_shot_targets();
    // Backwards: a death swaps or erases entries at and after its own index, which we have already handled.
    for (std::size_t c = shot_scratch_.size(); c-- > 0;) {
        const ShotTarget thing = shot_scratch_[c];
        const double d = std::hypot(thing.position.x - position.x, thing.position.y - position.y);
        if (d > radius + thing.radius || !line_of_sight(level_.map, position, thing.position)) continue;
        const double scale = std::clamp(1.0 - std::max(0.0, d - thing.radius) / radius, 0.0, 1.0);
        damage_thing(c, std::max(1, static_cast<int>(std::lround(damage * scale))), thing.position);
        ++damaged;
    }
    return damaged;
}

void GameApplication::spawn_enemy_bolt(std::size_t enemy_index, ge::Vec2 aim) {
    auto* t = entities_.get<ge::Transform>(enemies_[enemy_index]);
    auto* en = entities_.get<Enemy>(enemies_[enemy_index]);
    Projectile bolt;
    bolt.kind = ProjectileKind::Bolt;
    bolt.position = {t->position.x + aim.x * 0.4, t->position.y + aim.y * 0.4};
    bolt.velocity = {aim.x * en->shot_speed, aim.y * en->shot_speed};
    bolt.damage = en->shot_damage;
    bolt.radius = 0.12;
    bolt.life = 4.0;
    projectiles_.push_back(bolt);
    en->flash = 0.5;
    events_.publish(EnemyShot{t->position});
}

void GameApplication::update_projectiles(double dt) {
    constexpr double kPlayerRadius = 0.3;
    for (std::size_t i = 0; i < projectiles_.size();) {
        Projectile& p = projectiles_[i];
        const bool rocket = p.kind == ProjectileKind::Rocket;
        if (rocket) build_shot_targets();  // earlier blasts may have killed things
        const Impact impact = step_projectile(p, level_.map, dt,
                                              rocket ? std::span<const ShotTarget>(shot_scratch_) : std::span<const ShotTarget>(),
                                              player_.position, kPlayerRadius);
        // A glowing trail behind it.
        ge::Particle trail;
        trail.color = rocket ? ge::pack_rgb(255, 170, 60) : ge::pack_rgb(255, 90, 230);
        trail.size = rocket ? 0.07 : 0.05;
        trail.life = 0.3;
        trail.gravity = 0.0;
        trail.glow = true;
        trail.position = p.position;
        particles_.emit(trail);

        if (impact.kind == ImpactKind::None) { ++i; continue; }
        const Projectile done = p;
        projectiles_[i] = projectiles_.back();
        projectiles_.pop_back();
        if (done.kind == ProjectileKind::Rocket) {
            const WeaponDef& def = weapon_def(WeaponId::Launcher);
            if (explode(impact.point, def.splash_radius, done.damage) > 0) {
                ++hits_;
                hit_marker_ = 0.15;
            }
        } else {
            effect_sparks(impact.point, 0.0, 6);
            if (impact.kind == ImpactKind::Player) {
                damage_player(done.damage);
                if (state_ != GameState::Playing) return;
            }
        }
    }
}

ge::EnemyKind GameApplication::enemy_kind(std::size_t i) const {
    return const_cast<ge::EntityWorld&>(entities_).get<Enemy>(enemies_.at(i))->kind;
}

int GameApplication::enemy_health(std::size_t i) const {
    return const_cast<ge::EntityWorld&>(entities_).get<ge::Health>(enemies_.at(i))->current;
}

bool GameApplication::select_weapon(WeaponId id) {
    if (!owns_weapon(id)) return false;
    const int index = static_cast<int>(id);
    if (index == current_weapon_) return true;
    weapons_[static_cast<std::size_t>(current_weapon_)].reload_left = 0.0;  // switching cancels a reload
    current_weapon_ = index;
    Weapon& w = weapons_[static_cast<std::size_t>(index)];
    w.cooldown = std::max(w.cooldown, 0.25);  // a short raise time
    w.flash = 0.0;
    events_.publish(WeaponSwitched{index});
    return true;
}

void GameApplication::give_weapon(WeaponId id) {
    const WeaponDef& d = weapon_def(id);
    Weapon& w = weapons_[static_cast<std::size_t>(id)];
    const bool first = !owns_weapon(id);
    int added = 0;
    if (first) {
        owned_weapons_ |= 1u << static_cast<unsigned>(id);
        w.ammo = d.magazine;
        w.reserve = d.start_reserve;
        added = d.magazine + std::max(0, d.start_reserve);
    } else if (w.reserve >= 0) {
        const int before = w.reserve;
        w.reserve = std::min(d.max_reserve, w.reserve + d.start_reserve);
        added = w.reserve - before;
    }
    select_weapon(id);
    events_.publish(WeaponPickedUp{static_cast<int>(id), added, first});
}

void GameApplication::handle_weapon_input(const ge::InputState& input) {
    if (input.weapon_slot > 0) {
        for (int i = 0; i < kWeaponCount; ++i)
            if (weapon_def(static_cast<WeaponId>(i)).slot == input.weapon_slot) select_weapon(static_cast<WeaponId>(i));
    }
    for (int n = input.weapon_cycle; n != 0; n += (n > 0 ? -1 : 1)) {
        const int dir = n > 0 ? 1 : -1;
        for (int step = 1; step < kWeaponCount; ++step) {  // the next owned weapon in that direction
            const int candidate = ((current_weapon_ + dir * step) % kWeaponCount + kWeaponCount) % kWeaponCount;
            if (owns_weapon(static_cast<WeaponId>(candidate))) { select_weapon(static_cast<WeaponId>(candidate)); break; }
        }
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
        case GameState::GameOver:
            if (input.menu_quit) set_state(GameState::Quit);
            else if (input.confirm) { restart(); set_state(GameState::Playing); }
            break;
        case GameState::Quit:
            break;
    }
}

void GameApplication::update_playing(const ge::InputState& input, double dt) {
    update_player(player_, input, level_.map, dt);
    if (input.use) try_use_door();

    handle_weapon_input(input);
    Weapon& in_hand = weapons_[static_cast<std::size_t>(current_weapon_)];
    const bool was_reloading = in_hand.reloading();
    if (weapon_step(in_hand, input.fire, input.reload, dt)) fire_weapon();
    if (!was_reloading && weapons_[static_cast<std::size_t>(current_weapon_)].reloading()) events_.publish(ReloadStarted{});
    hit_marker_ = std::max(0.0, hit_marker_ - dt);
    hurt_flash_ = std::max(0.0, hurt_flash_ - dt * 2.5);
    for (double& f : flash_) f = std::max(0.0, f - dt * 5.0);

    motion_.step(dt, level_.map);
    for (std::size_t i = 0; i < targets_.size(); ++i)
        if (auto* t = entities_.get<ge::Transform>(targets_[i])) t->position = motion_.position(i);

    update_enemies(dt);
    if (state_ != GameState::Playing) return;  // the player died this step
    update_projectiles(dt);
    if (state_ != GameState::Playing) return;
    particles_.step(dt, level_.map);
    blast_flash_ = std::max(0.0, blast_flash_ - dt * 2.2);
    anim_time_ += dt;
    update_pickups();
    alert_timer_ = std::max(0.0, alert_timer_ - dt);

    if (targets_.empty() && enemies_.empty()) {
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
    const int walk_frame = static_cast<int>(anim_time_ * 4.0) & 1;
    for (ge::Entity e : enemies_) {
        const auto* t = entities_.get<ge::Transform>(e);
        const auto* en = entities_.get<Enemy>(e);
        ge::Sprite s;
        s.position = t->position;
        s.size = enemy_def(en->kind).height;
        s.image = &art_.enemy[static_cast<int>(en->kind)][en->state == EnemyState::Idle ? 0 : walk_frame];
        s.flash = en->flash;
        s.light = en->state == EnemyState::Idle ? 0.6 : 1.0;  // sleeping enemies look dim, hunting ones bright
        sprites_.push_back(s);
    }
    for (ge::Entity e : pickups_) {
        const auto* t = entities_.get<ge::Transform>(e);
        const auto* pk = entities_.get<Pickup>(e);
        if (const ge::SpriteImage* icon = art_.pickup_icon(pk->kind)) {  // weapons: a picture that hovers
            ge::Sprite s;
            s.position = t->position;
            s.size = 0.3;
            s.image = icon;
            s.z = 0.12 + 0.03 * std::sin(anim_time_ * 3.0);
            sprites_.push_back(s);
            continue;
        }
        const std::uint32_t colour = pk->kind == ge::PickupKind::Health ? ge::pack_rgb(60, 230, 90)
                                     : pk->kind == ge::PickupKind::Ammo ? ge::pack_rgb(240, 210, 60)
                                                                         : ge::pack_rgb(80, 160, 255);
        sprites_.push_back({t->position, 0.34, colour, 0.0, true});
    }
    for (const Projectile& p : projectiles_) {
        ge::Sprite s;
        s.position = p.position;
        s.size = p.kind == ProjectileKind::Rocket ? 0.14 : 0.16;
        s.color = p.kind == ProjectileKind::Rocket ? ge::pack_rgb(255, 190, 90) : ge::pack_rgb(255, 80, 225);
        s.light = 1.8;
        sprites_.push_back(s);
    }
    particles_.append_sprites(sprites_);
    ge::draw_sprites(cam, renderer_.depth(), sprites_, target);
    if (hurt_flash_ > 0.0) tint_red(target, hurt_flash_);
    if (blast_flash_ > 0.0) tint_warm(target, blast_flash_);

    // Frame timing is only used by the optional debug overlay.
    const double now = ge::monotonic_seconds();
    if (last_render_time_ > 0.0) frame_ms_ = (now - last_render_time_) * 1000.0;
    last_render_time_ = now;

    HudData hud;
    const Weapon& in_hand = weapon();
    hud.weapon = current_weapon_;
    hud.weapon_name = weapon_def(weapon_id()).name;
    hud.owned_weapons = owned_weapons_;
    hud.ammo = in_hand.ammo;
    hud.magazine = in_hand.magazine;
    hud.reserve = in_hand.reserve;
    hud.reloading = in_hand.reloading();
    hud.reload_fraction = hud.reloading ? 1.0 - in_hand.reload_left / in_hand.reload_time : 0.0;
    hud.weapon_flash = in_hand.flash;
    hud.hit_marker = hit_marker_ > 0.0 ? 1.0 : 0.0;
    hud.score = score_;
    hud.wave = wave_;
    hud.waves_to_win = level_.waves_to_win;
    hud.between_waves = between_waves_ && state_ != GameState::Victory;  // the win screen replaces the banner
    hud.debug = debug_;
    hud.frame_ms = frame_ms_;
    hud.fps = frame_ms_ > 0.0 ? 1000.0 / frame_ms_ : 0.0;
    hud.entities = entities_.size();
    hud.backend = ge::opt::fog_kernel_name(ge::opt::active_fog_kernel());
    hud.level_name = level_.name.c_str();
    hud.health = player_.health;
    hud.max_health = player_.max_health;
    hud.keys = player_.keys;
    hud.targets_left = static_cast<int>(targets_.size() + enemies_.size());
    for (const FeedMessage& m : feed_) hud.feed.push_back(m.text);

    draw_weapon_view(target, in_hand.flash, in_hand.reloading(), current_weapon_);
    draw_hud(target, hud);

    switch (state_) {
        case GameState::Menu: draw_menu_screen(target, hud); break;
        case GameState::Paused: draw_pause_screen(target); break;
        case GameState::GameOver:
            draw_gameover_screen(target, hud);
            break;
        case GameState::Victory:
            hud.shots = shots_;
            hud.hits = hits_;
            draw_victory_screen(target, hud);
            break;
        default: break;
    }
}

}  // namespace game
