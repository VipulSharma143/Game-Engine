#pragma once

#include <cstdint>
#include <string>

#include "engine/core/Logger.hpp"

namespace ge {

struct HeadlessOptions {
    std::string ppm_path;      // write the final frame here as a binary PPM when non-empty
    bool ascii_summary = false;  // print the final frame as ASCII art to stdout
    int ascii_columns = 80;
};

enum class RunMode { Auto, Window, Headless };

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
