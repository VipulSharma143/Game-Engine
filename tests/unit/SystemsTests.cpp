#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <queue>
#include <string>
#include <vector>

#include "TestFramework.hpp"
#include "engine/core/Config.hpp"
#include "engine/core/EventBus.hpp"
#include "engine/core/Settings.hpp"
#include "engine/resources/AssetPath.hpp"
#include "engine/resources/ResourceManager.hpp"
#include "engine/world/LevelLoader.hpp"
#include "game/GameApplication.hpp"

namespace fs = std::filesystem;

namespace {

// A throwaway directory removed at scope exit.
struct TempDir {
    fs::path path;
    TempDir() {
        static int counter = 0;
        path = fs::temp_directory_path() / ("ge_test_" + std::to_string(std::rand()) + "_" + std::to_string(++counter));
        fs::create_directories(path);
    }
    ~TempDir() { std::error_code ec; fs::remove_all(path, ec); }
};

void write_file(const fs::path& p, const std::string& text) {
    fs::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary) << text;
}

const char* kGoodLevel =
    "; comment\n"
    "name: Test Room\n"
    "angle: 90\n"
    "waves: 2\n"
    "map:\n"
    "#####\n"
    "#P.T#\n"
    "#..T#\n"
    "#####\n";

std::string load_error(const std::string& text) {
    std::string error;
    CHECK(!ge::parse_level_text(text, error).has_value());
    return error;
}

bool contains(const std::string& s, const std::string& part) { return s.find(part) != std::string::npos; }

// Every cell reachable from `start` (4-connected). Doors are passable; locked doors only when
// `locked_open` is true.
std::vector<bool> flood(const ge::GridMap& m, ge::Vec2 start, bool locked_open = false) {
    std::vector<bool> seen(static_cast<std::size_t>(m.width() * m.height()), false);
    std::queue<std::pair<int, int>> q;
    q.push({static_cast<int>(start.x), static_cast<int>(start.y)});
    auto passable = [&](int x, int y) {
        if (!m.is_solid(x, y)) return true;
        const std::uint8_t id = m.cell(x, y);
        return id == ge::kDoorId || (locked_open && id == ge::kLockedDoorId);
    };
    while (!q.empty()) {
        auto [x, y] = q.front();
        q.pop();
        if (x < 0 || y < 0 || x >= m.width() || y >= m.height() || !passable(x, y)) continue;
        auto idx = static_cast<std::size_t>(y * m.width() + x);
        if (seen[idx]) continue;
        seen[idx] = true;
        q.push({x + 1, y}); q.push({x - 1, y}); q.push({x, y + 1}); q.push({x, y - 1});
    }
    return seen;
}

}  // namespace

// ---- level files ------------------------------------------------------------------------------
TEST(level_text_parses_header_map_and_target_spawns) {
    std::string error;
    const auto level = ge::parse_level_text(kGoodLevel, error);
    CHECK(level.has_value());
    if (!level) return;
    CHECK(level->name == "Test Room");
    CHECK_NEAR(level->spawn_angle, ge::kPi / 2.0, 1e-12);
    CHECK_EQ(level->waves_to_win, 2);
    CHECK_EQ(level->map.width(), 5);
    CHECK_EQ(level->map.height(), 4);
    CHECK_NEAR(level->spawn.x, 1.5, 0.0);
    CHECK_EQ(level->target_spawns.size(), 2u);
    CHECK_NEAR(level->target_spawns[1].y, 2.5, 0.0);
}

TEST(level_text_accepts_windows_line_endings) {
    std::string text = kGoodLevel, crlf;
    for (char c : text) { if (c == '\n') crlf += '\r'; crlf += c; }
    std::string error;
    CHECK(ge::parse_level_text(crlf, error).has_value());
}

TEST(level_text_errors_are_specific) {
    CHECK(contains(load_error("name: x\n"), "map"));
    CHECK(contains(load_error("map:\n###\n#.#\n###\n"), "player spawn"));
    CHECK(contains(load_error("map:\n####\n#P..\n####\n"), "border must be solid"));
    CHECK(contains(load_error("map:\n####\n#P.#\n###\n"), "different length"));
    CHECK(contains(load_error("map:\n##\n##\n"), "3x3"));
    CHECK(contains(load_error("name: a\nbogus: 1\nmap:\n###\n#P#\n###\n"), "line 2"));
    CHECK(contains(load_error("angle: east\nmap:\n###\n#P#\n###\n"), "angle must be a number"));
    CHECK(contains(load_error("waves: 0\nmap:\n###\n#P#\n###\n"), "waves"));
    CHECK(contains(load_error("just some text\n"), "expected"));
    CHECK(contains(load_error("map:\n###\n#P#\n#Z#\n###\n"), "unknown map character"));
    std::string huge = "map:\n" + std::string(129, '#') + "\n#P" + std::string(127, '.') + "#\n" + std::string(129, '#') + "\n";
    CHECK(contains(load_error(huge), "larger than"));
}

TEST(level_file_loading_reports_missing_file_and_prefixes_parse_errors) {
    TempDir dir;
    std::string error;
    CHECK(!ge::load_level_file(dir.path / "nope.txt", error).has_value());
    CHECK(contains(error, "cannot open"));
    write_file(dir.path / "bad.txt", "map:\n###\n#.#\n###\n");
    CHECK(!ge::load_level_file(dir.path / "bad.txt", error).has_value());
    CHECK(contains(error, "bad.txt"));
    write_file(dir.path / "good.txt", kGoodLevel);
    CHECK(ge::load_level_file(dir.path / "good.txt", error).has_value());
}

TEST(shipped_levels_load_are_reachable_and_playable) {
    ge::ResourceManager rm(GE_ASSETS_DIR);
    const std::vector<std::string> names = rm.list_levels();
    CHECK(names.size() >= 3u);
    for (const std::string& name : names) {
        std::string error;
        const auto level = rm.load_level(name, error);
        CHECK(level.has_value());
        if (!level) { std::fprintf(stderr, "  %s: %s\n", name.c_str(), error.c_str()); continue; }
        // Without opening any locked door, every key must be reachable (otherwise the level is a dead end)...
        const std::vector<bool> before_locked = flood(level->map, level->spawn, false);
        // ...and with locked doors open, everything the player has to reach or collect must be reachable.
        const std::vector<bool> everything = flood(level->map, level->spawn, true);
        auto at = [&](const std::vector<bool>& reach, ge::Vec2 p) {
            return reach[static_cast<std::size_t>(static_cast<int>(p.y) * level->map.width() + static_cast<int>(p.x))];
        };
        for (const ge::PickupSpawn& p : level->pickups) {
            CHECK(at(everything, p.position));
            if (p.kind == ge::PickupKind::Key) CHECK(at(before_locked, p.position));
        }
        for (const ge::Vec2& t : level->target_spawns) CHECK(at(everything, t));  // unreachable targets make a level unwinnable
        for (const ge::Vec2& e : level->enemy_spawns) CHECK(at(everything, e));
        int locked = 0, keys = 0;
        for (int y = 0; y < level->map.height(); ++y)
            for (int x = 0; x < level->map.width(); ++x)
                if (level->map.cell(x, y) == ge::kLockedDoorId) ++locked;
        for (const ge::PickupSpawn& p : level->pickups) keys += p.kind == ge::PickupKind::Key ? 1 : 0;
        CHECK(keys >= locked);  // one key opens one locked door
        game::GameApplication app(*level);
        ge::EngineConfig cfg;
        CHECK(app.initialize(cfg).ok);
        CHECK(app.target_count() >= 8u);
    }
}

// ---- assets and resources -------------------------------------------------------------------------
TEST(assets_dir_detection_and_env_override) {
    TempDir dir;
    CHECK(!ge::looks_like_assets_dir(dir.path));
    fs::create_directories(dir.path / "levels");
    CHECK(ge::looks_like_assets_dir(dir.path));
#if defined(_WIN32)
    _putenv_s("GE_ASSETS", dir.path.string().c_str());
#else
    setenv("GE_ASSETS", dir.path.string().c_str(), 1);
#endif
    const auto candidates = ge::asset_dir_candidates("/some/where/build/release/funkshot");
    CHECK(candidates.size() >= 5u);
    CHECK(candidates[0] == dir.path);
    const auto found = ge::find_assets_dir("/some/where/build/release/funkshot");
    CHECK(found.has_value());
#if defined(_WIN32)
    _putenv_s("GE_ASSETS", "");
#else
    unsetenv("GE_ASSETS");
#endif
}

// An installed program looks for its data in share/<its own name>/assets, so FunkShot and SORTED Viewer each find theirs.
TEST(asset_search_uses_the_executable_name_for_the_installed_layout) {
    const auto contains = [](const std::vector<fs::path>& list, const fs::path& wanted) {
        for (const fs::path& c : list)
            if (c.lexically_normal() == wanted.lexically_normal()) return true;
        return false;
    };
    CHECK(contains(ge::asset_dir_candidates("/opt/x/bin/sorted-viewer"), "/opt/x/share/sorted-viewer/assets"));
    CHECK(contains(ge::asset_dir_candidates("/opt/x/bin/funkshot"), "/opt/x/share/funkshot/assets"));
    CHECK(!contains(ge::asset_dir_candidates("/opt/x/bin/funkshot"), "/opt/x/share/sorted-viewer/assets"));
}

#if defined(__linux__)
// Started through PATH (an installed package), argv[0] has no directory; the search must still start from the
// real location of the executable, not from the current directory.
TEST(asset_search_resolves_a_bare_executable_name) {
    std::error_code ec;
    const fs::path self = fs::read_symlink("/proc/self/exe", ec);
    CHECK(!ec);
    const auto candidates = ge::asset_dir_candidates("funkshot");
    bool found_next_to_exe = false;
    for (const fs::path& c : candidates) found_next_to_exe = found_next_to_exe || c == fs::absolute(self.parent_path()) / "assets";
    CHECK(found_next_to_exe);
}
#endif

TEST(resource_manager_caches_and_rejects_unsafe_paths) {
    TempDir dir;
    write_file(dir.path / "levels" / "b.txt", kGoodLevel);
    write_file(dir.path / "levels" / "a.txt", kGoodLevel);
    write_file(dir.path / "notes.txt", "hello");
    ge::ResourceManager rm(dir.path);
    std::string error;
    const std::string* first = rm.read_text("notes.txt", error);
    CHECK(first != nullptr && *first == "hello");
    const std::string* second = rm.read_text("notes.txt", error);
    CHECK(second == first);                 // same cached object
    CHECK_EQ(rm.disk_reads(), 1u);
    CHECK(rm.read_text("../secret.txt", error) == nullptr);
    CHECK(contains(error, ".."));
    CHECK(rm.read_text("/etc/passwd", error) == nullptr);
    CHECK(contains(error, "relative"));
    CHECK(rm.read_text("missing.txt", error) == nullptr);
    CHECK(contains(error, "cannot open"));

    const auto names = rm.list_levels();
    CHECK_EQ(names.size(), 2u);
    CHECK(names[0] == "a" && names[1] == "b");  // sorted
    CHECK(rm.load_level("a", error).has_value());
    CHECK(!rm.load_level("zzz", error).has_value());
    CHECK(contains(error, "available levels: a b"));
    CHECK(rm.load_level((dir.path / "levels" / "b.txt").string(), error).has_value());  // explicit path
}

// ---- settings ---------------------------------------------------------------------------------------
TEST(settings_apply_values_keys_and_comments) {
    ge::EngineConfig cfg;
    const ge::Status s = ge::apply_settings_text(
        "; comment\n# another\nwidth = 640\nheight=400\nfov = 75\ntextured = off\nmouse_sensitivity = 0.004\n"
        "level = arena\n[keys]\nfire = Return, Space\nforward = Up\n", cfg);
    CHECK(s.ok);
    CHECK_EQ(cfg.width, 640);
    CHECK_EQ(cfg.height, 400);
    CHECK_NEAR(cfg.fov_degrees, 75.0, 0.0);
    CHECK(!cfg.textured);
    CHECK_NEAR(cfg.mouse_sensitivity, 0.004, 0.0);
    CHECK(cfg.level == "arena");
    CHECK(cfg.keys.fire == "Return, Space");
    const auto fire_keys = ge::split_key_list(cfg.keys.fire);
    CHECK_EQ(fire_keys.size(), 2u);
    CHECK(fire_keys[1] == "Space");
    CHECK(cfg.keys.backward == "S,Down");  // untouched defaults remain
}

TEST(settings_errors_name_the_line_and_change_nothing) {
    ge::EngineConfig cfg;
    auto fail_msg = [&](const std::string& text) {
        const ge::Status s = ge::apply_settings_text(text, cfg);
        CHECK(!s.ok);
        return s.message;
    };
    CHECK(contains(fail_msg("width = 320\nwdith = 5\n"), "line 2"));
    CHECK(contains(fail_msg("width = abc\n"), "integer"));
    CHECK(contains(fail_msg("textured = maybe\n"), "true or false"));
    CHECK(contains(fail_msg("[keys]\njump = Space\n"), "unknown key binding 'jump'"));
    CHECK(contains(fail_msg("[nope]\n"), "unknown section"));
    CHECK(contains(fail_msg("width 5\n"), "key = value"));
    CHECK(contains(fail_msg("mouse_sensitivity = 5\n"), "mouse_sensitivity"));
    CHECK(contains(fail_msg("width = 4\n"), "width must be"));  // fails validation after parsing
    CHECK(contains(fail_msg("width = 800\nfov = 500\n"), "fov"));
    CHECK_EQ(cfg.width, 320);  // the earlier valid "width = 800" was not applied
}

TEST(settings_file_missing_and_shipped_default_is_valid) {
    ge::EngineConfig cfg;
    CHECK(!ge::apply_settings_file("/definitely/not/here.ini", cfg).ok);
    CHECK(ge::apply_settings_file(fs::path(GE_ASSETS_DIR) / "settings.ini", cfg).ok);
    CHECK_EQ(cfg.width, 320);
    CHECK(cfg.keys.fire == "Space");
}

TEST(command_line_level_and_settings_options_parse) {
    ge::EngineConfig cfg;
    const char* argv[] = {"ge", "--level", "arena", "--settings", "my.ini", "--assets", "/x", "--list-levels", "--no-menu", "--flat"};
    const auto r = ge::parse_arguments(10, argv, cfg);
    CHECK(r.ok);
    CHECK(cfg.level == "arena" && cfg.settings_path == "my.ini" && cfg.assets_dir == "/x");
    CHECK(cfg.list_levels && !cfg.textured && !cfg.start_in_menu);
    const char* bad[] = {"ge", "--level"};
    CHECK(!ge::parse_arguments(2, bad, cfg).ok);
    const char* k[] = {"ge", "--fog-kernel", "sse2"};
    CHECK(ge::parse_arguments(3, k, cfg).ok && cfg.fog_kernel == "sse2");
    const char* kbad[] = {"ge", "--fog-kernel", "gpu"};
    CHECK(!ge::parse_arguments(3, kbad, cfg).ok);
}

// ---- event bus ----------------------------------------------------------------------------------------
namespace {
struct Ping { int value; };
struct Pong { int value; };
}  // namespace

TEST(event_bus_delivers_in_order_by_type_and_unsubscribes) {
    ge::EventBus bus;
    std::vector<int> seen;
    int pongs = 0;
    const auto t1 = bus.subscribe<Ping>([&](const Ping& p) { seen.push_back(p.value); });
    bus.subscribe<Ping>([&](const Ping& p) { seen.push_back(p.value * 10); });
    bus.subscribe<Pong>([&](const Pong&) { ++pongs; });
    bus.publish(Ping{1});
    bus.publish(Pong{0});
    CHECK_EQ(seen.size(), 2u);
    CHECK(seen[0] == 1 && seen[1] == 10);  // subscription order
    CHECK_EQ(pongs, 1);
    bus.unsubscribe<Ping>(t1);
    bus.publish(Ping{2});
    CHECK_EQ(seen.size(), 3u);
    CHECK_EQ(seen[2], 20);
    CHECK_EQ(bus.subscriber_count<Ping>(), 1u);
    bus.publish(Pong{0});
    CHECK_EQ(pongs, 2);
}

TEST(event_bus_allows_subscribing_while_publishing) {
    ge::EventBus bus;
    int late = 0;
    bus.subscribe<Ping>([&](const Ping&) { bus.subscribe<Pong>([&](const Pong&) { ++late; }); });
    bus.publish(Ping{0});
    bus.publish(Pong{0});
    CHECK_EQ(late, 1);
}

// ---- game states ------------------------------------------------------------------------------------------
namespace {
game::GameApplication* make_menu_game(std::string_view level_text, std::unique_ptr<game::GameApplication>& holder, bool menu = true) {
    std::string error;
    auto level = ge::parse_level_text(level_text, error);
    holder = std::make_unique<game::GameApplication>(*level);
    ge::EngineConfig cfg;
    cfg.start_in_menu = menu;
    CHECK(holder->initialize(cfg).ok);
    return holder.get();
}
}  // namespace

TEST(game_state_machine_menu_play_pause_quit) {
    std::unique_ptr<game::GameApplication> holder;
    auto* app = make_menu_game("map:\n#######\n#P....#\n#.....#\n#.....#\n#######\n", holder);
    std::vector<game::StateChanged> changes;
    app->events().subscribe<game::StateChanged>([&](const game::StateChanged& c) { changes.push_back(c); });

    CHECK(app->state() == game::GameState::Menu);
    CHECK(!app->wants_mouse_capture());
    ge::InputState idle;
    const ge::Vec2 before = app->player().position;
    ge::InputState walk;
    walk.forward = 1.0;
    for (int i = 0; i < 30; ++i) app->fixed_update(walk, 1.0 / 60);
    CHECK_NEAR(app->player().position.x, before.x, 0.0);  // the menu does not move the player

    ge::InputState confirm;
    confirm.confirm = true;
    app->fixed_update(confirm, 1.0 / 60);
    CHECK(app->state() == game::GameState::Playing);
    CHECK(app->wants_mouse_capture());

    for (int i = 0; i < 30; ++i) app->fixed_update(walk, 1.0 / 60);
    CHECK(app->player().position.x > before.x);

    ge::InputState pause;
    pause.pause = true;
    app->fixed_update(pause, 1.0 / 60);
    CHECK(app->state() == game::GameState::Paused);
    CHECK(!app->wants_mouse_capture());
    const ge::Vec2 paused_at = app->player().position;
    const ge::Vec2 target_at = app->target_position(0);
    for (int i = 0; i < 60; ++i) app->fixed_update(walk, 1.0 / 60);
    CHECK_NEAR(app->player().position.x, paused_at.x, 0.0);     // frozen
    CHECK_NEAR(app->target_position(0).x, target_at.x, 0.0);    // world frozen too

    app->fixed_update(pause, 1.0 / 60);  // resume
    CHECK(app->state() == game::GameState::Playing);

    app->fixed_update(pause, 1.0 / 60);
    ge::InputState q;
    q.menu_quit = true;
    app->fixed_update(q, 1.0 / 60);
    CHECK(app->state() == game::GameState::Quit);
    CHECK(app->wants_quit());

    CHECK_EQ(changes.size(), 5u);  // Menu->Playing->Paused->Playing->Paused->Quit
    CHECK(changes[0].from == game::GameState::Menu && changes[0].to == game::GameState::Playing);
    CHECK(changes.back().to == game::GameState::Quit);
}

TEST(game_without_menu_starts_playing_and_window_close_quits) {
    std::unique_ptr<game::GameApplication> holder;
    auto* app = make_menu_game("map:\n#####\n#P..#\n#...#\n#####\n", holder, false);
    CHECK(app->state() == game::GameState::Playing);
    ge::InputState close;
    close.quit = true;
    app->fixed_update(close, 1.0 / 60);
    CHECK(app->state() == game::GameState::Quit);
}

TEST(game_victory_after_last_wave_then_restart) {
    std::unique_ptr<game::GameApplication> holder;
    auto* app = make_menu_game("waves: 1\nmap:\n#########\n#P......#\n#.......#\n#########\n", holder, false);
    int won = 0, cleared = 0, destroyed = 0, started = 0;
    app->events().subscribe<game::GameWon>([&](const game::GameWon&) { ++won; });
    app->events().subscribe<game::WaveCleared>([&](const game::WaveCleared&) { ++cleared; });
    app->events().subscribe<game::TargetDestroyed>([&](const game::TargetDestroyed&) { ++destroyed; });
    app->events().subscribe<game::WaveStarted>([&](const game::WaveStarted&) { ++started; });

    const ge::Vec2 p = app->player().position;
    app->place_single_target({p.x + 2.0, p.y});
    ge::InputState fire;
    fire.fire = true;
    for (int i = 0; i < 90 && app->state() == game::GameState::Playing; ++i) app->fixed_update(fire, 1.0 / 60);
    CHECK(app->state() == game::GameState::Victory);
    CHECK_EQ(won, 1);
    CHECK_EQ(cleared, 1);
    CHECK_EQ(destroyed, 1);
    CHECK(app->score() >= 350);  // kill + wave bonus

    ge::InputState confirm;
    confirm.confirm = true;
    app->fixed_update(confirm, 1.0 / 60);
    CHECK(app->state() == game::GameState::Playing);
    CHECK_EQ(app->score(), 0);
    CHECK_EQ(app->wave(), 1);
    CHECK_EQ(app->shots_fired(), 0u);
    CHECK_EQ(app->weapon().ammo, app->weapon().magazine);
    CHECK(app->target_count() > 0u);
    CHECK_EQ(started, 1);  // the restart begins wave 1 again (the first wave started before we subscribed)
}

TEST(designed_target_spawns_are_used_first) {
    std::unique_ptr<game::GameApplication> holder;
    auto* app = make_menu_game("map:\n#################\n#P..............#\n#.............T.#\n#T..............#\n#...............#\n#...............#\n#################\n", holder, false);
    CHECK_EQ(app->target_count(), 8u);
    CHECK_NEAR(app->target_position(0).x, 14.5, 0.0);  // the first 'T' in row-major order
    CHECK_NEAR(app->target_position(0).y, 2.5, 0.0);
    CHECK_NEAR(app->target_position(1).x, 1.5, 0.0);
}
