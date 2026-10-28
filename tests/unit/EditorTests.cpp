// Level editing: serialization and saving, the LevelEditor operations, crosshair picking and the selection
// highlight. Display-free and deterministic; files go to a temporary folder.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "TestFramework.hpp"
#include "engine/renderer/Raycaster.hpp"
#include "engine/renderer/Renderer.hpp"
#include "engine/renderer/Texture.hpp"
#include "engine/world/LevelEditor.hpp"
#include "engine/world/LevelLoader.hpp"

namespace {

namespace fs = std::filesystem;

std::string read_file(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

void write_file(const fs::path& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << text;
}

// A fresh empty folder for one test.
fs::path temp_dir(const char* name) {
    const fs::path dir = fs::temp_directory_path() / (std::string("sorted_editor_test_") + name);
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    return dir;
}

bool same_points(const std::vector<ge::Vec2>& a, const std::vector<ge::Vec2>& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (a[i].x != b[i].x || a[i].y != b[i].y) return false;
    return true;
}

// Every field of a Level that the level format stores.
bool same_level(const ge::Level& a, const ge::Level& b) {
    if (a.map.width() != b.map.width() || a.map.height() != b.map.height()) return false;
    for (int y = 0; y < a.map.height(); ++y)
        for (int x = 0; x < a.map.width(); ++x)
            if (a.map.cell(x, y) != b.map.cell(x, y)) return false;
    if (a.pickups.size() != b.pickups.size()) return false;
    for (std::size_t i = 0; i < a.pickups.size(); ++i)
        if (a.pickups[i].kind != b.pickups[i].kind || a.pickups[i].position.x != b.pickups[i].position.x ||
            a.pickups[i].position.y != b.pickups[i].position.y) return false;
    return a.name == b.name && std::fabs(a.spawn_angle - b.spawn_angle) < 1e-9 && a.waves_to_win == b.waves_to_win &&
           a.spawn.x == b.spawn.x && a.spawn.y == b.spawn.y && a.comments == b.comments &&
           same_points(a.target_spawns, b.target_spawns) && same_points(a.enemy_spawns, b.enemy_spawns);
}

ge::LevelEditor hall_editor() {
    ge::LevelEditor editor;
    editor.load(ge::test_level());  // 16x16 hall, player start at cell (2,2)
    return editor;
}

ge::Camera look_east_from(double x, double y) { return ge::Camera::from_angle({x, y}, 0.0, 66.0); }

}  // namespace

// ---- serialization and saving ------------------------------------------------------------------------

TEST(every_shipped_level_survives_parse_serialize_parse) {
    int checked = 0;
    for (const auto& entry : fs::directory_iterator(fs::path(GE_ASSETS_DIR) / "levels")) {
        if (entry.path().extension() != ".txt") continue;
        std::string error;
        const auto original = ge::load_level_file(entry.path(), error);
        CHECK(original.has_value());
        if (!original) continue;
        const auto text = ge::serialize_level_text(*original, error);
        CHECK(text.has_value());
        if (!text) continue;
        const auto again = ge::parse_level_text(*text, error);
        CHECK(again.has_value());
        if (again) CHECK(same_level(*original, *again));
        CHECK(!original->comments.empty());  // every shipped level has a description line, and saving keeps it
        ++checked;
    }
    CHECK(checked >= 4);
}

TEST(serializing_an_unchanged_level_reproduces_its_file_byte_for_byte) {
    // arena, bunker and maze write wall ids as digits, which is what the serializer writes ('#' is read as id 1)
    for (const char* name : {"arena", "bunker", "maze"}) {
        const fs::path path = fs::path(GE_ASSETS_DIR) / "levels" / (std::string(name) + ".txt");
        std::string error;
        const auto level = ge::load_level_file(path, error);
        CHECK(level.has_value());
        if (!level) continue;
        const auto text = ge::serialize_level_text(*level, error);
        CHECK(text.has_value());
        if (text) CHECK(*text == read_file(path));
    }
}

TEST(serializing_rejects_levels_the_format_cannot_hold) {
    std::string error;
    auto broken = [&](auto&& mutate) {
        ge::Level level = ge::test_level();
        mutate(level);
        error.clear();
        const bool refused = !ge::serialize_level_text(level, error).has_value();
        return refused && !error.empty();
    };
    CHECK(broken([](ge::Level& l) { l.map.set_cell(0, 5, 0); }));                              // open border
    CHECK(broken([](ge::Level& l) { l.enemy_spawns.push_back({2.5, 2.5}); }));                // same cell as the player
    CHECK(broken([](ge::Level& l) { l.target_spawns.push_back({6.3, 6.5}); }));               // off a cell centre
    CHECK(broken([](ge::Level& l) { l.target_spawns.push_back({3.5, 3.5}); }));               // inside a wall (the 2-block)
    CHECK(broken([](ge::Level& l) { l.map.set_cell(5, 5, 12); }));                            // no file character for id 12
    CHECK(broken([](ge::Level& l) { l.name = "two\nlines"; }));
    CHECK(broken([](ge::Level& l) { l.map = ge::GridMap(2, 2, {1, 1, 1, 1}); }));             // below 3x3
}

TEST(save_level_file_writes_atomically_and_a_refused_save_keeps_the_old_file) {
    const fs::path dir = temp_dir("save");
    const fs::path file = dir / "hall.txt";
    std::string error;

    CHECK(ge::save_level_file(file, ge::test_level(), error));
    CHECK(!fs::exists(dir / "hall.txt.tmp"));
    const std::string good = read_file(file);
    CHECK(!good.empty());

    ge::Level bad = ge::test_level();
    bad.map.set_cell(0, 0, 0);  // open border
    CHECK(!ge::save_level_file(file, bad, error));
    CHECK(error.find("border") != std::string::npos);
    CHECK(read_file(file) == good);              // untouched
    CHECK(!fs::exists(dir / "hall.txt.tmp"));    // nothing left behind

    CHECK(!ge::save_level_file(dir / "missing_folder" / "x.txt", ge::test_level(), error));
    CHECK(!error.empty());
}

// ---- LevelEditor operations -------------------------------------------------------------------------

TEST(editor_inserts_every_palette_item_and_selects_the_new_object) {
    ge::LevelEditor editor = hall_editor();
    CHECK(!editor.dirty());
    for (int i = 0; i < ge::LevelEditor::palette_size(); ++i) {
        editor.cycle_palette(i == 0 ? 0 : 1);
        const int x = 1 + i % 14, y = i < 14 ? 1 : 5;  // open floor in the hall: row 1, then row 5
        const ge::Status s = editor.insert(x, y);
        CHECK(s.ok);
        CHECK(editor.object_at(x, y) == editor.palette_current());
        CHECK(editor.has_selection() && editor.selected_x() == x && editor.selected_y() == y);
    }
    CHECK(editor.dirty());
    CHECK_EQ(editor.level().enemy_spawns.size(), 1u);
    CHECK_EQ(editor.level().target_spawns.size(), 1u);
    CHECK_EQ(editor.level().pickups.size(), 3u);
}

TEST(editor_removes_objects_from_the_level_data_not_just_the_picture) {
    ge::LevelEditor editor = hall_editor();
    editor.cycle_palette(12);  // enemy
    CHECK(editor.insert(5, 5).ok);
    CHECK_EQ(editor.level().enemy_spawns.size(), 1u);
    CHECK(editor.remove_selected().ok);
    CHECK_EQ(editor.level().enemy_spawns.size(), 0u);
    CHECK(editor.object_at(5, 5).kind == ge::ObjectKind::None);

    CHECK(editor.select(3, 3));  // a wall of the 2-block
    CHECK(editor.remove_selected().ok);
    CHECK(!editor.level().map.is_solid(3, 3));
}

TEST(editor_refuses_bad_requests_and_changes_nothing) {
    ge::LevelEditor editor = hall_editor();
    editor.set_player_cell(8, 8);
    const ge::Level before = editor.level();
    const std::uint64_t revision = editor.revision();

    CHECK(!editor.remove_selected().ok);              // nothing selected
    CHECK(!editor.move_selected(5, 5).ok);
    CHECK(!editor.change_selected().ok);
    CHECK(!editor.rotate_selected(1).ok);
    CHECK(!editor.insert(-1, 4).ok);                  // outside the map
    CHECK(!editor.insert(99, 4).ok);
    CHECK(!editor.insert(3, 3).ok);                   // occupied by a wall
    CHECK(!editor.insert(2, 2).ok);                   // occupied by the player start
    CHECK(!editor.insert(0, 4).ok);                   // the border
    CHECK(!editor.insert(8, 8).ok);                   // a wall where the player stands (palette starts at wall 1)
    CHECK(!editor.select(-1, 0));
    CHECK(!editor.select(0, 16));

    CHECK(editor.select(2, 2));                       // the player start
    CHECK(!editor.remove_selected().ok);
    CHECK(!editor.change_selected().ok);
    CHECK(editor.select(0, 0));                       // a border wall
    CHECK(!editor.remove_selected().ok);
    CHECK(!editor.move_selected(5, 5).ok);
    CHECK(editor.select(5, 5));                       // empty floor
    CHECK(!editor.remove_selected().ok);
    CHECK(!editor.rotate_selected(1).ok);

    CHECK(same_level(before, editor.level()));
    CHECK_EQ(editor.revision(), revision);
    CHECK(!editor.dirty());
}

TEST(editor_repeating_a_removal_is_safe) {
    ge::LevelEditor editor = hall_editor();
    CHECK(editor.select(7, 6));
    CHECK(editor.remove_selected().ok);
    for (int i = 0; i < 3; ++i) CHECK(!editor.remove_selected().ok);  // already gone: refused, not a crash
    CHECK(!editor.level().map.is_solid(7, 6));
}

TEST(editor_moves_objects_and_the_player_start_and_follows_them_with_the_selection) {
    ge::LevelEditor editor = hall_editor();
    CHECK(editor.select(7, 6));                       // wall id 1
    CHECK(editor.move_selected(9, 3).ok);
    CHECK(!editor.level().map.is_solid(7, 6));
    CHECK_EQ(editor.level().map.cell(9, 3), 1);
    CHECK(editor.selected_x() == 9 && editor.selected_y() == 3);
    CHECK(!editor.move_selected(3, 3).ok);            // occupied

    CHECK(editor.select(2, 2));                       // player start
    CHECK(editor.move_selected(12, 12).ok);
    CHECK_NEAR(editor.level().spawn.x, 12.5, 0.0);
    CHECK_NEAR(editor.level().spawn.y, 12.5, 0.0);
    CHECK(editor.object_at(2, 2).kind == ge::ObjectKind::None);
}

TEST(editor_change_turns_the_selection_into_the_palette_item_in_place) {
    ge::LevelEditor editor = hall_editor();
    CHECK(editor.select(7, 6));
    editor.cycle_palette(3);                          // wall 4
    CHECK(editor.change_selected().ok);
    CHECK_EQ(editor.level().map.cell(7, 6), 4);
    CHECK(!editor.change_selected().ok);              // already wall 4
    editor.cycle_palette(8);                          // enemy (index 11 is target, 12 enemy)
    CHECK(editor.palette_current().kind == ge::ObjectKind::Target || editor.palette_current().kind == ge::ObjectKind::Enemy);
    CHECK(editor.change_selected().ok);               // wall becomes a spawn
    CHECK(!editor.level().map.is_solid(7, 6));
    CHECK(editor.object_at(7, 6) == editor.palette_current());
    CHECK(editor.select(0, 0));
    CHECK(!editor.change_selected().ok);              // the border cannot become a non-wall
}

TEST(editor_rotates_only_the_player_start_in_fixed_steps_and_wraps) {
    ge::LevelEditor editor = hall_editor();
    CHECK(editor.select(2, 2));
    CHECK(editor.rotate_selected(6).ok);
    CHECK_NEAR(editor.level().spawn_angle, (ge::kPi / 2.0), 1e-9);
    CHECK(editor.rotate_selected(-7).ok);              // below zero wraps round
    CHECK_NEAR(editor.level().spawn_angle, 2.0 * ge::kPi - ge::LevelEditor::kRotateStep, 1e-9);
    CHECK(editor.rotate_selected(25).ok);              // 25 steps = 375 degrees: more than a full turn
    CHECK(editor.level().spawn_angle >= 0.0 && editor.level().spawn_angle < 2.0 * ge::kPi);
}

TEST(editor_never_walls_in_the_player) {
    ge::LevelEditor editor = hall_editor();
    editor.set_player_cell(6, 4);                      // open floor
    const ge::Status walled = editor.insert(6, 4);     // wall palette item on the player's cell
    CHECK(!walled.ok);
    CHECK(walled.message.find("STANDING") != std::string::npos);
    editor.cycle_palette(12);                          // enemy: not solid, allowed
    CHECK(editor.insert(6, 4).ok);
    editor.cycle_palette(-12);

    editor.set_player_cell(8, 12);                     // open floor
    CHECK(editor.select(7, 6));                        // a wall
    const ge::Status moved = editor.move_selected(8, 12);
    CHECK(!moved.ok);
    CHECK(moved.message.find("STANDING") != std::string::npos);
    CHECK(editor.level().map.is_solid(7, 6));          // the wall did not move
    CHECK(editor.move_selected(8, 11).ok);             // next to the player is fine
}

TEST(editor_palette_wraps_in_both_directions) {
    ge::LevelEditor editor;
    editor.load(ge::test_level());
    const int n = ge::LevelEditor::palette_size();
    editor.cycle_palette(-1);
    CHECK_EQ(editor.palette_index(), n - 1);
    editor.cycle_palette(1);
    CHECK_EQ(editor.palette_index(), 0);
    editor.cycle_palette(3 * n + 2);
    CHECK_EQ(editor.palette_index(), 2);
}

TEST(edited_level_in_memory_equals_the_level_loaded_back_from_its_file) {
    const fs::path dir = temp_dir("roundtrip");
    ge::LevelEditor editor;
    std::string error;
    auto base = ge::load_level_file(fs::path(GE_ASSETS_DIR) / "levels" / "bunker.txt", error);
    CHECK(base.has_value());
    if (!base) return;
    editor.load(*base, dir / "bunker.txt");

    // Inserts in a deliberately scrambled order: the editor must keep spawn lists in file order.
    editor.cycle_palette(12);                          // enemy
    CHECK(editor.insert(20, 19 - 1).ok);
    CHECK(editor.insert(2, 18).ok);
    CHECK(editor.insert(12, 18).ok);
    editor.cycle_palette(1);                           // health
    CHECK(editor.insert(5, 18).ok);
    CHECK(editor.select(2, 2));                        // player start
    CHECK(editor.rotate_selected(5).ok);
    CHECK(editor.move_selected(3, 1).ok);
    CHECK(editor.select(10, 4));                       // a door
    CHECK(editor.remove_selected().ok);
    CHECK(editor.dirty());

    CHECK(editor.save().ok);
    CHECK(!editor.dirty());
    const auto reloaded = ge::load_level_file(dir / "bunker.txt", error);
    CHECK(reloaded.has_value());
    if (reloaded) CHECK(same_level(editor.level(), *reloaded));
    CHECK(read_file(dir / "bunker.txt").rfind("; Showcase level", 0) == 0);  // the description survived
}

TEST(editor_reload_failure_keeps_the_edited_level_and_success_discards_edits) {
    const fs::path dir = temp_dir("reload");
    ge::LevelEditor editor;
    editor.load(ge::test_level(), dir / "hall.txt");
    CHECK(editor.save().ok);

    CHECK(editor.select(7, 6));
    CHECK(editor.remove_selected().ok);
    const ge::Level edited = editor.level();

    write_file(dir / "hall.txt", "name: broken\nmap:\nxxx\n");  // a file that will not parse
    const ge::Status failed = editor.reload();
    CHECK(!failed.ok);
    CHECK(failed.message.find("hall.txt") != std::string::npos);  // says which file and why
    CHECK(same_level(edited, editor.level()));                   // not silently replaced
    CHECK(editor.dirty());

    CHECK(editor.save().ok);                                      // writes the edited level back
    CHECK(editor.select(9, 9));
    editor.cycle_palette(12);
    CHECK(editor.insert(9, 9).ok);
    CHECK(editor.reload().ok);                                    // back to what was saved
    CHECK(same_level(edited, editor.level()));
    CHECK(!editor.dirty());
    CHECK(!editor.has_selection());

    ge::LevelEditor no_file;
    no_file.load(ge::test_level());
    CHECK(!no_file.save().ok);
    CHECK(!no_file.reload().ok);
}

TEST(a_save_that_cannot_write_reports_it_and_keeps_both_the_file_and_the_edits) {
    const fs::path dir = temp_dir("blocked");
    ge::LevelEditor editor;
    editor.load(ge::test_level(), dir / "hall.txt");
    CHECK(editor.save().ok);
    const std::string saved = read_file(dir / "hall.txt");

    CHECK(editor.select(7, 6));
    CHECK(editor.remove_selected().ok);
    fs::create_directory(dir / "hall.txt.tmp");        // the temporary file cannot be created (works even as root)
    const ge::Status blocked = editor.save();
    CHECK(!blocked.ok);
    CHECK(!blocked.message.empty());
    CHECK(editor.dirty());                             // the edits are still unsaved, not lost
    CHECK(!editor.level().map.is_solid(7, 6));
    CHECK(read_file(dir / "hall.txt") == saved);       // the old file is intact

    fs::remove(dir / "hall.txt.tmp");
    CHECK(editor.save().ok);                           // and works once the obstacle is gone
    CHECK(!editor.dirty());
}

// ---- crosshair picking ---------------------------------------------------------------------------------

TEST(crosshair_target_finds_the_wall_and_the_free_cell_in_front_of_it) {
    const ge::Level& hall = ge::test_level();
    const ge::Camera camera = look_east_from(2.5, 2.5);       // row 2 is open up to the east border at x = 15
    const ge::CrosshairTarget t = ge::crosshair_target(hall, camera, 3.0);
    CHECK(t.hit && !t.is_object);
    CHECK(t.cell_x == 15 && t.cell_y == 2);
    CHECK(t.free_x == 5 && t.free_y == 2);                    // 3 tiles ahead of 2.5
    const ge::CrosshairTarget far = ge::crosshair_target(hall, camera, 50.0);
    CHECK(far.free_x == 14 && far.free_y == 2);               // never inside the wall
    const ge::CrosshairTarget close = ge::crosshair_target(hall, look_east_from(13.5, 2.5), 3.0);
    CHECK(close.free_x == 14 && close.free_y == 2);           // the wall is nearer than the reach
}

TEST(crosshair_target_picks_objects_only_when_aimed_at_and_in_view) {
    ge::Level level = ge::test_level();
    level.enemy_spawns.push_back({6.5, 2.5});                 // dead ahead
    ge::CrosshairTarget t = ge::crosshair_target(level, look_east_from(2.5, 2.5), 3.0);
    CHECK(t.hit && t.is_object && t.cell_x == 6 && t.cell_y == 2);
    CHECK_NEAR(t.distance, 4.0, 1e-9);

    level.enemy_spawns[0] = {6.5, 3.0};                       // 0.5 tiles off the ray: not picked
    t = ge::crosshair_target(level, look_east_from(2.5, 2.5), 3.0);
    CHECK(!t.is_object);
    level.enemy_spawns[0] = {6.5, 2.7};                       // 0.2 off: picked
    t = ge::crosshair_target(level, look_east_from(2.5, 2.5), 3.0);
    CHECK(t.is_object);

    level.enemy_spawns[0] = {6.5, 2.5};
    level.map.set_cell(4, 2, 3);                              // a wall between the camera and the enemy hides it
    t = ge::crosshair_target(level, look_east_from(2.5, 2.5), 3.0);
    CHECK(!t.is_object && t.cell_x == 4 && t.cell_y == 2);

    level.map.set_cell(4, 2, 0);
    level.target_spawns.push_back({1.5, 2.5});                // behind the camera: never picked
    t = ge::crosshair_target(level, look_east_from(2.5, 2.5), 3.0);
    CHECK(t.cell_x == 6);                                     // still the enemy ahead
}

// ---- selection highlight in the renderer -----------------------------------------------------------------

TEST(renderer_highlights_only_the_selected_wall_and_clearing_restores_the_frame) {
    const ge::Level& hall = ge::test_level();
    const ge::Camera camera = look_east_from(2.5, 2.5);
    const ge::TextureSet textures = ge::TextureSet::make_procedural();
    for (const bool textured : {false, true}) {
        ge::Renderer renderer;
        renderer.set_textures(textured ? &textures : nullptr);
        ge::Framebuffer plain(96, 60), lit(96, 60), cleared(96, 60), elsewhere(96, 60);
        renderer.render(hall.map, camera, plain);
        renderer.set_highlight_cell(15, 2);                    // the wall straight ahead
        renderer.render(hall.map, camera, lit);
        renderer.clear_highlight();
        renderer.render(hall.map, camera, cleared);
        renderer.set_highlight_cell(1, 1);                     // a cell this view never shows
        renderer.render(hall.map, camera, elsewhere);
        CHECK(plain.checksum() != lit.checksum());
        CHECK(plain.checksum() == cleared.checksum());
        CHECK(plain.checksum() == elsewhere.checksum());
        // The tint is yellow: in the middle of the wall the pixel changes and gets brighter in red and green.
        const std::uint32_t a = plain.pixel(48, 30), b = lit.pixel(48, 30);
        CHECK(a != b);
        CHECK(ge::red_of(b) + ge::green_of(b) > ge::red_of(a) + ge::green_of(a));
    }
}
