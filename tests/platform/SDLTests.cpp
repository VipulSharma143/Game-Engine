// Runs the real SDL3 platform on SDL's "dummy" video driver, so no display is needed.
#include <cstdlib>

#include "TestFramework.hpp"
#include "engine/core/Engine.hpp"
#include "engine/platform/PlatformFactory.hpp"
#include "game/GameApplication.hpp"

namespace {
void use_dummy_driver() {
#if defined(_WIN32)
    _putenv_s("SDL_VIDEODRIVER", "dummy");
#else
    setenv("SDL_VIDEODRIVER", "dummy", 1);
#endif
}
}  // namespace

TEST(sdl_platform_is_compiled_in) { CHECK(ge::window_platform_compiled()); }

TEST(sdl_platform_initialises_presents_and_polls) {
    use_dummy_driver();
    auto platform = ge::make_window_platform();
    CHECK(platform != nullptr);
    ge::EngineConfig cfg;
    cfg.capture_mouse = false;
    const ge::Status s = platform->init(cfg);
    CHECK(s.ok);
    if (!s.ok) return;
    ge::Framebuffer fb(cfg.width, cfg.height);
    fb.clear(ge::pack_rgb(10, 20, 30));
    for (int i = 0; i < 5; ++i) {
        ge::InputState in;
        platform->poll_input(in);
        CHECK(!in.quit);
        CHECK_NEAR(in.forward, 0.0, 0.0);  // no keys pressed
        platform->present(fb, static_cast<std::uint64_t>(i));
    }
    platform->finalize(fb);
    platform->shutdown();
}

TEST(sdl_platform_runs_the_game_through_the_engine) {
    use_dummy_driver();
    ge::EngineConfig cfg;
    cfg.max_frames = 30;
    cfg.deterministic = true;
    cfg.capture_mouse = false;
    ge::Engine engine(cfg, ge::make_window_platform());
    CHECK(engine.initialize().ok);
    game::GameApplication app(ge::test_level());
    CHECK_EQ(engine.run(app), 0);
    CHECK_EQ(engine.stats().frames, 30u);
    CHECK_EQ(app.target_count(), 8u);
}
