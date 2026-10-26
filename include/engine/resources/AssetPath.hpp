#pragma once
// Finds the assets directory without hard-coded absolute paths.

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ge {

// A directory counts as the assets directory if it contains a "levels" subdirectory.
bool looks_like_assets_dir(const std::filesystem::path& dir);

// Candidate directories in priority order: $GE_ASSETS, ./assets, <exe>/assets, <exe>/../assets,
// <exe>/../share/<exe name>/assets (an installed or unpacked package: funkshot, sorted-viewer), <exe>/../../assets,
// <exe>/../../../assets (covers build/<config>/).
std::vector<std::filesystem::path> asset_dir_candidates(const std::filesystem::path& executable);

std::optional<std::filesystem::path> find_assets_dir(const std::filesystem::path& executable);

}  // namespace ge
