#include "engine/world/LevelLoader.hpp"

#include <charconv>
#include <cmath>
#include <fstream>
#include <sstream>

namespace ge {
namespace {

std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.remove_suffix(1);
    return s;
}

bool parse_number(std::string_view s, double& out) {
    s = trim(s);
    if (s.empty()) return false;
    try {
        std::size_t used = 0;
        out = std::stod(std::string(s), &used);
        return used == s.size() && std::isfinite(out);
    } catch (...) {
        return false;
    }
}

}  // namespace

std::optional<Level> parse_level_text(std::string_view text, std::string& error) {
    std::string name = "Untitled";
    double angle_deg = 0.0;
    int waves = 3;
    std::vector<std::string_view> rows;
    bool in_map = false;

    std::size_t line_no = 0;
    std::size_t pos = 0;
    while (pos <= text.size()) {
        const std::size_t end = text.find('\n', pos);
        std::string_view line = text.substr(pos, end == std::string_view::npos ? std::string_view::npos : end - pos);
        pos = end == std::string_view::npos ? text.size() + 1 : end + 1;
        ++line_no;
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);

        if (in_map) {
            if (trim(line).empty()) continue;  // blank lines after the map are ignored
            rows.push_back(line);
            continue;
        }
        const std::string_view t = trim(line);
        if (t.empty() || t.front() == ';') continue;
        if (t == "map:") { in_map = true; continue; }
        const std::size_t colon = t.find(':');
        if (colon == std::string_view::npos) {
            error = "line " + std::to_string(line_no) + ": expected 'key: value' or 'map:'";
            return std::nullopt;
        }
        const std::string_view key = trim(t.substr(0, colon));
        const std::string_view value = trim(t.substr(colon + 1));
        if (key == "name") {
            name = std::string(value);
        } else if (key == "angle") {
            if (!parse_number(value, angle_deg)) { error = "line " + std::to_string(line_no) + ": angle must be a number"; return std::nullopt; }
        } else if (key == "waves") {
            double w = 0;
            if (!parse_number(value, w) || w < 1 || w > 99 || w != std::floor(w)) {
                error = "line " + std::to_string(line_no) + ": waves must be an integer in [1, 99]";
                return std::nullopt;
            }
            waves = static_cast<int>(w);
        } else {
            error = "line " + std::to_string(line_no) + ": unknown key '" + std::string(key) + "'";
            return std::nullopt;
        }
    }
    if (!in_map || rows.empty()) { error = "no 'map:' section with rows found"; return std::nullopt; }

    if (rows.size() < 3 || rows[0].size() < 3) { error = "map must be at least 3x3"; return std::nullopt; }
    if (rows.size() > static_cast<std::size_t>(kMaxLevelSize) || rows[0].size() > static_cast<std::size_t>(kMaxLevelSize)) {
        error = "map is larger than " + std::to_string(kMaxLevelSize) + "x" + std::to_string(kMaxLevelSize);
        return std::nullopt;
    }

    std::optional<Level> level = parse_level(rows, error);
    if (!level) return std::nullopt;

    // parse_level tolerates a missing spawn; level files must have one.
    bool has_spawn = false;
    for (std::string_view r : rows)
        if (r.find('P') != std::string_view::npos) has_spawn = true;
    if (!has_spawn) { error = "map has no player spawn 'P'"; return std::nullopt; }

    const int w = level->map.width(), h = level->map.height();
    for (int x = 0; x < w; ++x)
        for (int y : {0, h - 1})
            if (!level->map.is_solid(x, y)) { error = "map border must be solid, but (" + std::to_string(x) + ", " + std::to_string(y) + ") is open"; return std::nullopt; }
    for (int y = 0; y < h; ++y)
        for (int x : {0, w - 1})
            if (!level->map.is_solid(x, y)) { error = "map border must be solid, but (" + std::to_string(x) + ", " + std::to_string(y) + ") is open"; return std::nullopt; }

    level->name = name;
    level->spawn_angle = angle_deg * kPi / 180.0;
    level->waves_to_win = waves;
    return level;
}

std::optional<Level> load_level_file(const std::filesystem::path& path, std::string& error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) { error = "cannot open level file '" + path.string() + "'"; return std::nullopt; }
    std::ostringstream buf;
    buf << in.rdbuf();
    std::optional<Level> level = parse_level_text(buf.str(), error);
    if (!level) error = path.string() + ": " + error;
    return level;
}

}  // namespace ge
