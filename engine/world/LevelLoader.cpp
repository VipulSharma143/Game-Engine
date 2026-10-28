#include "engine/world/LevelLoader.hpp"

#include <charconv>
#include <cmath>
#include <cstdio>
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
    std::vector<std::string> comments;
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
        if (t.empty()) continue;
        if (t.front() == ';') {
            std::string_view c = t.substr(1);
            if (!c.empty() && c.front() == ' ') c.remove_prefix(1);
            comments.emplace_back(c);
            continue;
        }
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
    level->comments = std::move(comments);
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

std::optional<std::string> serialize_level_text(const Level& level, std::string& error) {
    const int w = level.map.width(), h = level.map.height();
    if (w < 3 || h < 3 || w > kMaxLevelSize || h > kMaxLevelSize) {
        error = "map size " + std::to_string(w) + "x" + std::to_string(h) + " is outside 3x3 to " + std::to_string(kMaxLevelSize) + "x" + std::to_string(kMaxLevelSize);
        return std::nullopt;
    }
    for (int x = 0; x < w; ++x)
        for (int y : {0, h - 1})
            if (!level.map.is_solid(x, y)) { error = "map border must be solid, but (" + std::to_string(x) + ", " + std::to_string(y) + ") is open"; return std::nullopt; }
    for (int y = 0; y < h; ++y)
        for (int x : {0, w - 1})
            if (!level.map.is_solid(x, y)) { error = "map border must be solid, but (" + std::to_string(x) + ", " + std::to_string(y) + ") is open"; return std::nullopt; }
    if (level.name.find_first_of("\r\n") != std::string::npos) { error = "level name must be one line"; return std::nullopt; }
    for (const std::string& c : level.comments)
        if (c.find_first_of("\r\n") != std::string::npos) { error = "a comment must be one line"; return std::nullopt; }

    // One character per cell; objects sit at cell centres, one per open cell.
    std::vector<char> rows(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), '.');
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const std::uint8_t id = level.map.cell(x, y);
            char c = '.';
            if (id >= 1 && id <= 9) c = static_cast<char>('0' + id);
            else if (id == kDoorId) c = 'D';
            else if (id == kLockedDoorId) c = 'L';
            else if (id != 0) { error = "wall id " + std::to_string(id) + " at (" + std::to_string(x) + ", " + std::to_string(y) + ") has no level-file character"; return std::nullopt; }
            rows[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x)] = c;
        }
    }
    auto put = [&](Vec2 p, char c, const char* what) -> bool {
        const double fx = p.x - std::floor(p.x), fy = p.y - std::floor(p.y);
        const int x = static_cast<int>(std::floor(p.x)), y = static_cast<int>(std::floor(p.y));
        const std::string where = std::string(what) + " at (" + std::to_string(p.x) + ", " + std::to_string(p.y) + ")";
        if (std::fabs(fx - 0.5) > 1e-6 || std::fabs(fy - 0.5) > 1e-6) { error = where + " is not at a cell centre"; return false; }
        if (x < 0 || y < 0 || x >= w || y >= h || level.map.is_solid(x, y)) { error = where + " is not in an open cell"; return false; }
        char& slot = rows[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x)];
        if (slot != '.') { error = where + " shares its cell with another object"; return false; }
        slot = c;
        return true;
    };
    if (!put(level.spawn, 'P', "player spawn")) return std::nullopt;
    for (const Vec2& p : level.target_spawns) if (!put(p, 'T', "target")) return std::nullopt;
    for (const Vec2& p : level.enemy_spawns) if (!put(p, 'E', "enemy")) return std::nullopt;
    for (const PickupSpawn& p : level.pickups)
        if (!put(p.position, p.kind == PickupKind::Health ? 'H' : p.kind == PickupKind::Ammo ? 'A' : 'K', "pickup")) return std::nullopt;

    char number[48];
    std::string out;
    for (const std::string& c : level.comments) out += c.empty() ? ";\n" : "; " + c + "\n";
    out += "name: " + level.name + "\n";
    std::snprintf(number, sizeof number, "angle: %.9g\n", level.spawn_angle * 180.0 / kPi);
    out += number;
    out += "waves: " + std::to_string(level.waves_to_win) + "\nmap:\n";
    for (int y = 0; y < h; ++y) {
        out.append(rows.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(w), static_cast<std::size_t>(w));
        out += '\n';
    }
    return out;
}

bool save_level_file(const std::filesystem::path& path, const Level& level, std::string& error) {
    const std::optional<std::string> text = serialize_level_text(level, error);
    if (!text) { error = "cannot save: " + error; return false; }
    // Prove the text reads back before anything is written over the old file.
    std::string reread_error;
    if (!parse_level_text(*text, reread_error)) { error = "cannot save: the level would not load again (" + reread_error + ")"; return false; }

    std::filesystem::path temp = path;
    temp += ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        out.write(text->data(), static_cast<std::streamsize>(text->size()));
        out.flush();
        if (!out) {
            error = "cannot write '" + temp.string() + "' (is the folder writable?)";
            out.close();
            std::error_code ignore;
            std::filesystem::remove(temp, ignore);
            return false;
        }
    }
    std::error_code ec;
    std::filesystem::rename(temp, path, ec);
    if (ec) {
        error = "cannot replace '" + path.string() + "': " + ec.message();
        std::error_code ignore;
        std::filesystem::remove(temp, ignore);
        return false;
    }
    return true;
}

}  // namespace ge
