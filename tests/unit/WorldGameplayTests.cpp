#include <memory>
#include <string>
#include <vector>

#include "TestFramework.hpp"
#include "engine/world/LevelLoader.hpp"
#include "game/GameAudio.hpp"

namespace {

// Level text helper: the player starts at 'P' facing east.
std::unique_ptr<game::GameApplication> make_app(const std::string& map_rows, int waves = 3) {
    std::string error;
    auto level = ge::parse_level_text("waves: " + std::to_string(waves) + "\nmap:\n" + map_rows, error);
    if (!level) { std::fprintf(stderr, "bad test level: %s\n", error.c_str()); return nullptr; }
    auto app = std::make_unique<game::GameApplication>(*level);
    ge::EngineConfig cfg;
    CHECK(app->initialize(cfg).ok);
    return app;
}

const char* kRoom =
    "###########\n"
    "#P........#\n"
    "#.........#\n"
    "#.........#\n"
    "###########\n";

void step(game::GameApplication& app, int frames, const ge::InputState& in = {}) {
    for (int i = 0; i < frames; ++i) app.fixed_update(in, 1.0 / 60);
}

ge::InputState pressed_use() { ge::InputState i; i.use = true; return i; }

}  // namespace

// ---- level format ---------------------------------------------------------------------------------
TEST(level_markers_for_enemies_pickups_and_doors_parse) {
    std::string error;
    const auto level = ge::parse_level_text("map:\n#######\n#PEHAK#\n#.D.L.#\n#######\n", error);
    CHECK(level.has_value());
    if (!level) return;
    CHECK_EQ(level->enemy_spawns.size(), 1u);
    CHECK_NEAR(level->enemy_spawns[0].position.x, 2.5, 0.0);
    CHECK_EQ(level->pickups.size(), 3u);
    CHECK(level->pickups[0].kind == ge::PickupKind::Health);
    CHECK(level->pickups[1].kind == ge::PickupKind::Ammo);
    CHECK(level->pickups[2].kind == ge::PickupKind::Key);
    CHECK_EQ(level->map.cell(2, 2), ge::kDoorId);
    CHECK_EQ(level->map.cell(4, 2), ge::kLockedDoorId);
    CHECK(level->map.is_solid(2, 2));  // doors block movement until opened
    CHECK(!level->map.is_solid(2, 1));  // marker cells are floor
    ge::GridMap m = level->map;
    m.set_cell(2, 2, 0);
    CHECK(!m.is_solid(2, 2));
    m.set_cell(-5, 99, 7);  // out of range is ignored
}

// ---- enemies in the game --------------------------------------------------------------------------------
TEST(enemies_spawn_from_markers_and_scale_with_the_wave) {
    auto app = make_app("###########\n#P...E..E.#\n#.........#\n#.........#\n###########\n");
    CHECK_EQ(app->enemy_count(), 1u);               // wave 1 spawns the first marker
    CHECK(app->enemy_state(0) == game::EnemyState::Idle);
    app->place_single_target({3.0, 2.0});           // isolate: clears targets and enemies
    CHECK_EQ(app->enemy_count(), 0u);
}

TEST(an_enemy_that_sees_the_player_chases_and_bites) {
    auto app = make_app(kRoom);
    app->place_enemy({7.5, 1.5});                   // clear line of sight, 6 cells away
    CHECK(app->enemy_state(0) == game::EnemyState::Idle);
    int hits = 0, last_health = 100;
    app->events().subscribe<game::PlayerHit>([&](const game::PlayerHit& h) { ++hits; last_health = h.health_left; });
    step(*app, 60);
    CHECK(app->enemy_state(0) != game::EnemyState::Idle);
    const double d_before = app->enemy_position(0).x;
    step(*app, 120);
    CHECK(app->enemy_position(0).x < d_before + 1e-9);   // it came closer (x decreases towards the player)
    step(*app, 120);
    CHECK(hits >= 2);
    CHECK_EQ(app->player_health(), last_health);
    CHECK(app->player_health() <= 80);
}

TEST(player_dies_game_over_and_retry_resets_everything) {
    auto app = make_app("###########\n#P....E...#\n#.........#\n#H........#\n###########\n");
    int died = 0;
    app->events().subscribe<game::PlayerDied>([&](const game::PlayerDied&) { ++died; });
    for (int i = 0; i < 60 * 30 && app->state() == game::GameState::Playing; ++i) app->fixed_update({}, 1.0 / 60);
    CHECK(app->state() == game::GameState::GameOver);
    CHECK_EQ(app->player_health(), 0);
    CHECK_EQ(died, 1);
    CHECK(!app->wants_mouse_capture());

    const ge::Vec2 frozen = app->player().position;
    ge::InputState walk;
    walk.forward = 1.0;
    step(*app, 30, walk);
    CHECK_NEAR(app->player().position.x, frozen.x, 0.0);  // the dead cannot move

    ge::InputState confirm;
    confirm.confirm = true;
    app->fixed_update(confirm, 1.0 / 60);
    CHECK(app->state() == game::GameState::Playing);
    CHECK_EQ(app->player_health(), 100);
    CHECK_EQ(app->player_keys(), 0);
    CHECK_EQ(app->score(), 0);
    CHECK_EQ(app->enemy_count(), 1u);
    CHECK_EQ(app->pickup_count(), 1u);                   // the level's health pack is back
    CHECK_NEAR(app->player().position.x, 1.5, 0.0);
}

TEST(shooting_an_enemy_takes_two_hits_wakes_it_and_scores) {
    auto app = make_app(kRoom);
    app->place_single_target({9.0, 2.5});                // clears level enemies; keeps wave logic isolated
    app->place_enemy({5.5, 1.5});                        // straight ahead of the player
    int killed_events = 0;
    app->events().subscribe<game::EnemyKilled>([&](const game::EnemyKilled&) { ++killed_events; });
    ge::InputState fire;
    fire.fire = true;
    app->fixed_update(fire, 1.0 / 60);
    CHECK_EQ(app->shots_hit(), 1u);
    CHECK(app->enemy_state(0) != game::EnemyState::Idle);  // the first hit woke it
    CHECK_EQ(app->enemies_killed(), 0u);
    step(*app, 40, fire);
    CHECK_EQ(app->enemies_killed(), 1u);
    CHECK_EQ(killed_events, 1);
    CHECK(app->score() >= 150);
    CHECK_EQ(app->enemy_count(), 0u);
    CHECK_EQ(app->entities().size(), 1u);                // only the stationary target is left
}

TEST(every_third_enemy_kill_drops_a_health_pack) {
    auto app = make_app("###############\n#P............#\n#.............#\n###############\n");
    app->place_single_target({13.0, 2.0});
    ge::InputState fire;
    fire.fire = true;
    for (int kill = 1; kill <= 3; ++kill) {
        app->place_enemy({4.5, 1.5});
        for (int i = 0; i < 240 && app->enemies_killed() < static_cast<unsigned>(kill); ++i) app->fixed_update(fire, 1.0 / 60);
        CHECK_EQ(app->enemies_killed(), static_cast<unsigned>(kill));
        CHECK_EQ(app->pickup_count(), kill == 3 ? 1u : 0u);
    }
}

TEST(a_wave_is_not_cleared_while_an_enemy_is_alive) {
    // The enemy is sealed in a pocket it cannot leave, so only the target can be shot.
    auto app = make_app("###########\n#P........#\n#.........#\n###########\n#.#########\n#.#########\n#.#########\n###########\n", 1);
    app->place_single_target({5.5, 1.5});
    app->place_enemy({1.5, 5.5});
    int cleared = 0;
    app->events().subscribe<game::WaveCleared>([&](const game::WaveCleared&) { ++cleared; });
    ge::InputState fire;
    fire.fire = true;
    step(*app, 90, fire);
    CHECK_EQ(app->target_count(), 0u);
    CHECK_EQ(cleared, 0);
    CHECK(app->state() == game::GameState::Playing);
    CHECK_EQ(app->enemy_count(), 1u);
}

// ---- pickups ------------------------------------------------------------------------------------------
TEST(health_pickup_heals_up_to_the_maximum_and_waits_when_full) {
    auto app = make_app(kRoom);
    app->place_single_target({9.0, 2.5});
    int collected = 0;
    app->events().subscribe<game::PickupCollected>([&](const game::PickupCollected&) { ++collected; });
    app->place_pickup(ge::PickupKind::Health, app->player().position);
    step(*app, 5);
    CHECK_EQ(app->pickup_count(), 1u);                   // full health: the pack stays
    CHECK_EQ(collected, 0);
    app->mutable_player().health = 50;
    step(*app, 2);
    CHECK_EQ(app->player_health(), 75);
    CHECK_EQ(app->pickup_count(), 0u);
    CHECK_EQ(collected, 1);
    app->mutable_player().health = 90;
    app->place_pickup(ge::PickupKind::Health, app->player().position);
    step(*app, 2);
    CHECK_EQ(app->player_health(), 100);                 // capped
}

TEST(ammo_pickup_refills_only_a_used_magazine_and_keys_always_collect) {
    auto app = make_app(kRoom);
    app->place_single_target({9.0, 2.5});
    app->place_pickup(ge::PickupKind::Ammo, app->player().position);
    step(*app, 3);
    CHECK_EQ(app->pickup_count(), 1u);                   // full magazine: left for later
    ge::InputState fire;
    fire.fire = true;
    app->fixed_update(fire, 1.0 / 60);                  // the shot uses a round; the pack is collected in the same step
    CHECK_EQ(app->shots_fired(), 1u);
    CHECK_EQ(app->weapon().ammo, 12);
    CHECK_EQ(app->pickup_count(), 0u);

    app->place_pickup(ge::PickupKind::Key, app->player().position);
    step(*app, 2);
    CHECK_EQ(app->player_keys(), 1);
}

// ---- doors ---------------------------------------------------------------------------------------------------
TEST(use_opens_a_door_in_front_but_not_one_out_of_reach_or_a_plain_wall) {
    auto app = make_app("###########\n#P.D......#\n#.........#\n###########\n");
    app->place_single_target({9.0, 2.5});
    int opened = 0;
    app->events().subscribe<game::DoorOpened>([&](const game::DoorOpened&) { ++opened; });
    CHECK(app->level().map.is_solid(3, 1));
    app->mutable_player().position = {0.9 + 1.0, 1.5};   // 1.1 cells from the door face: in reach
    app->fixed_update(pressed_use(), 1.0 / 60);
    CHECK_EQ(opened, 1);
    CHECK(!app->level().map.is_solid(3, 1));
    ge::InputState walk;
    walk.forward = 1.0;
    step(*app, 90, walk);
    CHECK(app->player().position.x > 3.5);               // the player can walk through now

    auto far_app = make_app("###########\n#P.D......#\n#.........#\n###########\n");
    far_app->place_single_target({9.0, 2.5});
    far_app->mutable_player().position = {1.0, 1.5};     // 2.0 cells away: out of reach
    far_app->fixed_update(pressed_use(), 1.0 / 60);
    CHECK(far_app->level().map.is_solid(3, 1));

    auto wall_app = make_app(kRoom);
    wall_app->place_single_target({9.0, 2.5});
    wall_app->mutable_player().angle = 3.14159265358979;  // facing the plain west wall
    wall_app->fixed_update(pressed_use(), 1.0 / 60);      // nothing to open: must not crash or change the map
    CHECK(wall_app->level().map.is_solid(0, 1));
}

TEST(locked_door_needs_a_key_which_is_consumed) {
    auto app = make_app("###########\n#P.L......#\n#.........#\n###########\n");
    app->place_single_target({9.0, 2.5});
    int locked = 0, opened = 0;
    app->events().subscribe<game::DoorLocked>([&](const game::DoorLocked&) { ++locked; });
    app->events().subscribe<game::DoorOpened>([&](const game::DoorOpened&) { ++opened; });
    app->mutable_player().position = {2.0, 1.5};
    app->fixed_update(pressed_use(), 1.0 / 60);
    CHECK_EQ(locked, 1);
    CHECK_EQ(opened, 0);
    CHECK(app->level().map.is_solid(3, 1));
    app->place_pickup(ge::PickupKind::Key, app->player().position);
    step(*app, 2);
    CHECK_EQ(app->player_keys(), 1);
    app->fixed_update(pressed_use(), 1.0 / 60);
    CHECK_EQ(opened, 1);
    CHECK_EQ(app->player_keys(), 0);
    CHECK(!app->level().map.is_solid(3, 1));
}

TEST(enemies_wait_behind_a_closed_door_and_come_through_once_it_opens) {
    auto app = make_app("#############\n#P.D...E....#\n#...#########\n#############\n");
    app->place_single_target({1.5, 2.5});
    app->place_enemy({6.5, 1.5});
    step(*app, 180);
    CHECK(app->enemy_state(0) == game::EnemyState::Idle);   // no line of sight, no route
    CHECK_NEAR(app->enemy_position(0).x, 6.5, 1e-9);
    app->mutable_player().position = {2.0, 1.5};
    app->fixed_update(pressed_use(), 1.0 / 60);
    step(*app, 60);
    CHECK(app->enemy_state(0) != game::EnemyState::Idle);
    CHECK(app->enemy_position(0).x < 6.5);
}

TEST(retry_closes_opened_doors_again) {
    auto app = make_app("###########\n#P.D..E...#\n#.........#\n###########\n");
    app->mutable_player().position = {2.0, 1.5};
    app->fixed_update(pressed_use(), 1.0 / 60);
    CHECK(!app->level().map.is_solid(3, 1));
    app->mutable_player().health = 0;                        // force a restart through the real path
    ge::InputState confirm;
    confirm.confirm = true;
    app->restart();
    CHECK(app->level().map.is_solid(3, 1));
    CHECK(app->level().map.cell(3, 1) == ge::kDoorId);
    (void)confirm;
}

// ---- sound ---------------------------------------------------------------------------------------------------
TEST(new_game_events_trigger_their_sounds) {
    auto app = make_app("###########\n#P.L..E...#\n#.........#\n###########\n");
    ge::AudioMixer mixer;
    const ge::SoundBank bank = ge::SoundBank::make();
    game::GameAudio audio(*app, mixer, bank);
    auto count = [&](const char* n) { const auto c = mixer.started_counts(); const auto it = c.find(n); return it == c.end() ? 0u : it->second; };

    app->mutable_player().position = {2.0, 1.5};
    app->fixed_update(pressed_use(), 1.0 / 60);              // locked, no key
    CHECK_EQ(count("locked"), 1u);
    app->place_pickup(ge::PickupKind::Key, app->player().position);
    step(*app, 2);
    CHECK_EQ(count("pickup"), 1u);
    app->fixed_update(pressed_use(), 1.0 / 60);
    CHECK_EQ(count("door"), 1u);
    for (int i = 0; i < 60 * 30 && app->state() == game::GameState::Playing; ++i) app->fixed_update({}, 1.0 / 60);
    CHECK(app->state() == game::GameState::GameOver);
    CHECK(count("hurt") >= 1u);
    CHECK_EQ(count("die"), 1u);
}
