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
