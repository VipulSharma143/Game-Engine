#include <array>
#include <cmath>
#include <limits>
#include <vector>

#include "TestFramework.hpp"
#include "engine/renderer/SpriteRenderer.hpp"
#include "engine/renderer/TextRenderer.hpp"
#include "engine/world/MotionBatch.hpp"
#include "game/GameApplication.hpp"
#include "game/WeaponSystem.hpp"

namespace {
// 5x3 corridor with a wall at x=2:  . . # . .
ge::GridMap walled_corridor() {
    return ge::GridMap(5, 3, {1, 1, 1, 1, 1,  0, 0, 1, 0, 0,  1, 1, 1, 1, 1});
}
}  // namespace

// ---- hitscan ------------------------------------------------------------------------------
TEST(hitscan_hits_target_in_front) {
    const std::array<game::ShotTarget, 1> t = {{{{1.9, 1.5}, 0.2}}};
    const auto r = game::hitscan(walled_corridor(), {0.5, 1.5}, 0.0, t);
    CHECK_EQ(r.target, 0);
    CHECK_NEAR(r.distance, 1.2, 1e-9);
}

TEST(hitscan_wall_blocks_target_behind_it) {
    const std::array<game::ShotTarget, 1> t = {{{{3.5, 1.5}, 0.2}}};
    const auto r = game::hitscan(walled_corridor(), {0.5, 1.5}, 0.0, t);
    CHECK_EQ(r.target, -1);
    CHECK_NEAR(r.distance, 1.5, 1e-9);  // wall face at x = 2.0
}

TEST(hitscan_ignores_targets_behind_and_misses_to_the_side) {
    const std::array<game::ShotTarget, 2> t = {{{{0.2, 1.5}, 0.1}, {{1.5, 1.9}, 0.1}}};
    CHECK_EQ(game::hitscan(walled_corridor(), {0.6, 1.5}, 0.0, t).target, -1);
}

TEST(hitscan_picks_the_nearest_of_overlapping_targets) {
    const std::array<game::ShotTarget, 2> t = {{{{1.8, 1.5}, 0.1}, {{1.3, 1.5}, 0.1}}};
    CHECK_EQ(game::hitscan(walled_corridor(), {0.5, 1.5}, 0.0, t).target, 1);
}

// ---- weapon ---------------------------------------------------------------------------------
TEST(weapon_fires_respects_cooldown_and_consumes_ammo) {
    game::Weapon w;
    CHECK(game::weapon_step(w, true, false, 1.0 / 60));
    CHECK_EQ(w.ammo, 11);
    CHECK(!game::weapon_step(w, true, false, 1.0 / 60));  // still cooling down
    for (int i = 0; i < 20; ++i) game::weapon_step(w, false, false, 1.0 / 60);
    CHECK(game::weapon_step(w, true, false, 1.0 / 60));
    CHECK_EQ(w.ammo, 10);
}

TEST(weapon_reloads_automatically_when_empty_and_on_request) {
    game::Weapon w;
    w.ammo = 0;
    CHECK(!game::weapon_step(w, true, false, 1.0 / 60));
    CHECK(w.reloading());
    for (int i = 0; i < 30; ++i) CHECK(!game::weapon_step(w, true, false, 1.0 / 60));  // cannot fire while reloading
    CHECK(w.reloading());
    CHECK_EQ(w.ammo, 0);
    for (int i = 0; i < 40; ++i) game::weapon_step(w, false, false, 1.0 / 60);
    CHECK(!w.reloading());
    CHECK_EQ(w.ammo, w.magazine);
    game::Weapon full;
    CHECK(!game::weapon_step(full, false, true, 0.01));  // full magazine: reload request ignored
    CHECK(!full.reloading());
    game::Weapon partial;
    partial.ammo = 5;
    game::weapon_step(partial, false, true, 0.01);
    CHECK(partial.reloading());
    for (int i = 0; i < 70; ++i) game::weapon_step(partial, false, false, 1.0 / 60);
    CHECK_EQ(partial.ammo, partial.magazine);
}

// ---- game ------------------------------------------------------------------------------------
TEST(game_shooting_destroys_target_scores_and_starts_next_wave) {
    game::GameApplication app(ge::test_level());
    ge::EngineConfig cfg;
    CHECK(app.initialize(cfg).ok);
    const ge::Vec2 p = app.player().position;
    const double a = app.player().angle;
    app.place_single_target({p.x + 1.5 * std::cos(a), p.y + 1.5 * std::sin(a)});

    ge::InputState fire;
    fire.fire = true;
    for (int i = 0; i < 60; ++i) app.fixed_update(fire, 1.0 / 60);  // 1 s of fire: 3 hits needed
    CHECK_EQ(app.targets_destroyed(), 1u);
    CHECK(app.shots_hit() >= 3u);
    CHECK(app.score() >= 100);
    CHECK_EQ(app.target_count(), 0u);
    CHECK_EQ(app.entities().size(), 0u);  // the entity was destroyed with the target

    ge::InputState idle;
    for (int i = 0; i < 180; ++i) app.fixed_update(idle, 1.0 / 60);  // wave break is 2 s
    CHECK_EQ(app.wave(), 2);
    CHECK_EQ(app.target_count(), 10u);
}

TEST(game_miss_does_not_score) {
    game::GameApplication app(ge::test_level());
    ge::EngineConfig cfg;
    CHECK(app.initialize(cfg).ok);
    const ge::Vec2 p = app.player().position;
    const double a = app.player().angle;
    app.place_single_target({p.x - 2.0 * std::cos(a), p.y - 2.0 * std::sin(a)});  // behind the player
    ge::InputState fire;
    fire.fire = true;
    for (int i = 0; i < 30; ++i) app.fixed_update(fire, 1.0 / 60);
    CHECK(app.shots_fired() >= 1u);
    CHECK_EQ(app.shots_hit(), 0u);
    CHECK_EQ(app.score(), 0);
}

TEST(game_mouse_look_turns_the_player) {
    game::GameApplication app(ge::test_level());
    ge::EngineConfig cfg;
    CHECK(app.initialize(cfg).ok);
    const double before = app.player().angle;
    ge::InputState look;
    look.look = 100.0;  // pixels
    app.fixed_update(look, 1.0 / 60);
    CHECK_NEAR(app.player().angle - before, 100.0 * app.player().mouse_sensitivity, 1e-9);
}

TEST(game_renders_targets_and_hud_into_the_frame) {
    game::GameApplication app(ge::test_level());
    ge::EngineConfig cfg;
    CHECK(app.initialize(cfg).ok);
    const ge::Vec2 p = app.player().position;
    const double a = app.player().angle;
    app.place_single_target({p.x + 2.5 * std::cos(a), p.y + 2.5 * std::sin(a)});
    ge::Framebuffer with_target(320, 200);
    app.render(with_target);
    // The sphere is cyan-ish (blue/green dominate red) right at screen centre.
    const std::uint32_t c = with_target.pixel(160, 90);
    CHECK(ge::blue_of(c) > ge::red_of(c));
}

// ---- sprites ---------------------------------------------------------------------------------
TEST(project_point_places_points_relative_to_the_camera) {
    const ge::Camera cam = ge::Camera::from_angle({0.0, 0.0}, 0.0, 66.0);
    const auto ahead = ge::project_point(cam, {4.0, 0.0}, 320);
    CHECK(ahead.visible);
    CHECK_NEAR(ahead.depth, 4.0, 1e-9);
    CHECK_NEAR(ahead.screen_x, 160.0, 1e-9);
    CHECK(!ge::project_point(cam, {-4.0, 0.0}, 320).visible);
    // A point to the right (+y on the y-down map when facing +x) lands right of centre.
    CHECK(ge::project_point(cam, {4.0, 1.0}, 320).screen_x > 160.0);
    CHECK(ge::project_point(cam, {4.0, -1.0}, 320).screen_x < 160.0);
}

TEST(sprites_are_hidden_behind_walls_and_drawn_when_closer) {
    const ge::Camera cam = ge::Camera::from_angle({0.0, 0.0}, 0.0, 66.0);
    ge::Framebuffer fb(64, 40);
    fb.clear(ge::pack_rgb(0, 0, 0));
    const std::vector<double> near_wall(64, 2.0), far_wall(64, 50.0);
    const std::array<ge::Sprite, 1> s = {{{{4.0, 0.0}, 1.0, ge::pack_rgb(200, 100, 50), 0.0}}};
    CHECK_EQ(ge::draw_sprites(cam, near_wall, s, fb), 0);  // wall at 2, sprite at 4
    CHECK_EQ(fb.pixel(32, 20), ge::pack_rgb(0, 0, 0));
    CHECK_EQ(ge::draw_sprites(cam, far_wall, s, fb), 1);
    CHECK(fb.pixel(32, 20) != ge::pack_rgb(0, 0, 0));
}

// ---- text ------------------------------------------------------------------------------------
TEST(text_renderer_draws_glyph_pixels_and_measures_width) {
    ge::Framebuffer fb(40, 12);
    fb.clear(ge::pack_rgb(0, 0, 0));
    const std::uint32_t white = ge::pack_rgb(255, 255, 255);
    ge::draw_text(fb, 1, 1, "I", white);
    CHECK_EQ(fb.pixel(3, 1), white);   // top of the I's bar, middle column
    CHECK_EQ(fb.pixel(3, 4), white);   // stem
    CHECK_EQ(fb.pixel(1, 4), ge::pack_rgb(0, 0, 0));
    CHECK_EQ(ge::text_width("AB"), 11);
    CHECK_EQ(ge::text_width("AB", 2), 22);
    CHECK_EQ(ge::text_width(""), 0);
}

// ---- motion batch removal ------------------------------------------------------------------
TEST(motion_batch_remove_swaps_last_into_slot) {
    ge::MotionBatch m;
    m.add({1, 1}, {0, 0});
    m.add({2, 2}, {0, 0});
    m.add({3, 3}, {0, 0});
    m.remove(0);
    CHECK_EQ(m.size(), 2u);
    CHECK_NEAR(m.position(0).x, 3.0, 0.0);
    CHECK_NEAR(m.position(1).x, 2.0, 0.0);
    m.remove(5);  // out of range: ignored
    CHECK_EQ(m.size(), 2u);
}

TEST(text_renderer_knows_every_character_the_game_prints) {
    // Unknown characters render as a hollow box; every HUD/menu character must have a real glyph.
    ge::Framebuffer a(16, 10), b(16, 10);
    a.clear(0); b.clear(0);
    const std::uint32_t w = ge::pack_rgb(255, 255, 255);
    ge::draw_text(a, 1, 1, "?", w);  // '?' is not in the table, so it draws the box
    for (char c : std::string("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 :/.-!%+(),<>=")) {
        b.clear(0);
        ge::draw_text(b, 1, 1, std::string(1, c), w);
        if (c != ' ') CHECK(b.checksum() != a.checksum());
    }
}

TEST(diamond_sprites_leave_the_corners_empty_unlike_spheres_which_leave_less) {
    const ge::Camera cam = ge::Camera::from_angle({0.0, 0.0}, 0.0, 66.0);
    const std::vector<double> far_wall(100, 50.0);
    auto fill_count = [&](bool diamond) {
        ge::Framebuffer fb(100, 100);
        fb.clear(ge::pack_rgb(0, 0, 0));
        const std::array<ge::Sprite, 1> s = {{{{3.0, 0.0}, 1.0, ge::pack_rgb(200, 200, 200), 0.0, diamond}}};
        ge::draw_sprites(cam, far_wall, s, fb);
        int lit = 0;
        for (int y = 0; y < 100; ++y)
            for (int x = 0; x < 100; ++x) lit += fb.pixel(x, y) != ge::pack_rgb(0, 0, 0) ? 1 : 0;
        return lit;
    };
    const int sphere = fill_count(false), diamond = fill_count(true);
    CHECK(diamond > 0);
    // Area ratio: a diamond covers half its bounding square, a disc about pi/4 of it.
    CHECK(diamond < sphere);
    CHECK(static_cast<double>(diamond) / sphere > 0.55 && static_cast<double>(diamond) / sphere < 0.8);
}
