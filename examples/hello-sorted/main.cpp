// The smallest program built on SORTED: an Application that paints a moving bar, driven by the engine loop.
//   hello-sorted            run 120 frames without a window and print a checksum of the last frame
//   hello-sorted --window   open a window (until you close it)
#include <cstdio>
#include <cstring>
#include <memory>

#include "engine/core/Engine.hpp"
#include "engine/platform/HeadlessPlatform.hpp"
#include "engine/platform/PlatformFactory.hpp"

class Hello final : public ge::Application {
public:
    ge::Status initialize(const ge::EngineConfig&) override { return ge::Status::success(); }
    void fixed_update(const ge::InputState&, double dt) override { time_ += dt; }  // fixed step: game logic goes here
    void render(ge::Framebuffer& target) override {                                // draw into the pixel buffer
        target.clear(0xFF101820u);
        const int x = static_cast<int>(time_ * 60.0) % target.width();
        target.fill_column(x, 0, target.height(), 0xFFFFC107u);
    }
    void shutdown() noexcept override {}
    bool wants_quit() const noexcept override { return false; }

private:
    double time_ = 0.0;
};

int main(int argc, char** argv) {
    const bool window = argc > 1 && std::strcmp(argv[1], "--window") == 0;

    ge::EngineConfig config;
    config.deterministic = !window;          // headless: every frame advances exactly one fixed step
    config.max_frames = window ? 0 : 120;    // 0 = run until the window is closed

    std::unique_ptr<ge::IPlatform> platform;
    if (window) {
        platform = ge::make_window_platform();
        if (!platform) { std::fprintf(stderr, "error: this SORTED build has no window support\n"); return 1; }
    } else {
        platform = std::make_unique<ge::HeadlessPlatform>();
    }

    ge::Engine engine(config, std::move(platform));
    if (const ge::Status status = engine.initialize(); !status) {
        std::fprintf(stderr, "error: %s\n", status.message.c_str());
        return 1;
    }
    Hello app;
    const int rc = engine.run(app);
    std::printf("SORTED hello: %llu frames, %.2f simulated seconds\n",
                static_cast<unsigned long long>(engine.stats().frames), engine.stats().simulated_seconds);
    return rc;
}
