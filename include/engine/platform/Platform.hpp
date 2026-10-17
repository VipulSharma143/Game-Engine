#pragma once

#include <cstdint>

#include "engine/core/Config.hpp"
#include "engine/core/Status.hpp"
#include "engine/renderer/Framebuffer.hpp"

namespace ge {

// Device-independent input sampled once per frame. Axes are in [-1, 1].
struct InputState {
    double forward = 0.0;  // +1 = forward
    double strafe = 0.0;   // +1 = right
    double turn = 0.0;     // +1 = turn right (clockwise on the map)
    bool quit = false;
};

// Everything operating-system specific (window, input devices, presentation)
// sits behind this interface. The engine never includes OS headers.
class IPlatform {
public:
    virtual ~IPlatform() = default;

    virtual Status init(const EngineConfig& config) = 0;
    virtual void shutdown() noexcept = 0;
    virtual void poll_input(InputState& input) = 0;
    virtual void present(const Framebuffer& frame, std::uint64_t frame_index) = 0;
    // Called once after the last frame; headless output (PPM, ASCII) is written here.
    virtual void finalize(const Framebuffer& final_frame) = 0;
};

}  // namespace ge
