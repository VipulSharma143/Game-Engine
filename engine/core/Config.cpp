#include "engine/core/Config.hpp"

#include <cerrno>
#include <cstdlib>
#include <cstring>

namespace ge {
namespace {

bool parse_int(const char* text, long long& out) {
    char* end = nullptr;
    errno = 0;
    out = std::strtoll(text, &end, 10);
    return errno == 0 && end != text && *end == '\0';
}

bool parse_double(const char* text, double& out) {
    char* end = nullptr;
    errno = 0;
    out = std::strtod(text, &end);
    return errno == 0 && end != text && *end == '\0';
}

}  // namespace

std::string validate_config(const EngineConfig& c) {
    if (c.width < 16 || c.width > 8192) return "width must be in [16, 8192]";
    if (c.height < 16 || c.height > 8192) return "height must be in [16, 8192]";
    if (!(c.fov_degrees > 10.0 && c.fov_degrees < 170.0)) return "fov must be in (10, 170) degrees";
    if (!(c.fixed_timestep > 0.0 && c.fixed_timestep <= 1.0)) return "fixed timestep must be in (0, 1] seconds";
    if (!(c.max_frame_time >= c.fixed_timestep)) return "max frame time must be >= fixed timestep";
    if (c.max_catchup_steps < 1 || c.max_catchup_steps > 100) return "max catch-up steps must be in [1, 100]";
    if (c.volume < 0.0 || c.volume > 1.0) return "volume must be in [0, 1]";
    if (c.window_scale < 1 || c.window_scale > 16) return "window scale must be in [1, 16]";
    if (c.headless.ascii_columns < 8 || c.headless.ascii_columns > 400) return "ascii columns must be in [8, 400]";
    return {};
}

ParseResult parse_arguments(int argc, const char* const* argv, EngineConfig& config) {
    ParseResult result;
    auto fail = [&result](std::string message) {
        result.ok = false;
        result.error = std::move(message);
        return result;
    };
    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        auto need_value = [&]() -> const char* { return (i + 1 < argc) ? argv[++i] : nullptr; };
        long long iv = 0;
        double dv = 0.0;
        if (std::strcmp(arg, "--help") == 0 || std::strcmp(arg, "-h") == 0) {
            result.help_requested = true;
        } else if (std::strcmp(arg, "--width") == 0 || std::strcmp(arg, "--height") == 0 ||
                   std::strcmp(arg, "--frames") == 0) {
            const char* v = need_value();
            if (v == nullptr || !parse_int(v, iv) || iv < 0 || iv > 2000000000LL)
                return fail(std::string("invalid value for ") + arg);
            if (arg[2] == 'w') config.width = static_cast<int>(iv);
            else if (arg[2] == 'h') config.height = static_cast<int>(iv);
            else config.max_frames = static_cast<std::uint64_t>(iv);
        } else if (std::strcmp(arg, "--fov") == 0) {
            const char* v = need_value();
            if (v == nullptr || !parse_double(v, dv)) return fail("invalid value for --fov");
            config.fov_degrees = dv;
        } else if (std::strcmp(arg, "--log-level") == 0) {
            const char* v = need_value();
            if (v == nullptr || !Logger::parse_level(v, config.log_level))
                return fail("invalid value for --log-level (trace|debug|info|warn|error|off)");
        } else if (std::strcmp(arg, "--ppm") == 0) {
            const char* v = need_value();
            if (v == nullptr) return fail("--ppm needs a file path");
            config.headless.ppm_path = v;
        } else if (std::strcmp(arg, "--window") == 0) {
            config.mode = RunMode::Window;
        } else if (std::strcmp(arg, "--headless") == 0) {
            config.mode = RunMode::Headless;
        } else if (std::strcmp(arg, "--level") == 0) {
            const char* v = need_value();
            if (v == nullptr || *v == '\0') return fail("--level needs a level name or file path");
            config.level = v;
        } else if (std::strcmp(arg, "--assets") == 0) {
            const char* v = need_value();
            if (v == nullptr || *v == '\0') return fail("--assets needs a directory");
            config.assets_dir = v;
        } else if (std::strcmp(arg, "--settings") == 0) {
            const char* v = need_value();
            if (v == nullptr || *v == '\0') return fail("--settings needs a file path");
            config.settings_path = v;
        } else if (std::strcmp(arg, "--list-levels") == 0) {
            config.list_levels = true;
        } else if (std::strcmp(arg, "--no-menu") == 0) {
            config.start_in_menu = false;  // main() also skips enabling the menu when this flag is present
        } else if (std::strcmp(arg, "--no-audio") == 0) {
            config.audio = false;
        } else if (std::strcmp(arg, "--volume") == 0) {
            const char* v = need_value();
            if (v == nullptr || !parse_double(v, dv) || dv < 0.0 || dv > 1.0) return fail("--volume needs a number in [0, 1]");
            config.volume = dv;
        } else if (std::strcmp(arg, "--wav") == 0) {
            const char* v = need_value();
            if (v == nullptr || *v == '\0') return fail("--wav needs a file path");
            config.wav_path = v;
        } else if (std::strcmp(arg, "--flat") == 0) {
            config.textured = false;
        } else if (std::strcmp(arg, "--no-mouse") == 0) {
            config.capture_mouse = false;
        } else if (std::strcmp(arg, "--scale") == 0) {
            const char* v = need_value();
            if (v == nullptr || !parse_int(v, iv) || iv < 1 || iv > 16) return fail("--scale needs an integer in [1, 16]");
            config.window_scale = static_cast<int>(iv);
        } else if (std::strcmp(arg, "--ascii") == 0) {
            config.headless.ascii_summary = true;
        } else if (std::strcmp(arg, "--realtime") == 0) {
            config.deterministic = false;
        } else if (std::strcmp(arg, "--deterministic") == 0) {
            config.deterministic = true;
        } else {
            return fail(std::string("unknown option: ") + arg);
        }
    }
    return result;
}

const char* usage_text() noexcept {
    return "Usage: ge-demo [options]\n"
           "  --width N / --height N   framebuffer size (default 320x200)\n"
           "  --fov DEG                horizontal field of view (default 66)\n"
           "  --frames N               frames to run (default 300; 0 = until quit, window only)\n"
           "  --window / --headless    force a window (needs SDL3 + display) or no window (default: auto)\n"
           "  --scale N                window size multiplier (default 3)\n"
           "  --level NAME|FILE        play assets/levels/NAME.txt or a level file (default: built-in)\n"
           "  --list-levels            list available levels and exit\n"
           "  --settings FILE          settings file (default: ./settings.ini if present)\n"
           "  --assets DIR             assets directory (default: searched near the executable)\n"
           "  --no-menu                start playing immediately (window mode)\n"
           "  --no-audio               disable sound\n"
           "  --volume V               master volume 0..1 (default 0.7)\n"
           "  --wav FILE               headless: also save the demo's audio as a WAV file\n"
           "  --flat                   flat-shaded walls instead of textures and fog\n"
           "  --no-mouse               do not capture the mouse (keyboard turning only)\n"
           "  --deterministic          virtual clock, fixed step per frame (default)\n"
           "  --realtime               use the monotonic clock instead\n"
           "  --ascii                  print the final frame as ASCII art\n"
           "  --ppm FILE               write the final frame as a PPM image\n"
           "  --log-level LEVEL        trace|debug|info|warn|error|off\n"
           "  -h, --help               show this text\n";
}

}  // namespace ge

namespace ge {

std::vector<std::string> split_key_list(const std::string& list) {
    std::vector<std::string> out;
    std::size_t pos = 0;
    while (pos <= list.size()) {
        std::size_t end = list.find(',', pos);
        if (end == std::string::npos) end = list.size();
        std::size_t b = pos, e = end;
        while (b < e && (list[b] == ' ' || list[b] == '\t')) ++b;
        while (e > b && (list[e - 1] == ' ' || list[e - 1] == '\t')) --e;
        if (e > b) out.push_back(list.substr(b, e - b));
        pos = end + 1;
    }
    return out;
}

}  // namespace ge
