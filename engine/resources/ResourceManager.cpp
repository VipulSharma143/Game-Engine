#include "engine/resources/ResourceManager.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

#include "engine/world/LevelLoader.hpp"

namespace ge {

namespace fs = std::filesystem;

const std::string* ResourceManager::read_text(const std::string& relative, std::string& error) {
    if (auto it = text_cache_.find(relative); it != text_cache_.end()) return &it->second;
    // Refuse paths that climb out of the assets directory.
    const fs::path rel(relative);
    for (const auto& part : rel)
        if (part == "..") { error = "asset path '" + relative + "' must not contain '..'"; return nullptr; }
    if (rel.is_absolute()) { error = "asset path '" + relative + "' must be relative"; return nullptr; }

    std::ifstream in(root_ / rel, std::ios::binary);
    if (!in) { error = "cannot open asset '" + (root_ / rel).string() + "'"; return nullptr; }
    std::ostringstream buf;
    buf << in.rdbuf();
    ++disk_reads_;
    return &text_cache_.emplace(relative, buf.str()).first->second;
}

std::optional<Level> ResourceManager::load_level(const std::string& name_or_path, std::string& error) {
    std::error_code ec;
    if (fs::is_regular_file(name_or_path, ec)) return load_level_file(name_or_path, error);
    const fs::path in_assets = root_ / "levels" / (name_or_path + ".txt");
    if (fs::is_regular_file(in_assets, ec)) return load_level_file(in_assets, error);

    error = "level '" + name_or_path + "' not found (tried the path itself and " + in_assets.string() + ")";
    const std::vector<std::string> available = list_levels();
    if (!available.empty()) {
        error += "; available levels:";
        for (const std::string& n : available) error += " " + n;
    }
    return std::nullopt;
}

std::vector<std::string> ResourceManager::list_levels() const {
    std::vector<std::string> names;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(root_ / "levels", ec))
        if (entry.is_regular_file() && entry.path().extension() == ".txt") names.push_back(entry.path().stem().string());
    std::sort(names.begin(), names.end());
    return names;
}

}  // namespace ge
