#include "engine/world/World.hpp"

#include <array>

namespace ge {

GridMap::GridMap(int width, int height, std::vector<std::uint8_t> cells)
    : width_(width), height_(height), cells_(std::move(cells)) {}

std::uint8_t GridMap::cell(int x, int y) const noexcept {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) return 1;
    return cells_[static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(x)];
}

void GridMap::set_cell(int x, int y, std::uint8_t id) noexcept {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) return;
    cells_[static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(x)] = id;
}

std::optional<Level> parse_level(std::span<const std::string_view> rows, std::string& error) {
    if (rows.empty() || rows[0].empty()) {
        error = "level has no rows";
        return std::nullopt;
    }
    const std::size_t width = rows[0].size();
    std::vector<std::uint8_t> cells;
    cells.reserve(width * rows.size());
    Level level;
    bool spawn_found = false;
    for (std::size_t y = 0; y < rows.size(); ++y) {
        if (rows[y].size() != width) {
            error = "row " + std::to_string(y) + " has a different length";
            return std::nullopt;
        }
        for (std::size_t x = 0; x < width; ++x) {
            const char c = rows[y][x];
            std::uint8_t id = 0;
            if (c == '#') id = 1;
            else if (c >= '1' && c <= '9') id = static_cast<std::uint8_t>(c - '0');
            else if (c >= 'a' && c <= 'a' + (kLastWallId - kFirstExtraWallId)) id = static_cast<std::uint8_t>(kFirstExtraWallId + (c - 'a'));
            else if (c == 'P') {
                if (spawn_found) {
                    error = "more than one player spawn";
                    return std::nullopt;
                }
                spawn_found = true;
                level.spawn = {static_cast<double>(x) + 0.5, static_cast<double>(y) + 0.5};
            } else if (c == 'T') {
                level.target_spawns.push_back({static_cast<double>(x) + 0.5, static_cast<double>(y) + 0.5});
            } else if (c == 'E' || c == 'G' || c == 'B' || c == 'F') {
                const EnemyKind kind = c == 'E' ? EnemyKind::Grunt : c == 'G' ? EnemyKind::Gunner : c == 'B' ? EnemyKind::Brute : EnemyKind::Skitter;
                level.enemy_spawns.push_back({{static_cast<double>(x) + 0.5, static_cast<double>(y) + 0.5}, kind});
            } else if (c == 'H' || c == 'A' || c == 'K' || c == 'S' || c == 'C' || c == 'R') {
                const PickupKind kind = c == 'H' ? PickupKind::Health : c == 'A' ? PickupKind::Ammo : c == 'K' ? PickupKind::Key
                                        : c == 'S' ? PickupKind::Shotgun : c == 'C' ? PickupKind::Chaingun : PickupKind::Launcher;
                level.pickups.push_back({kind, {static_cast<double>(x) + 0.5, static_cast<double>(y) + 0.5}});
            } else if (c == 'D') {
                id = kDoorId;
            } else if (c == 'L') {
                id = kLockedDoorId;
            } else if (c != '.' && c != ' ') {
                error = std::string("unknown map character '") + c + "'";
                return std::nullopt;
            }
            cells.push_back(id);
        }
    }
    level.map = GridMap(static_cast<int>(width), static_cast<int>(rows.size()), std::move(cells));
    return level;
}

const Level& test_level() {
    static const Level level = [] {
        static constexpr std::array<std::string_view, 16> rows = {
            "################",
            "#..............#",
            "#.P............#",
            "#..22......33..#",
            "#..22......33..#",
            "#..............#",
            "#......11......#",
            "#......11......#",
            "#..............#",
            "#.444.....555..#",
            "#.4.4.....5.5..#",
            "#.444.....555..#",
            "#..............#",
            "#......66......#",
            "#..............#",
            "################",
        };
        std::string error;
        auto parsed = parse_level(rows, error);
        Level built_in = parsed ? *parsed : Level{};
        built_in.name = "Training Hall";  // same layout as assets/levels/hall.txt
        return built_in;
    }();
    return level;
}

}  // namespace ge
