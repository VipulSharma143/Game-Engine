#include "engine/core/Settings.hpp"

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

bool parse_bool(std::string_view v, bool& out) {
    if (v == "true" || v == "on" || v == "yes" || v == "1") { out = true; return true; }
    if (v == "false" || v == "off" || v == "no" || v == "0") { out = false; return true; }
    return false;
}

bool parse_double(std::string_view v, double& out) {
    if (v.empty()) return false;
    try {
        std::size_t used = 0;
        out = std::stod(std::string(v), &used);
        return used == v.size() && std::isfinite(out);
    } catch (...) {
        return false;
    }
}

}  // namespace

Status apply_settings_text(std::string_view text, EngineConfig& config) {
    EngineConfig working = config;  // only committed when the whole file is valid
    bool in_keys = false;
    std::size_t line_no = 0;
    std::size_t pos = 0;
    auto err = [&line_no](const std::string& msg) { return Status::failure("settings line " + std::to_string(line_no) + ": " + msg); };

    while (pos <= text.size()) {
        const std::size_t end = text.find('\n', pos);
        std::string_view line = trim(text.substr(pos, end == std::string_view::npos ? std::string_view::npos : end - pos));
        pos = end == std::string_view::npos ? text.size() + 1 : end + 1;
        ++line_no;
        if (line.empty() || line.front() == ';' || line.front() == '#') continue;
        if (line.front() == '[') {
            if (line == "[keys]") { in_keys = true; continue; }
            if (line == "[game]" || line == "[video]") { in_keys = false; continue; }
            return err("unknown section " + std::string(line));
        }
        const std::size_t eq = line.find('=');
        if (eq == std::string_view::npos) return err("expected 'key = value'");
        const std::string key(trim(line.substr(0, eq)));
        const std::string_view value = trim(line.substr(eq + 1));

        if (in_keys) {
            if (value.empty()) return err("key binding '" + key + "' has no keys");
            std::string* target = nullptr;
            KeyBindings& k = working.keys;
            if (key == "forward") target = &k.forward;
            else if (key == "backward") target = &k.backward;
            else if (key == "left") target = &k.left;
            else if (key == "right") target = &k.right;
            else if (key == "turn_left") target = &k.turn_left;
            else if (key == "turn_right") target = &k.turn_right;
            else if (key == "fire") target = &k.fire;
            else if (key == "reload") target = &k.reload;
            else if (key == "use") target = &k.use;
            else if (key == "debug") target = &k.debug;
            else if (key == "pause") target = &k.pause;
            else if (key == "confirm") target = &k.confirm;
            else if (key == "quit_menu") target = &k.quit_menu;
            else return err("unknown key binding '" + key + "'");
            *target = std::string(value);
            continue;
        }

        double d = 0.0;
        bool b = false;
        if (key == "width" || key == "height" || key == "window_scale") {
            if (!parse_double(value, d) || d != std::floor(d)) return err(key + " must be an integer");
            (key == "width" ? working.width : key == "height" ? working.height : working.window_scale) = static_cast<int>(d);
        } else if (key == "fov") {
            if (!parse_double(value, d)) return err("fov must be a number");
            working.fov_degrees = d;
        } else if (key == "mouse_sensitivity") {
            if (!parse_double(value, d) || d <= 0.0 || d > 0.1) return err("mouse_sensitivity must be a number in (0, 0.1]");
            working.mouse_sensitivity = d;
        } else if (key == "volume") {
            if (!parse_double(value, d) || d < 0.0 || d > 1.0) return err("volume must be a number in [0, 1]");
            working.volume = d;
        } else if (key == "audio") {
            if (!parse_bool(value, b)) return err("audio must be true or false");
            working.audio = b;
        } else if (key == "textured") {
            if (!parse_bool(value, b)) return err("textured must be true or false");
            working.textured = b;
        } else if (key == "capture_mouse") {
            if (!parse_bool(value, b)) return err("capture_mouse must be true or false");
            working.capture_mouse = b;
        } else if (key == "level") {
            working.level = std::string(value);
        } else {
            return err("unknown setting '" + key + "'");
        }
    }

    const std::string problem = validate_config(working);
    if (!problem.empty()) return Status::failure("settings: " + problem);
    config = working;
    return Status::success();
}

Status apply_settings_file(const std::filesystem::path& path, EngineConfig& config) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return Status::failure("cannot open settings file '" + path.string() + "'");
    std::ostringstream buf;
    buf << in.rdbuf();
    Status s = apply_settings_text(buf.str(), config);
    if (!s.ok) s.message = path.string() + ": " + s.message;
    return s;
}

}  // namespace ge
