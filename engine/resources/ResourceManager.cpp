
#include "engine/resources/ResourceManager.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>

#include "engine/world/LevelLoader.hpp"

namespace ge {

namespace fs = std::filesystem;

namespace {

bool is_absolute_resource_path(const std::string& path, const fs::path& parsed) {
    if (parsed.is_absolute() ||
        (!path.empty() && (path.front() == '/' || path.front() == '\\'))) {
        return true;
    }

    // Reject Windows drive-rooted paths on every platform.
    return path.size() >= 3 &&
           std::isalpha(static_cast<unsigned char>(path[0])) &&
           path[1] == ':' &&
           (path[2] == '/' || path[2] == '\\');
}

bool contains_parent_traversal(const fs::path& path) {
    for (const auto& part : path) {
        if (part == "..") {
            return true;
        }
    }
    return false;
}

}  // namespace

const std::string* ResourceManager::read_text(
    const std::string& relative,
    std::string& error) {

    const fs::path rel(relative);

    // Validate before consulting the cache so unsafe paths can never bypass checks.
    if (is_absolute_resource_path(relative, rel)) {
        error = "asset path '" + relative + "' must be relative";
        return nullptr;
    }

    if (contains_parent_traversal(rel)) {
        error = "asset path '" + relative + "' must not contain '..'";
        return nullptr;
    }

    if (const auto it = text_cache_.find(relative); it != text_cache_.end()) {
        return &it->second;
    }

    std::ifstream in(root_ / rel, std::ios::binary);
    if (!in) {
        error = "cannot open asset '" + (root_ / rel).string() + "'";
        return nullptr;
    }

    std::ostringstream buffer;
    buffer << in.rdbuf();

    if (in.bad()) {
        error = "failed to read asset '" + (root_ / rel).string() + "'";
        return nullptr;
    }

    ++disk_reads_;
    return &text_cache_.emplace(relative, buffer.str()).first->second;
}

std::optional<Level> ResourceManager::load_level(
    const std::string& name_or_path,
    std::string& error) {

    std::error_code ec;
    if (fs::is_regular_file(name_or_path, ec)) {
        return load_level_file(name_or_path, error);
    }

    ec.clear();
    const fs::path in_assets = root_ / "levels" / (name_or_path + ".txt");

    if (fs::is_regular_file(in_assets, ec)) {
        return load_level_file(in_assets, error);
    }

    error = "level '" + name_or_path +
            "' not found (tried the path itself and " +
            in_assets.string() + ")";

    const std::vector<std::string> available = list_levels();
    if (!available.empty()) {
        error += "; available levels:";
        for (const std::string& name : available) {
            error += " " + name;
        }
    }

    return std::nullopt;
}

std::vector<std::string> ResourceManager::list_levels() const {
    std::vector<std::string> names;
    std::error_code ec;

    const fs::directory_iterator end;
    for (fs::directory_iterator it(root_ / "levels", ec); !ec && it != end;
         it.increment(ec)) {
        if (it->is_regular_file(ec) &&
            it->path().extension() == ".txt") {
            names.push_back(it->path().stem().string());
        }
        if (ec) {
            break;
        }
    }

    std::sort(names.begin(), names.end());
    return names;
}

}  // namespace ge
