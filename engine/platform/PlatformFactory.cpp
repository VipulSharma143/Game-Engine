#include "engine/platform/PlatformFactory.hpp"

#include <cstdlib>

#if defined(GE_HAVE_SDL)
#include "engine/platform/SDLPlatform.hpp"
#endif

namespace ge {

bool window_platform_compiled() noexcept {
#if defined(GE_HAVE_SDL)
    return true;
#else
    return false;
#endif
}

bool display_available() noexcept {
#if defined(_WIN32) || defined(__APPLE__)
    return true;
#else
    auto set = [](const char* name) { const char* v = std::getenv(name); return v != nullptr && *v != '\0'; };
    return set("DISPLAY") || set("WAYLAND_DISPLAY") || set("SDL_VIDEODRIVER");
#endif
}

std::unique_ptr<IPlatform> make_window_platform() {
#if defined(GE_HAVE_SDL)
    return std::make_unique<SDLPlatform>();
#else
    return nullptr;
#endif
}

}  // namespace ge
