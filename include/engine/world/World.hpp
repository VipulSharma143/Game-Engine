#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "engine/core/Math.hpp"

namespace ge {

// Rectangular grid of wall ids. 0 is empty space; anything else is solid.
// Coordinates outside the grid read as solid wall (id 1).
class GridMap {
public:
    GridMap() = default;
    GridMap(int width, int height, std::vector<std::uint8_t> cells);

    int width() const noexcept { return width_; }
    int height() const noexcept { return height_; }
    std::uint8_t cell(int x, int y) const noexcept;
    // Changes one cell (e.g. a door opening). Out-of-range coordinates are ignored.
    void set_cell(int x, int y, std::uint8_t id) noexcept;
    bool is_solid(int x, int y) const noexcept { return cell(x, y) != 0; }

private:
    int width_ = 0;
    int height_ = 0;
    std::vector<std::uint8_t> cells_;
};

// Wall ids 10 and 11 are doors: an unlocked door and a door that needs a key.
constexpr std::uint8_t kDoorId = 10;
constexpr std::uint8_t kLockedDoorId = 11;

// Wall ids 12-17 are extra textured walls, written 'a'-'f' in level files (ids 1-9 are digits).
constexpr std::uint8_t kFirstExtraWallId = 12;
constexpr std::uint8_t kLastWallId = 17;

// Level-file character for a wall id, or 0 if the id has none (0 is empty floor, anything above kLastWallId is unknown).
constexpr char wall_char(std::uint8_t id) noexcept {
    if (id >= 1 && id <= 9) return static_cast<char>('0' + id);
    if (id == kDoorId) return 'D';
    if (id == kLockedDoorId) return 'L';
    if (id >= kFirstExtraWallId && id <= kLastWallId) return static_cast<char>('a' + (id - kFirstExtraWallId));
    return 0;
}

enum class PickupKind { Health, Ammo, Key, Shotgun, Chaingun, Launcher };
struct PickupSpawn {
    PickupKind kind = PickupKind::Health;
    Vec2 position{};
};

// What an enemy spawn cell holds: 'E' grunt, 'G' gunner, 'B' brute, 'F' skitter.
enum class EnemyKind : std::uint8_t { Grunt, Gunner, Brute, Skitter };
constexpr int kEnemyKindCount = 4;
struct EnemySpawn {
    Vec2 position{};
    EnemyKind kind = EnemyKind::Grunt;
};

// Texture themes a level can ask for with "theme: NAME": they change the floor, ceiling and fog tint.
constexpr std::string_view kThemeNames[] = {"default", "foundry", "cavern", "lab"};
constexpr bool is_known_theme(std::string_view name) noexcept {
    for (std::string_view t : kThemeNames) if (t == name) return true;
    return false;
}

struct Level {
    GridMap map;
    Vec2 spawn{1.5, 1.5};
    double spawn_angle = 0.0;
    std::string name = "Untitled";
    std::vector<Vec2> target_spawns;  // 'T' cells; waves place targets here first
    std::vector<EnemySpawn> enemy_spawns; // 'E' 'G' 'B' 'F' cells; wave N spawns the first N of them
    std::vector<PickupSpawn> pickups; // 'H' health, 'A' ammo, 'K' key, 'S' shotgun, 'C' chaingun, 'R' rocket launcher
    int waves_to_win = 3;
    std::string theme = "default";    // see kThemeNames
    std::vector<std::string> comments; // the ';' lines before 'map:' (without the ';'), kept so saving does not lose them
};

// Rows use '#' or '1'-'9' or 'a'-'f' for walls ('#' = id 1, 'a'-'f' = ids 12-17), '.' or ' ' for empty space and
// 'P' for the single player spawn, 'T' for a preferred target spawn, 'E'/'G'/'B'/'F' for a grunt, gunner, brute
// or skitter spawn, 'H'/'A'/'K' for health, ammo and key pickups and 'S'/'C'/'R' for the shotgun, chaingun and
// rocket launcher (all empty cells), 'D' for a door and 'L' for a door that needs a key. All rows must have equal length.
std::optional<Level> parse_level(std::span<const std::string_view> rows, std::string& error);

// Deterministic built-in map used by the demo and the tests.
const Level& test_level();

}  // namespace ge
