#pragma once

#include <cstdint>
#include <memory>

#include "engine/core/Application.hpp"
#include "engine/core/Config.hpp"
#include "engine/platform/Platform.hpp"
#include "engine/renderer/Framebuffer.hpp"

namespace ge {

struct EngineStats {
    std::uint64_t frames = 0;
    std::uint64_t fixed_steps = 0;
    std::uint64_t clamped_frames = 0;
    double simulated_seconds = 0.0;
};

// Owns the platform and the frame buffer and drives the main loop.
// Lifecycle: construct -> initialize() -> run() -> shutdown() (or destruct).
class Engine {
public:
    Engine(EngineConfig config, std::unique_ptr<IPlatform> platform);
    ~Engine();
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    Status initialize();
    // Returns 0 on a clean run, non-zero if the engine or application failed to start.
    int run(Application& app);
    void shutdown() noexcept;

    bool initialized() const noexcept { return initialized_; }
    const EngineConfig& config() const noexcept { return config_; }
    const EngineStats& stats() const noexcept { return stats_; }
    const Framebuffer& framebuffer() const noexcept { return framebuffer_; }

private:
    EngineConfig config_;
    std::unique_ptr<IPlatform> platform_;
    Framebuffer framebuffer_;
    EngineStats stats_;
    bool initialized_ = false;
};

}  // namespace ge
