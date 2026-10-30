#pragma once
// Edits a Level in memory: select, insert, remove, move, change and rotate objects, then save it with
// save_level_file(). Every operation checks its arguments and returns a Status, so a bad request (an occupied
// cell, the border, a repeated removal) changes nothing. The viewer and the tests share this one code path.
//
// An "object" is whatever one map cell holds: a wall (ids 1-9), a door, a locked door, a target, an enemy spawn,
// a pickup or the player start. Walls and doors have no direction or size in the level format, so only the player
// start can be rotated and nothing can be scaled.

#include <cstdint>
#include <filesystem>
#include <string>

#include "engine/core/Status.hpp"
#include "engine/world/World.hpp"

namespace ge {

// Enemy is the grunt; the other enemy kinds and the weapon pickups come after the original kinds.
enum class ObjectKind : std::uint8_t {
    None, Wall, Door, LockedDoor, Target, Enemy, Health, Ammo, Key, PlayerStart,
    Gunner, Brute, Skitter, Shotgun, Chaingun, Launcher
};

struct EditObject {
    ObjectKind kind = ObjectKind::None;
    std::uint8_t wall_id = 0;  // 1-9 or 12-17, only meaningful for Wall
    friend constexpr bool operator==(EditObject a, EditObject b) noexcept { return a.kind == b.kind && a.wall_id == b.wall_id; }
};

// Uppercase display name, e.g. "WALL 3", "ENEMY".
std::string object_name(EditObject object);
// True for objects the player cannot walk through.
constexpr bool is_solid_object(EditObject o) noexcept {
    return o.kind == ObjectKind::Wall || o.kind == ObjectKind::Door || o.kind == ObjectKind::LockedDoor;
}

class LevelEditor {
public:
    static constexpr double kRotateStep = 15.0 * kPi / 180.0;  // radians per rotate step

    // Replaces the edited level. `path` is where save() writes and reload() reads; it may be empty.
    void load(Level level, std::filesystem::path path = {});
    const Level& level() const noexcept { return level_; }
    const std::filesystem::path& path() const noexcept { return path_; }
    // Changes whenever the level changes (edit, load, reload), so callers can rebuild what they derived from it.
    std::uint64_t revision() const noexcept { return revision_; }
    bool dirty() const noexcept { return dirty_; }

    EditObject object_at(int x, int y) const noexcept;  // kind None for empty or out-of-range cells

    // Selection is a cell. Selecting an empty cell is allowed; selecting outside the map is not.
    bool select(int x, int y) noexcept;
    void deselect() noexcept { has_selection_ = false; }
    bool has_selection() const noexcept { return has_selection_; }
    int selected_x() const noexcept { return sel_x_; }
    int selected_y() const noexcept { return sel_y_; }
    EditObject selected() const noexcept { return has_selection_ ? object_at(sel_x_, sel_y_) : EditObject{}; }

    // The palette is what insert() places and change() turns the selection into.
    static int palette_size() noexcept;
    static EditObject palette_item(int index) noexcept;
    int palette_index() const noexcept { return palette_index_; }
    EditObject palette_current() const noexcept { return palette_item(palette_index_); }
    void cycle_palette(int steps) noexcept;

    // Cell the player stands in: solid objects are refused there so the player is never walled in.
    void set_player_cell(int x, int y) noexcept { player_x_ = x; player_y_ = y; }

    // All of these return a message either way ("PLACED ENEMY" / "CELL IS OCCUPIED") and select the result.
    Status insert(int x, int y);           // palette item into an empty cell
    Status remove_selected();              // not the player start, not the border
    Status move_selected(int x, int y);    // to an empty cell; the player start may move
    Status change_selected();              // selected object becomes the palette item, in place
    Status rotate_selected(int steps);     // player start only, kRotateStep per step

    // Writes the level to path() (atomic replace) and clears dirty(). Fails, changing nothing, if the level
    // is not valid or the file cannot be written.
    Status save();
    // Reads path() again. On failure the edited level stays exactly as it was.
    Status reload();

private:
    bool in_map(int x, int y) const noexcept;
    bool on_border(int x, int y) const noexcept;
    Status check_target(EditObject object, int x, int y) const;
    void put(EditObject object, int x, int y);   // caller has validated
    void take(int x, int y);
    void touch() noexcept;                        // sort spawn lists, bump revision, mark dirty

    Level level_;
    std::filesystem::path path_;
    std::uint64_t revision_ = 0;
    bool dirty_ = false;
    bool has_selection_ = false;
    int sel_x_ = 0, sel_y_ = 0;
    int palette_index_ = 0;
    int player_x_ = -1, player_y_ = -1;
};

}  // namespace ge
