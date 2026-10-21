#include "engine/core/Engine.hpp"

#include "engine/core/GameLoop.hpp"
#include "engine/core/Logger.hpp"
#include "engine/core/Time.hpp"

namespace ge {

Engine::Engine(EngineConfig config, std::unique_ptr<IPlatform> platform)
    : config_(std::move(config)), platform_(std::move(platform)) {}

Engine::~Engine() { shutdown(); }

Status Engine::initialize() {
    if (initialized_) return Status::failure("engine already initialized");
    if (!platform_) return Status::failure("no platform provided");
    const std::string problem = validate_config(config_);
    if (!problem.empty()) return Status::failure("invalid config: " + problem);
    Logger::set_level(config_.log_level);
    Status s = platform_->init(config_);
    if (!s) return Status::failure("platform init failed: " + s.message);
    framebuffer_.resize(config_.width, config_.height);
    stats_ = {};
    initialized_ = true;
    GE_LOG_DEBUG("engine initialized %dx%d", config_.width, config_.height);
    return Status::success();
}

int Engine::run(Application& app) {
    if (!initialized_) {
        GE_LOG_ERROR("run() called before successful initialize()");
        return 1;
    }
    const Status started = app.initialize(config_);
    if (!started) {
        GE_LOG_ERROR("application init failed: %s", started.message.c_str());
        return 2;
    }

    FrameTimer timer(config_.max_frame_time);
    FixedStepper stepper(config_.fixed_timestep, config_.max_catchup_steps);
    double previous = monotonic_seconds();
    InputState input;

    for (;;) {
        double raw = config_.fixed_timestep;
        if (!config_.deterministic) {
            const double now = monotonic_seconds();
            raw = now - previous;
            previous = now;
        }
        const double dt = timer.advance(raw);

        input = InputState{};
        platform_->poll_input(input);
        if (input.quit || app.wants_quit()) break;

        const int steps = stepper.consume(dt);
        for (int i = 0; i < steps; ++i) {
            app.fixed_update(input, stepper.step());
            // Edge-triggered and delta inputs must not repeat on catch-up steps.
            input.look = 0.0;
            input.use = input.reload = input.toggle_debug = false;
        }
        stats_.fixed_steps += static_cast<std::uint64_t>(steps);

        app.render(framebuffer_);
        platform_->present(framebuffer_, stats_.frames);
        ++stats_.frames;
        if (config_.max_frames != 0 && stats_.frames >= config_.max_frames) break;
    }

    stats_.clamped_frames = timer.clamped_count();
    stats_.simulated_seconds = static_cast<double>(stats_.fixed_steps) * config_.fixed_timestep;
    platform_->finalize(framebuffer_);
    app.shutdown();
    return 0;
}

void Engine::shutdown() noexcept {
    if (!initialized_) return;
    platform_->shutdown();
    initialized_ = false;
}

}  // namespace ge
