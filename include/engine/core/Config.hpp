#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "engine/core/Logger.hpp"

namespace ge {

struct HeadlessOptions {
    std::string ppm_path;      // write the final frame here as a binary PPM when non-empty
    bool ascii_summary = false;  // print the final frame as ASCII art to stdout
    int ascii_columns = 80;
};

enum class RunMode { Auto, Window, Headless };

// Key names are SDL key names ("W", "Space", "Left", "Escape", "Return", "F3"...). Each action
// takes a comma-separated list, e.g. "W,Up". Only the window platform uses these.
struct KeyBindings {
    std::string forward = "W,Up";
    std::string backward = "S,Down";
    std::string left = "A";
    std::string right = "D";
    std::string turn_left = "Left";
    std::string turn_right = "Right";
    std::string fire = "Space";
    std::string reload = "R";
    std::string use = "E";
    std::string debug = "F3";
    std::string pause = "Escape,P";
    std::string confirm = "Return,Space";
    std::string quit_menu = "Q";
};

// Splits "W, Up" into {"W", "Up"}; empty entries are dropped.
std::vector<std::string> split_key_list(const std::string& list);

// Central engine configuration. Plain data; validate() before use.
struct EngineConfig {
    int width = 320;
    int height = 200;
    double fov_degrees = 66.0;
    double fixed_timestep = 1.0 / 60.0;  // simulation step, seconds
    double max_frame_time = 0.25;        // raw frame deltas are clamped to this
    int max_catchup_steps = 5;           // simulation steps allowed per rendered frame
    std::uint64_t max_frames = 0;        // 0 = run until the application quits
    bool deterministic = false;          // virtual clock: every frame advances exactly fixed_timestep
    LogLevel log_level = LogLevel::Info;
    HeadlessOptions headless;
    RunMode mode = RunMode::Auto;  // Auto: window when SDL3 and a display are available
    int window_scale = 3;          // window size = framebuffer size * scale
    bool capture_mouse = true;     // relative mouse look in window mode
    bool textured = true;          // textured walls/floor/ceiling with fog; false = flat colours
    double mouse_sensitivity = 0.0025;  // radians per pixel
    KeyBindings keys;
    bool audio = true;             // sound effects (window mode plays them; headless only renders offline)
    double volume = 0.7;           // master volume 0..1
    std::string wav_path;          // headless: save the demo's audio to this WAV file
    std::string level;             // level name or file path; empty = built-in level
    std::string assets_dir;        // empty = search (see find_assets_dir)
    std::string settings_path;     // empty = ./settings.ini if present
    bool start_in_menu = false;    // show the title screen first (window mode)
    bool list_levels = false;      // print available levels and exit
};

// Returns an empty string when valid, otherwise a description of the first problem.
std::string validate_config(const EngineConfig& config);

struct ParseResult {
    bool ok = true;
    bool help_requested = false;
    std::string error;
};

// Applies command-line options to `config`. argv[0] is skipped.
ParseResult parse_arguments(int argc, const char* const* argv, EngineConfig& config);

const char* usage_text() noexcept;

}  // namespace ge
