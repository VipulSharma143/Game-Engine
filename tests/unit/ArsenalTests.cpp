// The new weapons (shotgun, chaingun, rocket launcher), the new enemies (gunner, brute, skitter), projectiles,
// weapon pickups and switching, and kill rewards.
#include <cmath>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "TestFramework.hpp"
#include "engine/audio/AudioMixer.hpp"
#include "engine/audio/Synth.hpp"
#include "engine/world/LevelLoader.hpp"
#include "game/GameAudio.hpp"
#include "game/Hud.hpp"

namespace {

using game::EnemyState;
using game::WeaponId;

// A wide open hall: the player starts at the west end facing east.
const char* kHall =
    "#####################\n"
    "#P..................#\n"
    "#...................#\n"
    "#...................#\n"
    "#...................#\n"
    "#...................#\n"
    "#####################\n";

std::unique_ptr<game::GameApplication> app_from(const std::string& rows) {
    std::string error;
    auto level = ge::parse_level_text("waves: 3\nmap:\n" + rows, error);
    if (!level) { std::fprintf(stderr, "bad test level: %s\n", error.c_str()); return nullptr; }
    auto app = std::make_unique<game::GameApplication>(*level);
    ge::EngineConfig cfg;
    CHECK(app->initialize(cfg).ok);
    app->place_single_target({19.5, 5.5});  // one far, stationary target: clears the wave's own targets and enemies
    return app;
}

void step(game::GameApplication& app, int frames, ge::InputState in = {}) {
    for (int i = 0; i < frames; ++i) { app.fixed_update(in, 1.0 / 60); in.clear_edges(); }
}

ge::InputState holding_fire() { ge::InputState i; i.fire = true; return i; }

// Gives the weapon and waits out the short raise time, so the next press fires at once.
void arm(game::GameApplication& app, WeaponId id) {
    app.give_weapon(id);
    step(app, 20);
}

}  // namespace

// ---- definitions and ammo -------------------------------------------------------------------------------------

TEST(weapon_and_enemy_tables_are_consistent) {
    bool slots[10] = {};
    for (int i = 0; i < game::kWeaponCount; ++i) {
        const game::WeaponDef& d = game::weapon_def(static_cast<WeaponId>(i));
        CHECK(d.magazine > 0 && d.damage > 0 && d.pellets >= 1 && d.fire_interval > 0.0 && d.reload_time > 0.0);
        CHECK(d.slot >= 1 && d.slot <= 9 && !slots[d.slot]);
        slots[d.slot] = true;
        if (d.start_reserve >= 0) CHECK(d.max_reserve >= d.start_reserve && d.ammo_pickup > 0);
        if (d.projectile) CHECK(d.projectile_speed > 0.0 && d.splash_radius > 0.0);
    }
    CHECK_EQ(game::weapon_def(WeaponId::Pistol).start_reserve, -1);        // the pistol never runs dry
    CHECK_EQ(game::weapon_def(WeaponId::Shotgun).pellets, 8);
    CHECK(game::weapon_def(WeaponId::Launcher).projectile);
    for (int k = 0; k < ge::kEnemyKindCount; ++k) {
        const game::EnemyDef& d = game::enemy_def(static_cast<ge::EnemyKind>(k));
        CHECK(d.health > 0 && d.speed > 0.0 && d.radius > 0.0 && d.height > 0.0 && d.points > 0);
        CHECK(d.ranged ? (d.max_range > d.min_range && d.shot_damage > 0 && d.shot_speed > 0.0) : (d.melee_damage > 0 && d.melee_range > 0.0));
    }
    CHECK(game::enemy_def(ge::EnemyKind::Brute).health > game::enemy_def(ge::EnemyKind::Grunt).health);
    CHECK(game::enemy_def(ge::EnemyKind::Skitter).speed > game::enemy_def(ge::EnemyKind::Grunt).speed);
    CHECK(game::enemy_def(ge::EnemyKind::Gunner).ranged);
}

TEST(a_weapon_with_a_reserve_reloads_from_it_and_stops_when_both_are_empty) {
    game::Weapon w;
    w.magazine = 4; w.ammo = 4; w.reserve = 6; w.fire_interval = 0.1; w.reload_time = 0.5;
    int fired = 0;
    for (int i = 0; i < 60 && w.ammo > 0; ++i) fired += game::weapon_step(w, true, false, 0.1) ? 1 : 0;
    CHECK_EQ(fired, 4);
    CHECK(game::weapon_step(w, true, false, 0.1) == false);  // empty: starts reloading
    CHECK(w.reloading());
    for (int i = 0; i < 10; ++i) game::weapon_step(w, false, false, 0.1);
    CHECK_EQ(w.ammo, 4);
    CHECK_EQ(w.reserve, 2);                                   // 6 - 4
    for (int i = 0; i < 60 && w.ammo > 0; ++i) game::weapon_step(w, true, false, 0.1);
    for (int i = 0; i < 10; ++i) game::weapon_step(w, false, false, 0.1);
    CHECK_EQ(w.ammo, 2);                                      // only what was left
    CHECK_EQ(w.reserve, 0);
    for (int i = 0; i < 60 && w.ammo > 0; ++i) game::weapon_step(w, true, false, 0.1);
    CHECK(w.out_of_ammo());
    CHECK(!game::weapon_step(w, true, false, 0.1));           // nothing to fire...
    CHECK(!w.reloading());                                    // ...and no endless reload loop
    game::Weapon pistol;                                      // unlimited reserve behaves as before
    pistol.ammo = 0;
    game::weapon_step(pistol, false, false, 0.1);
    for (int i = 0; i < 20; ++i) game::weapon_step(pistol, false, false, 0.1);
    CHECK_EQ(pistol.ammo, pistol.magazine);
    CHECK_EQ(pistol.reserve, -1);
}

// ---- weapons ------------------------------------------------------------------------------------------------------

TEST(the_pistol_still_does_exactly_34_damage) {
    auto app = app_from(kHall);
    app->place_enemy({6.5, 1.5});
    step(*app, 1, holding_fire());
    CHECK_EQ(app->enemy_health(0), 60 - 34);
    CHECK_EQ(app->weapon().ammo, 11);
}

TEST(the_shotgun_hurts_a_lot_up_close_and_little_from_far_away) {
    auto lost_at = [](double distance) {
        auto app = app_from(kHall);
        app->place_enemy({1.5 + distance, 1.5}, ge::EnemyKind::Brute);
        arm(*app, WeaponId::Shotgun);
        const int before = app->enemy_health(0);
        step(*app, 1, holding_fire());
        return before - (app->enemy_count() ? app->enemy_health(0) : 0);
    };
    const int close = lost_at(1.8), far = lost_at(8.0);
    CHECK(close >= 80);              // most of the 8 pellets land, near full strength
    CHECK(far > 0);                  // a few still reach
    CHECK(far < close / 2);          // spread and falloff
}

TEST(the_shotgun_kills_a_grunt_at_point_blank_and_uses_one_shell) {
    auto app = app_from(kHall);
    app->place_enemy({3.0, 1.5});
    arm(*app, WeaponId::Shotgun);
    CHECK_EQ(app->weapon().ammo, 6);
    CHECK_EQ(app->weapon().reserve, 12);
    step(*app, 1, holding_fire());
    CHECK_EQ(app->enemy_count(), 0u);
    CHECK_EQ(app->weapon().ammo, 5);
    CHECK_EQ(app->shots_fired(), 1u);   // eight pellets are still one shot
    CHECK_EQ(app->shots_hit(), 1u);
    CHECK(app->particles().alive() > 0);
}

TEST(a_shotgun_reload_takes_shells_from_the_reserve) {
    auto app = app_from(kHall);
    arm(*app, WeaponId::Shotgun);
    for (int i = 0; i < 1200 && app->weapon().ammo > 0; ++i) step(*app, 1, holding_fire());
    CHECK_EQ(app->weapon().ammo, 0);
    step(*app, 1);                                        // the empty magazine starts a reload on the next step
    CHECK(app->weapon().reloading());
    step(*app, 120);                                      // longer than the 1.6 s reload
    CHECK_EQ(app->weapon().ammo, 6);
    CHECK_EQ(app->weapon().reserve, 6);                   // 12 - 6
}

TEST(the_chaingun_fires_about_a_dozen_rounds_a_second) {   // 0.07 s between shots, on 60 Hz steps: every 5th step
    auto app = app_from(kHall);
    arm(*app, WeaponId::Chaingun);
    const unsigned before = app->shots_fired();
    step(*app, 60, holding_fire());
    const unsigned fired = app->shots_fired() - before;
    CHECK(fired >= 11 && fired <= 14);
    CHECK_EQ(app->weapon().ammo, 40 - static_cast<int>(fired));
}

TEST(a_rocket_flies_then_explodes_at_the_wall_and_the_blast_fades_with_distance) {
    auto splash_at = [](ge::Vec2 where, int* explosions) {
        auto app = app_from(kHall);
        app->place_single_target(where);               // stationary, off the rocket's path
        int damage = 0, blasts = 0;
        app->events().subscribe<game::TargetHit>([&](const game::TargetHit& e) { damage += e.damage; });
        app->events().subscribe<game::Explosion>([&](const game::Explosion&) { ++blasts; });
        arm(*app, WeaponId::Launcher);
        step(*app, 1, holding_fire());
        CHECK_EQ(app->projectile_count(), 1u);          // it is in the air, not an instant hit
        CHECK_EQ(blasts, 0);
        for (int i = 0; i < 300 && app->projectile_count() > 0; ++i) step(*app, 1);
        CHECK_EQ(app->projectile_count(), 0u);
        if (explosions) *explosions = blasts;
        return damage;
    };
    int blasts = 0;
    const int near_damage = splash_at({20.0, 2.3}, &blasts);   // the rocket ends at the east wall, level with the player
    const int far_damage = splash_at({19.0, 2.5}, nullptr);
    CHECK_EQ(blasts, 1);
    CHECK(near_damage > 0);
    CHECK(far_damage > 0);
    CHECK(near_damage > far_damage);
    CHECK(splash_at({5.5, 4.5}, nullptr) == 0);                // far from the blast: untouched
}

TEST(a_rocket_that_hits_something_explodes_on_it_and_counts_as_a_hit) {
    auto app = app_from(kHall);
    app->place_enemy({12.5, 1.5}, ge::EnemyKind::Brute);
    arm(*app, WeaponId::Launcher);
    step(*app, 1, holding_fire());
    const int before = app->enemy_health(0);
    for (int i = 0; i < 300 && app->projectile_count() > 0; ++i) step(*app, 1);
    CHECK(app->enemy_count() == 0 || app->enemy_health(0) < before);
    CHECK_EQ(app->shots_hit(), 1u);
}

// ---- projectiles -------------------------------------------------------------------------------------------------

TEST(a_bolt_flies_hits_the_player_or_a_wall_and_cannot_tunnel_through_a_thin_wall) {
    std::string error;
    auto level = ge::parse_level_text("map:\n#########\n#P..#...#\n#########\n", error);
    CHECK(level.has_value());
    if (!level) return;
    game::Projectile bolt;
    bolt.position = {1.5, 1.5};
    bolt.velocity = {2.0, 0.0};
    const ge::Vec2 far_player{7.5, 1.5};
    game::Impact hit = game::step_projectile(bolt, level->map, 0.1, {}, far_player, 0.3);
    CHECK(hit.kind == game::ImpactKind::None);
    CHECK_NEAR(bolt.position.x, 1.7, 1e-9);

    bolt.position = {1.5, 1.5};
    bolt.velocity = {300.0, 0.0};                                  // 5 cells in one step: it would jump over the wall in column 4
    hit = game::step_projectile(bolt, level->map, 1.0 / 60.0, {}, far_player, 0.3);
    CHECK(hit.kind == game::ImpactKind::Wall);
    CHECK(hit.point.x < 4.0);                                      // stopped at the wall in column 4, not beyond it

    bolt.position = {1.5, 1.5};
    bolt.velocity = {5.0, 0.0};
    hit = game::step_projectile(bolt, level->map, 0.1, {}, {2.2, 1.5}, 0.3);   // the player is in the way
    CHECK(hit.kind == game::ImpactKind::Player);

    game::Projectile rocket;
    rocket.kind = game::ProjectileKind::Rocket;
    rocket.position = {1.5, 1.5};
    rocket.velocity = {5.0, 0.0};
    const game::ShotTarget targets[] = {{{9.0, 9.0}, 0.3}, {{2.4, 1.5}, 0.3}};
    hit = game::step_projectile(rocket, level->map, 0.2, targets, {99.0, 99.0}, 0.3);
    CHECK(hit.kind == game::ImpactKind::Target);
    CHECK_EQ(hit.index, 1u);

    game::Projectile fizzle;
    fizzle.position = {1.5, 1.5};
    fizzle.velocity = {0.0, 0.0};
    fizzle.life = 0.05;
    CHECK(game::step_projectile(fizzle, level->map, 0.1, {}, far_player, 0.3).kind == game::ImpactKind::Wall);   // out of life
}

// ---- enemy behaviour -----------------------------------------------------------------------------------------------

TEST(enemies_take_their_numbers_from_the_definition_table) {
    const game::Enemy grunt = game::make_enemy(ge::EnemyKind::Grunt), gunner = game::make_enemy(ge::EnemyKind::Gunner);
    const game::Enemy brute = game::make_enemy(ge::EnemyKind::Brute), skitter = game::make_enemy(ge::EnemyKind::Skitter);
    CHECK(!grunt.ranged && !brute.ranged && !skitter.ranged && gunner.ranged);
    CHECK(skitter.speed > grunt.speed && grunt.speed > brute.speed);
    CHECK(brute.damage > grunt.damage && grunt.damage > skitter.damage);
    CHECK(brute.radius > grunt.radius && grunt.radius > skitter.radius);
    CHECK(gunner.kind == ge::EnemyKind::Gunner);
    CHECK_NEAR(grunt.speed, 1.7, 0.0);   // unchanged from before the new enemies existed
    CHECK_EQ(grunt.damage, 10);
}

TEST(a_gunner_shoots_from_range_backs_away_when_close_and_chases_when_far) {
    std::string error;
    auto level = ge::parse_level_text("map:\n#########################\n#P......................#\n#.......................#\n#########################\n", error);
    CHECK(level.has_value());
    if (!level) return;
    game::DistanceField field;
    const ge::Vec2 player{1.5, 1.5};
    field.rebuild(level->map, 1, 1);

    game::Enemy g = game::make_enemy(ge::EnemyKind::Gunner);
    ge::Vec2 pos{8.5, 1.5};                                                  // 7 cells away, clear line
    game::EnemyDecision d = game::enemy_think(g, pos, player, level->map, field, false, 1.0 / 60.0);
    CHECK(g.state == EnemyState::Attack);
    CHECK(d.fire);
    CHECK_NEAR(d.aim.x, -1.0, 1e-9);                                         // aimed at the player
    d = game::enemy_think(g, pos, player, level->map, field, false, 1.0 / 60.0);
    CHECK(!d.fire);                                                          // reloading its weapon
    for (int i = 0; i < 100; ++i) d = game::enemy_think(g, pos, player, level->map, field, false, 1.0 / 60.0);
    CHECK(g.shoot_cooldown >= 0.0);
    bool fired_again = false;
    for (int i = 0; i < 200 && !fired_again; ++i) fired_again = game::enemy_think(g, pos, player, level->map, field, false, 1.0 / 60.0).fire;
    CHECK(fired_again);

    game::Enemy close = game::make_enemy(ge::EnemyKind::Gunner);
    pos = {3.5, 1.5};                                                        // 2 cells away: too close
    d = game::enemy_think(close, pos, player, level->map, field, false, 1.0 / 60.0);
    CHECK(d.move.x > 0.0);                                                   // steps back, away from the player

    game::Enemy far = game::make_enemy(ge::EnemyKind::Gunner);
    pos = {22.5, 1.5};                                                       // 21 cells away: out of range
    d = game::enemy_think(far, pos, player, level->map, field, true, 1.0 / 60.0);
    CHECK(far.state == EnemyState::Chase);
    CHECK(!d.fire);
    CHECK(d.move.x < 0.0);                                                   // walks towards the player

    auto walled = ge::parse_level_text("map:\n#########\n#P.#....#\n#########\n", error);
    CHECK(walled.has_value());
    if (!walled) return;
    game::DistanceField f2;
    f2.rebuild(walled->map, 1, 1);
    game::Enemy blind = game::make_enemy(ge::EnemyKind::Gunner);
    d = game::enemy_think(blind, {5.5, 1.5}, {1.5, 1.5}, walled->map, f2, true, 1.0 / 60.0);
    CHECK(!d.fire);                                                          // no line of sight, no shot
}

TEST(a_gunner_hits_the_player_with_bolts_from_a_distance) {
    auto app = app_from(kHall);
    app->place_enemy({9.5, 1.5}, ge::EnemyKind::Gunner);
    int shots = 0, hits = 0;
    app->events().subscribe<game::EnemyShot>([&](const game::EnemyShot&) { ++shots; });
    app->events().subscribe<game::PlayerHit>([&](const game::PlayerHit&) { ++hits; });
    double closest = 99.0;
    for (int i = 0; i < 900 && app->state() == game::GameState::Playing; ++i) {
        step(*app, 1);
        if (app->enemy_count() > 0) closest = std::min(closest, std::hypot(app->enemy_position(0).x - app->player().position.x, app->enemy_position(0).y - app->player().position.y));
    }
    CHECK(shots >= 3);
    CHECK(hits >= 1);
    CHECK(app->player_health() < 100);
    CHECK(closest > 2.5);               // it kites: it never walks up to bite
}

TEST(brutes_hit_hard_and_skitters_are_fast_and_fragile) {
    auto run_until_bitten = [](ge::EnemyKind kind, double* seconds, int* damage) {
        auto app = app_from(kHall);
        app->place_enemy({7.5, 1.5}, kind);
        int taken = 0;
        app->events().subscribe<game::PlayerHit>([&](const game::PlayerHit& e) { taken = e.amount; });
        int frames = 0;
        for (; frames < 1200 && taken == 0; ++frames) { step(*app, 1, frames == 0 ? holding_fire() : ge::InputState{}); if (app->enemy_count() == 0) break; }
        *seconds = frames / 60.0;
        *damage = taken;
    };
    double t_brute = 0, t_skitter = 0, t_grunt = 0;
    int d_brute = 0, d_skitter = 0, d_grunt = 0;
    run_until_bitten(ge::EnemyKind::Brute, &t_brute, &d_brute);
    run_until_bitten(ge::EnemyKind::Skitter, &t_skitter, &d_skitter);
    run_until_bitten(ge::EnemyKind::Grunt, &t_grunt, &d_grunt);
    CHECK_EQ(d_brute, 28);
    CHECK_EQ(d_grunt, 10);
    CHECK(d_skitter == 5 || d_skitter == 0);   // the skitter may die to the first pistol shot (24 health) before it bites
    CHECK(t_brute > t_grunt);                  // slower to arrive
}

// ---- pickups, inventory, switching --------------------------------------------------------------------------------------

TEST(weapon_pickups_grant_the_weapon_with_ammo_select_it_and_top_up_when_owned) {
    auto app = app_from(kHall);
    int picked = 0;
    bool first = false;
    app->events().subscribe<game::WeaponPickedUp>([&](const game::WeaponPickedUp& e) { ++picked; first = e.first_time; });
    CHECK(!app->owns_weapon(WeaponId::Shotgun));
    app->place_pickup(ge::PickupKind::Shotgun, app->player().position);
    step(*app, 1);
    CHECK(app->owns_weapon(WeaponId::Shotgun));
    CHECK(app->weapon_id() == WeaponId::Shotgun);       // picking it up puts it in your hands
    CHECK_EQ(app->weapon().ammo, 6);
    CHECK_EQ(app->weapon().reserve, 12);
    CHECK_EQ(picked, 1);
    CHECK(first);
    CHECK_EQ(app->pickup_count(), 0u);

    app->place_pickup(ge::PickupKind::Shotgun, app->player().position);   // a second one adds shells
    step(*app, 1);
    CHECK_EQ(app->weapon().reserve, 24);
    CHECK(!first);
    app->place_pickup(ge::PickupKind::Shotgun, app->player().position);
    step(*app, 1);
    CHECK_EQ(app->weapon().reserve, 36);                // the cap
    app->place_pickup(ge::PickupKind::Shotgun, app->player().position);
    step(*app, 1);
    CHECK_EQ(app->pickup_count(), 1u);                  // a full stack is left on the floor
    CHECK_EQ(app->weapon().reserve, 36);

    app->place_pickup(ge::PickupKind::Launcher, app->player().position);
    app->place_pickup(ge::PickupKind::Chaingun, app->player().position);
    step(*app, 1);
    CHECK(app->owns_weapon(WeaponId::Launcher) && app->owns_weapon(WeaponId::Chaingun));
    CHECK_EQ(app->weapon_state(WeaponId::Launcher).reserve, 5);
    CHECK_EQ(app->weapon_state(WeaponId::Chaingun).reserve, 80);
}

TEST(ammo_packs_top_up_every_weapon_you_own_and_are_left_alone_when_all_are_full) {
    auto app = app_from(kHall);
    app->place_pickup(ge::PickupKind::Ammo, app->player().position);
    step(*app, 1);
    CHECK_EQ(app->pickup_count(), 1u);                  // only the pistol, and it is full: nothing to give
    app->give_weapon(WeaponId::Shotgun);                // (no stepping in between: the pack is still at the player's feet)
    app->give_weapon(WeaponId::Chaingun);
    step(*app, 1);
    CHECK_EQ(app->pickup_count(), 0u);
    CHECK_EQ(app->weapon_state(WeaponId::Shotgun).reserve, 12 + 6);
    CHECK_EQ(app->weapon_state(WeaponId::Chaingun).reserve, 80 + 40);
    CHECK_EQ(app->weapon_state(WeaponId::Pistol).reserve, -1);
}

TEST(weapons_switch_by_slot_and_wheel_only_to_owned_ones) {
    auto app = app_from(kHall);
    int switched = 0;
    app->events().subscribe<game::WeaponSwitched>([&](const game::WeaponSwitched&) { ++switched; });
    ge::InputState slot3; slot3.weapon_slot = 3;
    step(*app, 1, slot3);
    CHECK(app->weapon_id() == WeaponId::Pistol);        // not owned: ignored
    CHECK_EQ(switched, 0);
    app->give_weapon(WeaponId::Shotgun);
    app->give_weapon(WeaponId::Launcher);
    switched = 0;
    ge::InputState slot1; slot1.weapon_slot = 1;
    step(*app, 1, slot1);
    CHECK(app->weapon_id() == WeaponId::Pistol);
    CHECK_EQ(switched, 1);
    ge::InputState next; next.weapon_cycle = 1;
    step(*app, 1, next);
    CHECK(app->weapon_id() == WeaponId::Shotgun);       // next owned
    step(*app, 1, next);
    CHECK(app->weapon_id() == WeaponId::Launcher);      // skips the chaingun, which is not owned
    step(*app, 1, next);
    CHECK(app->weapon_id() == WeaponId::Pistol);        // wraps
    ge::InputState back; back.weapon_cycle = -1;
    step(*app, 1, back);
    CHECK(app->weapon_id() == WeaponId::Launcher);
    CHECK(!app->select_weapon(WeaponId::Chaingun));
}

TEST(switching_cancels_a_reload_and_has_a_short_raise_time) {
    auto app = app_from(kHall);
    app->give_weapon(WeaponId::Shotgun);
    step(*app, 20);
    ge::InputState reload; reload.reload = true;
    step(*app, 1, reload);                              // a full magazine does not reload
    CHECK(!app->weapon().reloading());
    step(*app, 1, holding_fire());                      // fire once so a reload is possible
    step(*app, 60);
    step(*app, 1, reload);
    CHECK(app->weapon().reloading());
    ge::InputState slot1; slot1.weapon_slot = 1;
    step(*app, 1, slot1);
    CHECK(!app->weapon_state(WeaponId::Shotgun).reloading());
    const unsigned shots = app->shots_fired();
    step(*app, 3, holding_fire());                      // still raising the pistol
    CHECK_EQ(app->shots_fired(), shots);
    step(*app, 30, holding_fire());
    CHECK(app->shots_fired() > shots);
}

// ---- rewards ---------------------------------------------------------------------------------------------------------------

TEST(each_enemy_kind_is_worth_its_points_and_drops_the_right_pickup) {
    struct Case { ge::EnemyKind kind; int points; std::size_t drops; };
    for (const Case& c : {Case{ge::EnemyKind::Gunner, 220, 1}, Case{ge::EnemyKind::Brute, 400, 1}, Case{ge::EnemyKind::Skitter, 90, 0}, Case{ge::EnemyKind::Grunt, 150, 0}}) {
        auto app = app_from(kHall);
        app->place_enemy({5.5, 1.5}, c.kind);
        arm(*app, WeaponId::Chaingun);
        int ammo_drops = 0, health_drops = 0;
        for (int i = 0; i < 600 && app->enemy_count() > 0; ++i) step(*app, 1, holding_fire());
        (void)ammo_drops; (void)health_drops;
        CHECK_EQ(app->enemy_count(), 0u);
        CHECK_EQ(app->score(), c.points);
        CHECK_EQ(app->pickup_count(), c.drops);
    }
}

TEST(a_new_run_gives_back_only_the_pistol_and_clears_flying_things) {
    auto app = app_from(kHall);
    arm(*app, WeaponId::Launcher);
    step(*app, 1, holding_fire());
    CHECK(app->projectile_count() > 0);
    app->restart();
    CHECK(app->weapon_id() == WeaponId::Pistol);
    CHECK(!app->owns_weapon(WeaponId::Launcher));
    CHECK_EQ(app->projectile_count(), 0u);
    CHECK_EQ(app->particles().alive(), 0u);
    CHECK_EQ(app->weapon().ammo, 12);
}

// ---- the real levels ----------------------------------------------------------------------------------------------------------------

TEST(enemies_find_their_way_through_the_catacomb_maze_to_the_player) {
    std::string error;
    auto level = ge::load_level_file(std::filesystem::path(GE_ASSETS_DIR) / "levels" / "catacombs.txt", error);
    CHECK(level.has_value());
    if (!level) return;
    for (const ge::EnemyKind kind : {ge::EnemyKind::Skitter, ge::EnemyKind::Brute, ge::EnemyKind::Grunt}) {
        game::GameApplication app(*level);
        ge::EngineConfig cfg;
        CHECK(app.initialize(cfg).ok);
        const ge::EnemySpawn far = level->enemy_spawns.back();                // the deepest spawn
        app.place_single_target({1.5, 3.5});
        app.place_enemy(far.position, kind);
        ge::InputState wake;
        wake.fire = true;
        step(app, 1, wake);
        int hits = 0;
        app.events().subscribe<game::PlayerHit>([&](const game::PlayerHit&) { ++hits; });
        for (int i = 0; i < 60 * 90 && hits == 0 && app.state() == game::GameState::Playing; ++i) step(app, 1);
        CHECK(hits > 0);   // it walked the whole maze and reached the player
    }
}

// ---- drawing and sound ----------------------------------------------------------------------------------------------------------------

TEST(each_weapon_has_its_own_view_and_flash) {
    std::vector<std::uint64_t> sums;
    for (int w = 0; w < game::kWeaponCount; ++w) {
        ge::Framebuffer fb(160, 100);
        game::draw_weapon_view(fb, 0.0, false, w);
        sums.push_back(fb.checksum());
        ge::Framebuffer fired(160, 100);
        game::draw_weapon_view(fired, 1.0, false, w);
        CHECK(fired.checksum() != fb.checksum());              // the muzzle flash shows
        ge::Framebuffer reloading(160, 100);
        game::draw_weapon_view(reloading, 0.0, true, w);
        CHECK(reloading.checksum() != fb.checksum());          // it drops while reloading
    }
    for (std::size_t i = 0; i < sums.size(); ++i)
        for (std::size_t j = i + 1; j < sums.size(); ++j) CHECK(sums[i] != sums[j]);
}

TEST(enemies_and_effects_change_the_frame_and_the_game_renders_deterministically) {
    auto frame = [](bool with_enemy) {
        auto app = app_from(kHall);
        if (with_enemy) app->place_enemy({5.5, 1.5}, ge::EnemyKind::Brute);
        step(*app, 3);
        ge::Framebuffer fb(192, 120);
        app->render(fb);
        return fb.checksum();
    };
    CHECK(frame(true) != frame(false));
    CHECK(frame(true) == frame(true));

    auto fight = [] {
        auto app = app_from(kHall);
        app->place_enemy({6.5, 1.5}, ge::EnemyKind::Gunner);
        arm(*app, WeaponId::Shotgun);
        step(*app, 40, holding_fire());
        ge::Framebuffer fb(192, 120);
        app->render(fb);
        return fb.checksum();
    };
    CHECK(fight() == fight());
}

TEST(the_new_game_events_play_their_sounds) {
    auto app = app_from(kHall);
    ge::AudioMixer mixer(44100);
    const ge::SoundBank bank = ge::SoundBank::make();
    game::GameAudio audio(*app, mixer, bank);
    const std::size_t base = mixer.total_started();
    app->events().publish(game::ShotFired{5, static_cast<int>(WeaponId::Shotgun)});
    app->events().publish(game::ShotFired{5, static_cast<int>(WeaponId::Chaingun)});
    app->events().publish(game::ShotFired{5, static_cast<int>(WeaponId::Launcher)});
    app->events().publish(game::EnemyShot{{4.0, 2.0}});
    app->events().publish(game::Explosion{{4.0, 2.0}, 2.0});
    app->events().publish(game::WeaponSwitched{1});
    CHECK_EQ(mixer.total_started() - base, 6u);
}
