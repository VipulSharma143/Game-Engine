// Engine-level pieces behind the new enemies, weapons and levels: the level format (enemy kinds, weapon pickups, extra
// walls, themes), textures and themes, pixel-art sprites, the particle system and the new sounds.
#include <cstdint>
#include <filesystem>
#include <queue>
#include <set>
#include <string>
#include <vector>

#include "TestFramework.hpp"
#include "engine/audio/Synth.hpp"
#include "engine/renderer/Framebuffer.hpp"
#include "engine/renderer/Particles.hpp"
#include "engine/renderer/Renderer.hpp"
#include "engine/renderer/SpriteImage.hpp"
#include "engine/renderer/SpriteRenderer.hpp"
#include "engine/renderer/Texture.hpp"
#include "engine/world/LevelLoader.hpp"

namespace {

namespace fs = std::filesystem;

ge::Level parse_ok(const std::string& text) {
    std::string error;
    auto level = ge::parse_level_text(text, error);
    if (!level) std::fprintf(stderr, "bad test level: %s\n", error.c_str());
    CHECK(level.has_value());
    return level ? *level : ge::Level{};
}

// Open cells reachable from the player start on foot; doors count as open (the player can unlock them).
std::set<std::pair<int, int>> reachable(const ge::Level& level) {
    std::set<std::pair<int, int>> seen;
    std::queue<std::pair<int, int>> q;
    const std::pair<int, int> start{static_cast<int>(level.spawn.x), static_cast<int>(level.spawn.y)};
    seen.insert(start);
    q.push(start);
    while (!q.empty()) {
        const auto [x, y] = q.front();
        q.pop();
        for (const auto& [dx, dy] : {std::pair{1, 0}, std::pair{-1, 0}, std::pair{0, 1}, std::pair{0, -1}}) {
            const int nx = x + dx, ny = y + dy;
            const std::uint8_t id = level.map.cell(nx, ny);
            const bool open = id == 0 || id == ge::kDoorId || id == ge::kLockedDoorId;
            if (open && seen.insert({nx, ny}).second) q.push({nx, ny});
        }
    }
    return seen;
}

bool cell_reachable(const std::set<std::pair<int, int>>& cells, ge::Vec2 p) {
    return cells.count({static_cast<int>(p.x), static_cast<int>(p.y)}) != 0;
}

std::uint64_t sum_pixels(const ge::Texture& t) {
    std::uint64_t s = 0;
    for (std::uint32_t p : t.pixels()) s = s * 1099511628211ull + p;
    return s;
}

}  // namespace

// ---- level format ---------------------------------------------------------------------------------------

TEST(level_format_maps_letters_to_enemy_kinds_pickups_walls_and_theme) {
    const ge::Level level = parse_ok("theme: cavern\nmap:\n###########\n#PEGBFSCR.#\n#abcdefHAK#\n###########\n");
    CHECK(level.theme == "cavern");
    CHECK_EQ(level.enemy_spawns.size(), 4u);
    CHECK(level.enemy_spawns[0].kind == ge::EnemyKind::Grunt);
    CHECK(level.enemy_spawns[1].kind == ge::EnemyKind::Gunner);
    CHECK(level.enemy_spawns[2].kind == ge::EnemyKind::Brute);
    CHECK(level.enemy_spawns[3].kind == ge::EnemyKind::Skitter);
    CHECK_EQ(level.pickups.size(), 6u);
    CHECK(level.pickups[0].kind == ge::PickupKind::Shotgun);
    CHECK(level.pickups[1].kind == ge::PickupKind::Chaingun);
    CHECK(level.pickups[2].kind == ge::PickupKind::Launcher);
    for (int i = 0; i < 6; ++i) CHECK_EQ(level.map.cell(1 + i, 2), 12 + i);  // a-f are wall ids 12-17
    CHECK(level.map.is_solid(1, 2));
}

TEST(level_format_rejects_an_unknown_theme_and_defaults_to_the_default_theme) {
    std::string error;
    CHECK(!ge::parse_level_text("theme: swamp\nmap:\n###\n#P#\n###\n", error).has_value());
    CHECK(error.find("swamp") != std::string::npos);
    CHECK(parse_ok("map:\n###\n#P#\n###\n").theme == "default");
}

TEST(new_level_content_round_trips_through_save_and_load) {
    ge::Level level = parse_ok("name: T\ntheme: lab\nmap:\n#########\n#P.GBF..#\n#.SCRHAK#\n#aaffbbd#\n#########\n");
    std::string error;
    const auto text = ge::serialize_level_text(level, error);
    CHECK(text.has_value());
    if (!text) return;
    CHECK(text->find("theme: lab") != std::string::npos);
    const ge::Level again = parse_ok(*text);
    CHECK_EQ(again.enemy_spawns.size(), level.enemy_spawns.size());
    for (std::size_t i = 0; i < level.enemy_spawns.size(); ++i) CHECK(again.enemy_spawns[i].kind == level.enemy_spawns[i].kind);
    for (std::size_t i = 0; i < level.pickups.size(); ++i) CHECK(again.pickups[i].kind == level.pickups[i].kind);
    CHECK(again.theme == "lab");
    for (int x = 0; x < level.map.width(); ++x)
        for (int y = 0; y < level.map.height(); ++y) CHECK_EQ(again.map.cell(x, y), level.map.cell(x, y));
    // The default theme is not written, so older level files stay byte-identical.
    ge::Level plain = level;
    plain.theme = "default";
    CHECK(ge::serialize_level_text(plain, error)->find("theme:") == std::string::npos);
}

TEST(shipped_levels_load_and_everything_in_them_can_be_reached) {
    int levels = 0;
    for (const auto& entry : fs::directory_iterator(fs::path(GE_ASSETS_DIR) / "levels")) {
        if (entry.path().extension() != ".txt") continue;
        std::string error;
        const auto level = ge::load_level_file(entry.path(), error);
        CHECK(level.has_value());
        if (!level) continue;
        ++levels;
        const auto cells = reachable(*level);
        for (const auto& e : level->enemy_spawns) CHECK(cell_reachable(cells, e.position));
        for (const auto& p : level->pickups) CHECK(cell_reachable(cells, p.position));
        for (const auto& t : level->target_spawns) CHECK(cell_reachable(cells, t));
    }
    CHECK(levels >= 7);
}

TEST(the_new_levels_use_their_themes_enemies_and_weapons) {
    struct Expect { const char* file; const char* theme; int min_kinds; };
    for (const Expect& e : {Expect{"foundry", "foundry", 3}, Expect{"crossfire", "lab", 3}, Expect{"catacombs", "cavern", 3}}) {
        std::string error;
        const auto level = ge::load_level_file(fs::path(GE_ASSETS_DIR) / "levels" / (std::string(e.file) + ".txt"), error);
        CHECK(level.has_value());
        if (!level) continue;
        CHECK(level->theme == e.theme);
        CHECK(static_cast<int>(level->enemy_spawns.size()) >= level->waves_to_win);  // every wave has its enemy
        std::set<int> kinds;
        for (const auto& s : level->enemy_spawns) kinds.insert(static_cast<int>(s.kind));
        CHECK(static_cast<int>(kinds.size()) >= e.min_kinds);
        bool weapon = false;
        for (const auto& p : level->pickups) weapon |= p.kind == ge::PickupKind::Shotgun || p.kind == ge::PickupKind::Chaingun || p.kind == ge::PickupKind::Launcher;
        CHECK(weapon);
        bool extra_wall = false;
        for (int y = 0; y < level->map.height(); ++y)
            for (int x = 0; x < level->map.width(); ++x) extra_wall |= level->map.cell(x, y) >= ge::kFirstExtraWallId;
        CHECK(extra_wall);
    }
}

// ---- textures ------------------------------------------------------------------------------------------

TEST(extra_wall_textures_exist_differ_and_are_not_the_missing_texture) {
    const ge::TextureSet tex = ge::TextureSet::make_procedural();
    std::set<std::uint64_t> seen;
    const std::uint64_t missing = sum_pixels(tex.wall(0));
    for (int id = 1; id <= 17; ++id) {
        const std::uint64_t h = sum_pixels(tex.wall(static_cast<std::uint8_t>(id)));
        CHECK(h != missing);
        CHECK(seen.insert(h).second);  // every wall id looks different
    }
    CHECK(sum_pixels(tex.wall(200)) == missing);  // unknown ids still show the marker
}

TEST(themes_change_floor_and_ceiling_but_keep_walls_and_unknown_themes_fall_back) {
    const ge::TextureSet base = ge::TextureSet::make_procedural();
    std::set<std::uint64_t> floors, ceilings;
    for (const char* theme : {"default", "foundry", "cavern", "lab"}) {
        const ge::TextureSet t = ge::TextureSet::make_procedural(theme);
        floors.insert(sum_pixels(t.floor(0)));
        ceilings.insert(sum_pixels(t.ceiling()));
        CHECK(sum_pixels(t.wall(3)) == sum_pixels(base.wall(3)));
        CHECK(sum_pixels(t.wall(15)) == sum_pixels(base.wall(15)));
        CHECK(sum_pixels(ge::TextureSet::make_procedural(theme).floor(1)) == sum_pixels(t.floor(1)));  // deterministic
    }
    CHECK_EQ(floors.size(), 4u);
    CHECK_EQ(ceilings.size(), 4u);
    CHECK(sum_pixels(ge::TextureSet::make_procedural("nonsense").floor(0)) == sum_pixels(base.floor(0)));
}

TEST(the_flat_renderer_draws_every_wall_id_without_the_missing_colour) {
    ge::GridMap map(20, 3, std::vector<std::uint8_t>(60, 0));
    for (int x = 0; x < 20; ++x) { map.set_cell(x, 0, static_cast<std::uint8_t>(x < 18 ? x + 1 : 1)); map.set_cell(x, 2, 1); }
    for (int id = 12; id <= 17; ++id) {
        ge::GridMap m(5, 3, std::vector<std::uint8_t>(15, 1));
        m.set_cell(1, 1, 0); m.set_cell(2, 1, 0); m.set_cell(3, 1, static_cast<std::uint8_t>(id));
        ge::Renderer r;
        ge::Framebuffer fb(32, 20);
        r.render(m, ge::Camera::from_angle({1.5, 1.5}, 0.0, 66.0), fb);
        const std::uint32_t c = fb.pixel(16, 10);
        CHECK(!(ge::red_of(c) > 200 && ge::green_of(c) < 40 && ge::blue_of(c) > 200));  // not magenta
    }
}

// ---- pixel-art sprites ------------------------------------------------------------------------------------

TEST(sprite_image_builds_from_text_rows_and_mirrors) {
    const std::string_view rows[] = {"ab.", "..a"};
    const std::pair<char, std::uint32_t> palette[] = {{'a', 0xFF112233u}, {'b', 0xFF445566u}};
    const ge::SpriteImage img = ge::SpriteImage::from_ascii(rows, palette);
    CHECK_EQ(img.width, 3);
    CHECK_EQ(img.height, 2);
    CHECK_EQ(img.at(0, 0), 0xFF112233u);
    CHECK_EQ(img.at(1, 0), 0xFF445566u);
    CHECK_EQ(img.at(2, 0), 0u);          // '.' is transparent
    CHECK_EQ(img.at(9, 9), 0u);          // outside is transparent
    const ge::SpriteImage flipped = img.mirrored();
    CHECK_EQ(flipped.at(2, 0), 0xFF112233u);
    CHECK_EQ(flipped.at(1, 0), 0xFF445566u);
    const std::string_view ragged[] = {"ab", "a"};
    CHECK(ge::SpriteImage::from_ascii(ragged, palette).empty());  // ragged rows are refused, not guessed
}

TEST(picture_sprites_skip_transparent_pixels_respect_walls_and_light_and_lift) {
    const std::string_view rows[] = {"aaaa", "a..a", "aaaa", "a..a"};
    const std::pair<char, std::uint32_t> palette[] = {{'a', 0xFFFF0000u}};
    const ge::SpriteImage img = ge::SpriteImage::from_ascii(rows, palette);
    const ge::Camera cam = ge::Camera::from_angle({1.0, 5.0}, 0.0, 66.0);
    std::vector<double> far_wall(64, 100.0), near_wall(64, 0.5);

    auto draw = [&](ge::Sprite s, const std::vector<double>& depth) {
        ge::Framebuffer fb(64, 40);
        s.position = {5.0, 5.0};
        s.image = &img;
        const ge::Sprite list[] = {s};
        ge::draw_sprites(cam, depth, list, fb);
        return fb;
    };
    ge::Sprite base;
    base.size = 0.8;
    const ge::Framebuffer open = draw(base, far_wall);
    int red = 0;
    for (std::uint32_t p : open.pixels()) red += ge::red_of(p) > 100 && ge::green_of(p) < 40;
    CHECK(red > 40);                                    // it was drawn

    const ge::Framebuffer hidden = draw(base, near_wall);  // a wall in front hides it completely
    for (std::uint32_t p : hidden.pixels()) CHECK(!(ge::red_of(p) > 100 && ge::green_of(p) < 40));

    ge::Sprite dim = base;
    dim.light = 0.3;
    const ge::Framebuffer dark = draw(dim, far_wall);
    unsigned bright = 0, dimmed = 0;
    for (std::uint32_t p : open.pixels()) bright = std::max(bright, ge::red_of(p));
    for (std::uint32_t p : dark.pixels()) dimmed = std::max(dimmed, ge::red_of(p));
    CHECK(dimmed < bright);

    // Lifting moves it up the screen.
    auto top_row = [&](const ge::Framebuffer& fb) {
        for (int y = 0; y < fb.height(); ++y)
            for (int x = 0; x < fb.width(); ++x)
                if (ge::red_of(fb.pixel(x, y)) > 100 && ge::green_of(fb.pixel(x, y)) < 40) return y;
        return -1;
    };
    ge::Sprite lifted = base;
    lifted.z = 0.3;
    CHECK(top_row(draw(lifted, far_wall)) < top_row(open));
}

TEST(sprites_without_the_new_fields_draw_exactly_as_before) {
    const ge::Camera cam = ge::Camera::from_angle({1.0, 5.0}, 0.0, 66.0);
    std::vector<double> depth(64, 100.0);
    const ge::Sprite plain{{5.0, 5.0}, 0.5, 0xFF40C0FFu, 0.0, false};
    ge::Sprite explicit_defaults = plain;
    explicit_defaults.image = nullptr;
    explicit_defaults.z = 0.0;
    explicit_defaults.light = 1.0;
    ge::Framebuffer a(64, 40), b(64, 40);
    const ge::Sprite la[] = {plain}, lb[] = {explicit_defaults};
    ge::draw_sprites(cam, depth, la, a);
    ge::draw_sprites(cam, depth, lb, b);
    CHECK(a.checksum() == b.checksum());
    CHECK(a.checksum() != ge::Framebuffer(64, 40).checksum());
}

// ---- particles ---------------------------------------------------------------------------------------------

TEST(particles_burst_fall_bounce_and_expire) {
    ge::GridMap open(9, 9, std::vector<std::uint8_t>(81, 0));
    ge::ParticleSystem ps;
    ge::Particle t;
    t.life = 0.5;
    t.gravity = 3.0;
    ps.burst({4.5, 4.5}, 0.2, 20, 1.0, 1.0, t);
    CHECK_EQ(ps.alive(), 20u);
    std::vector<ge::Sprite> sprites;
    ps.append_sprites(sprites);
    CHECK_EQ(sprites.size(), 20u);
    for (int i = 0; i < 20; ++i) ps.step(1.0 / 60.0, open);
    sprites.clear();
    ps.append_sprites(sprites);
    for (const ge::Sprite& s : sprites) CHECK(s.z >= -0.5 - 1e-9);   // never below the floor
    for (int i = 0; i < 60; ++i) ps.step(1.0 / 60.0, open);
    CHECK_EQ(ps.alive(), 0u);                                         // everything expired
}

TEST(particles_die_on_walls_and_the_pool_never_overflows) {
    ge::GridMap box(5, 5, std::vector<std::uint8_t>(25, 0));
    for (int i = 0; i < 5; ++i) { box.set_cell(i, 0, 1); box.set_cell(i, 4, 1); box.set_cell(0, i, 1); box.set_cell(4, i, 1); }
    ge::ParticleSystem ps;
    ge::Particle t;
    t.life = 10.0;
    t.gravity = 0.0;
    ps.burst({2.5, 2.5}, 0.0, 200, 8.0, 0.0, t);
    for (int i = 0; i < 120; ++i) ps.step(1.0 / 60.0, box);
    CHECK_EQ(ps.alive(), 0u);                                         // all flew into the walls
    for (int i = 0; i < 5; ++i) ps.burst({2.5, 2.5}, 0.0, 500, 1.0, 1.0, t);
    CHECK_EQ(ps.alive(), ge::ParticleSystem::kCapacity);              // capped, extra particles dropped
    CHECK(!ps.emit(t));
    ps.clear();
    CHECK_EQ(ps.alive(), 0u);
}

TEST(particles_are_deterministic) {
    ge::GridMap open(9, 9, std::vector<std::uint8_t>(81, 0));
    auto run = [&] {
        ge::ParticleSystem ps;
        ge::Particle t;
        ps.burst({4.5, 4.5}, 0.0, 30, 2.0, 2.0, t);
        for (int i = 0; i < 10; ++i) ps.step(0.02, open);
        std::vector<ge::Sprite> s;
        ps.append_sprites(s);
        return s;
    };
    const auto a = run(), b = run();
    CHECK_EQ(a.size(), b.size());
    for (std::size_t i = 0; i < a.size(); ++i) CHECK(a[i].position.x == b[i].position.x && a[i].position.y == b[i].position.y && a[i].z == b[i].z);
}

// ---- sounds ---------------------------------------------------------------------------------------------------

TEST(the_new_weapon_and_enemy_sounds_exist_and_are_deterministic) {
    const ge::SoundBank a = ge::SoundBank::make(), b = ge::SoundBank::make();
    const ge::Sound* new_sounds[] = {&a.shotgun, &a.chaingun, &a.rocket, &a.explosion, &a.enemy_shot, &a.switch_weapon};
    const ge::Sound* again[] = {&b.shotgun, &b.chaingun, &b.rocket, &b.explosion, &b.enemy_shot, &b.switch_weapon};
    for (int i = 0; i < 6; ++i) {
        CHECK(!new_sounds[i]->samples.empty());
        CHECK(new_sounds[i]->samples == again[i]->samples);
        float peak = 0.0f;
        for (float s : new_sounds[i]->samples) peak = std::max(peak, std::fabs(s));
        CHECK(peak > 0.3f && peak <= 1.0f);
    }
    CHECK(a.explosion.duration_seconds() > a.shotgun.duration_seconds());   // a blast rumbles longer than a gunshot
    CHECK(a.chaingun.duration_seconds() < a.shot.duration_seconds());       // chaingun rounds are short
}
