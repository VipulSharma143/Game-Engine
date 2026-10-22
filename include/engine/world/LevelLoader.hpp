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
// 'T' target spawn (any number). The map must be at least 3x3, at most 128x128, rectangular,
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

}  // namespace ge
