#pragma once
// Level files are plain text:
//
//   ; comment lines start with a semicolon
//   name: Training Hall
//   angle: 90          (player start direction in degrees, 0 = east, 90 = south)
//   waves: 3           (waves to clear for victory)
//   map:
//   1111111111
//   1P......T1
//   1111111111
//
// Map characters: 1-9 wall ids ('#' = 1), '.' or ' ' empty, 'P' player spawn (exactly one),
// 'T' target spawn, 'E' enemy spawn (wave N spawns the first N), 'H' health pack, 'A' ammo,
// 'K' key, 'D' door, 'L' door that needs a key (any number of each). The map must be at least 3x3, at most 128x128, rectangular,
// and its outer border must be solid.

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "engine/world/World.hpp"

namespace ge {

constexpr int kMaxLevelSize = 128;

std::optional<Level> parse_level_text(std::string_view text, std::string& error);
std::optional<Level> load_level_file(const std::filesystem::path& path, std::string& error);

// The inverse of parse_level_text: parsing the result gives back an identical level. Fails (with a message) for a
// level the format cannot hold: border not solid, no or several player spawns, two objects in one cell, an object
// away from its cell centre, a wall id above 11, a size outside 3..128. Comments, name, angle and waves are kept.
std::optional<std::string> serialize_level_text(const Level& level, std::string& error);
// Serializes, writes to "<path>.tmp" and renames it over `path`, so a failed save never leaves a half-written level.
bool save_level_file(const std::filesystem::path& path, const Level& level, std::string& error);

}  // namespace ge
