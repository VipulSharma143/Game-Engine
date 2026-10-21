#pragma once

#include <memory>

#include "engine/platform/Platform.hpp"

namespace ge {

// True when this build contains the SDL3 platform.
bool window_platform_compiled() noexcept;
// True when a display is likely available (DISPLAY / WAYLAND_DISPLAY / SDL_VIDEODRIVER on
// Linux; always true elsewhere).
bool display_available() noexcept;
// Returns nullptr when the SDL3 platform is not compiled in.
std::unique_ptr<IPlatform> make_window_platform();

}  // namespace ge
