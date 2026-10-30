#include "engine/world/LevelEditor.hpp"

#include <algorithm>
#include <cmath>

#include "engine/world/LevelLoader.hpp"

namespace ge {
namespace {

// Walls 1-9, then the other objects, then the newer enemies, weapons and walls (so older indices stay put).
// The player start is not here: there is exactly one, so it is only moved.
constexpr EditObject kPalette[] = {
    {ObjectKind::Wall, 1}, {ObjectKind::Wall, 2}, {ObjectKind::Wall, 3}, {ObjectKind::Wall, 4}, {ObjectKind::Wall, 5},
    {ObjectKind::Wall, 6}, {ObjectKind::Wall, 7}, {ObjectKind::Wall, 8}, {ObjectKind::Wall, 9},
    {ObjectKind::Door, 0}, {ObjectKind::LockedDoor, 0}, {ObjectKind::Target, 0}, {ObjectKind::Enemy, 0},
    {ObjectKind::Health, 0}, {ObjectKind::Ammo, 0}, {ObjectKind::Key, 0},
    {ObjectKind::Gunner, 0}, {ObjectKind::Brute, 0}, {ObjectKind::Skitter, 0},
    {ObjectKind::Shotgun, 0}, {ObjectKind::Chaingun, 0}, {ObjectKind::Launcher, 0},
    {ObjectKind::Wall, 12}, {ObjectKind::Wall, 13}, {ObjectKind::Wall, 14}, {ObjectKind::Wall, 15},
    {ObjectKind::Wall, 16}, {ObjectKind::Wall, 17},
};
constexpr int kPaletteSize = static_cast<int>(sizeof kPalette / sizeof kPalette[0]);

Vec2 center_of(int x, int y) noexcept { return {x + 0.5, y + 0.5}; }
bool at_cell(Vec2 p, int x, int y) noexcept { return static_cast<int>(std::floor(p.x)) == x && static_cast<int>(std::floor(p.y)) == y; }
bool row_major_less(Vec2 a, Vec2 b) noexcept { return a.y != b.y ? a.y < b.y : a.x < b.x; }

bool is_pickup_kind(ObjectKind k) noexcept {
    return k == ObjectKind::Health || k == ObjectKind::Ammo || k == ObjectKind::Key || k == ObjectKind::Shotgun ||
           k == ObjectKind::Chaingun || k == ObjectKind::Launcher;
}
PickupKind pickup_kind(ObjectKind k) noexcept {
    switch (k) {
        case ObjectKind::Health: return PickupKind::Health;
        case ObjectKind::Ammo: return PickupKind::Ammo;
        case ObjectKind::Shotgun: return PickupKind::Shotgun;
        case ObjectKind::Chaingun: return PickupKind::Chaingun;
        case ObjectKind::Launcher: return PickupKind::Launcher;
        default: return PickupKind::Key;
    }
}
ObjectKind object_kind(PickupKind k) noexcept {
    switch (k) {
        case PickupKind::Health: return ObjectKind::Health;
        case PickupKind::Ammo: return ObjectKind::Ammo;
        case PickupKind::Key: return ObjectKind::Key;
        case PickupKind::Shotgun: return ObjectKind::Shotgun;
        case PickupKind::Chaingun: return ObjectKind::Chaingun;
        case PickupKind::Launcher: return ObjectKind::Launcher;
    }
    return ObjectKind::Key;
}
bool is_enemy_kind(ObjectKind k) noexcept { return k == ObjectKind::Enemy || k == ObjectKind::Gunner || k == ObjectKind::Brute || k == ObjectKind::Skitter; }
EnemyKind enemy_kind(ObjectKind k) noexcept {
    return k == ObjectKind::Gunner ? EnemyKind::Gunner : k == ObjectKind::Brute ? EnemyKind::Brute : k == ObjectKind::Skitter ? EnemyKind::Skitter : EnemyKind::Grunt;
}
ObjectKind object_kind(EnemyKind k) noexcept {
    return k == EnemyKind::Gunner ? ObjectKind::Gunner : k == EnemyKind::Brute ? ObjectKind::Brute : k == EnemyKind::Skitter ? ObjectKind::Skitter : ObjectKind::Enemy;
}

}  // namespace

std::string object_name(EditObject o) {
    switch (o.kind) {
        case ObjectKind::None: return "NOTHING";
        case ObjectKind::Wall: return "WALL " + std::to_string(o.wall_id);
        case ObjectKind::Door: return "DOOR";
        case ObjectKind::LockedDoor: return "LOCKED DOOR";
        case ObjectKind::Target: return "TARGET";
        case ObjectKind::Enemy: return "GRUNT";
        case ObjectKind::Gunner: return "GUNNER";
        case ObjectKind::Brute: return "BRUTE";
        case ObjectKind::Skitter: return "SKITTER";
        case ObjectKind::Shotgun: return "SHOTGUN";
        case ObjectKind::Chaingun: return "CHAINGUN";
        case ObjectKind::Launcher: return "ROCKET LAUNCHER";
        case ObjectKind::Health: return "HEALTH PACK";
        case ObjectKind::Ammo: return "AMMO";
        case ObjectKind::Key: return "KEY";
        case ObjectKind::PlayerStart: return "PLAYER START";
    }
    return "?";
}

void LevelEditor::load(Level level, std::filesystem::path path) {
    level_ = std::move(level);
    path_ = std::move(path);
    dirty_ = false;
    has_selection_ = false;
    ++revision_;
}

bool LevelEditor::in_map(int x, int y) const noexcept {
    return x >= 0 && y >= 0 && x < level_.map.width() && y < level_.map.height();
}
bool LevelEditor::on_border(int x, int y) const noexcept {
    return x == 0 || y == 0 || x == level_.map.width() - 1 || y == level_.map.height() - 1;
}

EditObject LevelEditor::object_at(int x, int y) const noexcept {
    if (!in_map(x, y)) return {};
    const std::uint8_t id = level_.map.cell(x, y);
    if (id == kDoorId) return {ObjectKind::Door, 0};
    if (id == kLockedDoorId) return {ObjectKind::LockedDoor, 0};
    if (id != 0) return {ObjectKind::Wall, id};
    if (at_cell(level_.spawn, x, y)) return {ObjectKind::PlayerStart, 0};
    for (const Vec2& p : level_.target_spawns) if (at_cell(p, x, y)) return {ObjectKind::Target, 0};
    for (const EnemySpawn& p : level_.enemy_spawns) if (at_cell(p.position, x, y)) return {object_kind(p.kind), 0};
    for (const PickupSpawn& p : level_.pickups) if (at_cell(p.position, x, y)) return {object_kind(p.kind), 0};
    return {};
}

bool LevelEditor::select(int x, int y) noexcept {
    if (!in_map(x, y)) return false;
    has_selection_ = true;
    sel_x_ = x;
    sel_y_ = y;
    return true;
}

int LevelEditor::palette_size() noexcept { return kPaletteSize; }
EditObject LevelEditor::palette_item(int index) noexcept {
    return kPalette[((index % kPaletteSize) + kPaletteSize) % kPaletteSize];
}
void LevelEditor::cycle_palette(int steps) noexcept {
    palette_index_ = ((palette_index_ + steps) % kPaletteSize + kPaletteSize) % kPaletteSize;
}

// Can `object` be put into (x, y)? The cell must be inside the map and (apart from the object being replaced or
// moved, which the caller has already taken out) empty; the border stays solid; the player is never walled in.
Status LevelEditor::check_target(EditObject object, int x, int y) const {
    if (!in_map(x, y)) return Status::failure("OUTSIDE THE MAP");
    if (object_at(x, y).kind != ObjectKind::None) return Status::failure("CELL IS OCCUPIED");
    if (on_border(x, y)) return Status::failure("THE BORDER MUST STAY WALL");
    if (is_solid_object(object) && x == player_x_ && y == player_y_) return Status::failure("YOU ARE STANDING THERE");
    return Status::success();
}

void LevelEditor::put(EditObject o, int x, int y) {
    switch (o.kind) {
        case ObjectKind::Wall: level_.map.set_cell(x, y, o.wall_id); break;
        case ObjectKind::Door: level_.map.set_cell(x, y, kDoorId); break;
        case ObjectKind::LockedDoor: level_.map.set_cell(x, y, kLockedDoorId); break;
        case ObjectKind::Target: level_.target_spawns.push_back(center_of(x, y)); break;
        case ObjectKind::PlayerStart: level_.spawn = center_of(x, y); break;
        case ObjectKind::None: break;
        default:
            if (is_enemy_kind(o.kind)) level_.enemy_spawns.push_back({center_of(x, y), enemy_kind(o.kind)});
            else if (is_pickup_kind(o.kind)) level_.pickups.push_back({pickup_kind(o.kind), center_of(x, y)});
            break;
    }
}

void LevelEditor::take(int x, int y) {
    auto here = [&](Vec2 p) { return at_cell(p, x, y); };
    if (level_.map.cell(x, y) != 0) { level_.map.set_cell(x, y, 0); return; }
    std::erase_if(level_.target_spawns, here);
    std::erase_if(level_.enemy_spawns, [&](const EnemySpawn& p) { return here(p.position); });
    std::erase_if(level_.pickups, [&](const PickupSpawn& p) { return here(p.position); });
}

// Spawn lists are kept in row-major order, the order a saved file is read back in, so the level in memory is
// identical to the one you get after save() and load.
void LevelEditor::touch() noexcept {
    std::sort(level_.target_spawns.begin(), level_.target_spawns.end(), row_major_less);
    std::sort(level_.enemy_spawns.begin(), level_.enemy_spawns.end(),
              [](const EnemySpawn& a, const EnemySpawn& b) { return row_major_less(a.position, b.position); });
    std::sort(level_.pickups.begin(), level_.pickups.end(),
              [](const PickupSpawn& a, const PickupSpawn& b) { return row_major_less(a.position, b.position); });
    dirty_ = true;
    ++revision_;
}

Status LevelEditor::insert(int x, int y) {
    const EditObject item = palette_current();
    if (Status s = check_target(item, x, y); !s) return s;
    put(item, x, y);
    touch();
    select(x, y);
    return {true, "PLACED " + object_name(item)};
}

Status LevelEditor::remove_selected() {
    if (!has_selection_) return Status::failure("NOTHING SELECTED");
    const EditObject o = selected();
    if (o.kind == ObjectKind::None) return Status::failure("NOTHING TO REMOVE HERE");
    if (o.kind == ObjectKind::PlayerStart) return Status::failure("THE PLAYER START CANNOT BE REMOVED (MOVE IT)");
    if (on_border(sel_x_, sel_y_)) return Status::failure("THE BORDER MUST STAY WALL");
    take(sel_x_, sel_y_);
    touch();
    return {true, "REMOVED " + object_name(o)};
}

Status LevelEditor::move_selected(int x, int y) {
    if (!has_selection_) return Status::failure("NOTHING SELECTED");
    const EditObject o = selected();
    if (o.kind == ObjectKind::None) return Status::failure("NOTHING TO MOVE HERE");
    if (on_border(sel_x_, sel_y_)) return Status::failure("THE BORDER CANNOT BE MOVED");
    if (x == sel_x_ && y == sel_y_) return Status::failure("ALREADY THERE");
    if (Status s = check_target(o, x, y); !s) return s;
    if (o.kind == ObjectKind::PlayerStart) level_.spawn = center_of(x, y);
    else { take(sel_x_, sel_y_); put(o, x, y); }
    touch();
    select(x, y);
    return {true, "MOVED " + object_name(o)};
}

Status LevelEditor::change_selected() {
    if (!has_selection_) return Status::failure("NOTHING SELECTED");
    const EditObject o = selected(), item = palette_current();
    if (o.kind == ObjectKind::None) return Status::failure("NOTHING TO CHANGE HERE (INSERT INSTEAD)");
    if (o.kind == ObjectKind::PlayerStart) return Status::failure("THE PLAYER START CANNOT BE CHANGED");
    if (o == item) return Status::failure("ALREADY " + object_name(item));
    if (on_border(sel_x_, sel_y_) && item.kind != ObjectKind::Wall) return Status::failure("THE BORDER MUST STAY WALL");
    if (is_solid_object(item) && sel_x_ == player_x_ && sel_y_ == player_y_) return Status::failure("YOU ARE STANDING THERE");
    take(sel_x_, sel_y_);
    put(item, sel_x_, sel_y_);
    touch();
    return {true, object_name(o) + " CHANGED TO " + object_name(item)};
}

Status LevelEditor::rotate_selected(int steps) {
    if (!has_selection_) return Status::failure("NOTHING SELECTED");
    if (selected().kind != ObjectKind::PlayerStart) return Status::failure("ONLY THE PLAYER START CAN BE ROTATED");
    if (steps == 0) return Status::success();
    double a = std::fmod(level_.spawn_angle + steps * kRotateStep, 2.0 * kPi);
    if (a < 0.0) a += 2.0 * kPi;
    level_.spawn_angle = a;
    dirty_ = true;
    ++revision_;
    return {true, "PLAYER START FACES " + std::to_string(static_cast<int>(std::lround(a * 180.0 / kPi)) % 360) + " DEGREES"};
}

Status LevelEditor::save() {
    if (path_.empty()) return Status::failure("THIS LEVEL HAS NO FILE (START WITH --level FILE)");
    std::string error;
    if (!save_level_file(path_, level_, error)) return Status::failure(error);
    dirty_ = false;
    return {true, "SAVED " + path_.filename().string()};
}

Status LevelEditor::reload() {
    if (path_.empty()) return Status::failure("THIS LEVEL HAS NO FILE TO RELOAD");
    std::string error;
    std::optional<Level> fresh = load_level_file(path_, error);
    if (!fresh) return Status::failure(error);
    const std::filesystem::path keep = path_;
    load(std::move(*fresh), keep);
    return {true, "RELOADED " + keep.filename().string()};
}

}  // namespace ge
