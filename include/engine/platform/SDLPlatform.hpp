#pragma once
// SDL3 window, keyboard/mouse input and presentation. Only compiled when GE_HAVE_SDL is set.

#include <cstdint>
#include <memory>

#include "engine/platform/Platform.hpp"

namespace ge {

class SDLPlatform final : public IPlatform {
public:
    SDLPlatform();
    ~SDLPlatform() override;

    Status init(const EngineConfig& config) override;
    void shutdown() noexcept override;
    void poll_input(InputState& input) override;
    void present(const Framebuffer& frame, std::uint64_t frame_index) override;
    void finalize(const Framebuffer& final_frame) override;
    void set_mouse_capture(bool wanted) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace ge
