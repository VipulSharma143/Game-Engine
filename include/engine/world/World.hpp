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
    bool is_solid(int x, int y) const noexcept { return cell(x, y) != 0; }

private:
    int width_ = 0;
    int height_ = 0;
    std::vector<std::uint8_t> cells_;
};

struct Level {
    GridMap map;
    Vec2 spawn{1.5, 1.5};
    double spawn_angle = 0.0;
    std::string name = "Untitled";
    std::vector<Vec2> target_spawns;  // 'T' cells; waves place targets here first
    int waves_to_win = 3;
};

// Rows use '#' or '1'-'9' for walls ('#' = id 1), '.' or ' ' for empty space and
// 'P' for the single player spawn and 'T' for a preferred target spawn (both empty cells). All rows must have equal length.
std::optional<Level> parse_level(std::span<const std::string_view> rows, std::string& error);

// Deterministic built-in map used by the demo and the tests.
const Level& test_level();

}  // namespace ge
