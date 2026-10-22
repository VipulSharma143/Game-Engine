#pragma once
// Loads and caches files from the assets directory.

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "engine/world/World.hpp"

namespace ge {

class ResourceManager {
public:
    explicit ResourceManager(std::filesystem::path assets_root) : root_(std::move(assets_root)) {}

    const std::filesystem::path& root() const noexcept { return root_; }

    // Text file relative to the assets root. Cached: a second read does not touch the disk.
    // Returns nullptr and sets `error` when the file cannot be read.
    const std::string* read_text(const std::string& relative, std::string& error);

    // `name_or_path` is an existing file path, or a level name that resolves to
    // <assets>/levels/<name>.txt.
    std::optional<Level> load_level(const std::string& name_or_path, std::string& error);

    // Names of all <assets>/levels/*.txt files, sorted.
    std::vector<std::string> list_levels() const;

    std::size_t cached_files() const noexcept { return text_cache_.size(); }
    std::size_t disk_reads() const noexcept { return disk_reads_; }

private:
    std::filesystem::path root_;
    std::map<std::string, std::string> text_cache_;
    std::size_t disk_reads_ = 0;
};

}  // namespace ge
