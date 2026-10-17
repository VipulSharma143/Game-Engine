#pragma once

#include "engine/core/Config.hpp"
#include "engine/core/Status.hpp"
#include "engine/platform/Platform.hpp"
#include "engine/renderer/Framebuffer.hpp"

namespace ge {

// Interface the engine loop drives. The game implements it.
class Application {
public:
    virtual ~Application() = default;

    virtual Status initialize(const EngineConfig& config) = 0;
    virtual void fixed_update(const InputState& input, double dt) = 0;
    virtual void render(Framebuffer& target) = 0;
    // Called exactly once if initialize() succeeded.
    virtual void shutdown() noexcept = 0;
    virtual bool wants_quit() const noexcept = 0;
};

}  // namespace ge
