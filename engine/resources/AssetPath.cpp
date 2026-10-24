#include "engine/resources/AssetPath.hpp"

#include <cstdlib>

namespace ge {

namespace fs = std::filesystem;

bool looks_like_assets_dir(const fs::path& dir) {
    std::error_code ec;
    return fs::is_directory(dir / "levels", ec);
}

std::vector<fs::path> asset_dir_candidates(const fs::path& executable) {
    std::vector<fs::path> out;
    if (const char* env = std::getenv("GE_ASSETS"); env != nullptr && *env != '\0') out.emplace_back(env);
    std::error_code ec;
    out.push_back(fs::current_path(ec) / "assets");
    fs::path dir = executable.has_parent_path() ? executable.parent_path() : fs::path(".");
    dir = fs::absolute(dir, ec);
    out.push_back(dir / "assets");
    out.push_back(dir / ".." / "share" / "game-engine" / "assets");  // installed / unpacked package layout
    out.push_back(dir / ".." / "assets");
    out.push_back(dir / ".." / ".." / "assets");
    out.push_back(dir / ".." / ".." / ".." / "assets");
    return out;
}

std::optional<fs::path> find_assets_dir(const fs::path& executable) {
    for (const fs::path& c : asset_dir_candidates(executable))
        if (looks_like_assets_dir(c)) {
            std::error_code ec;
            const fs::path canon = fs::weakly_canonical(c, ec);
            return ec ? c : canon;
        }
    return std::nullopt;
}

}  // namespace ge
