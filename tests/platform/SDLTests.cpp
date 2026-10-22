// Runs the real SDL3 platform on SDL's "dummy" video driver, so no display is needed.
#include <cstdlib>

#include "TestFramework.hpp"
#include <chrono>
#include <thread>

#include "engine/audio/AudioMixer.hpp"
#include "engine/audio/SDLAudioDevice.hpp"
#include "engine/audio/Synth.hpp"
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

TEST(sdl_platform_rejects_unknown_key_names_with_a_clear_message) {
    use_dummy_driver();
    auto platform = ge::make_window_platform();
    ge::EngineConfig cfg;
    cfg.capture_mouse = false;
    cfg.keys.fire = "Space,NotARealKey";
    const ge::Status s = platform->init(cfg);
    CHECK(!s.ok);
    CHECK(s.message.find("NotARealKey") != std::string::npos);
    CHECK(s.message.find("fire") != std::string::npos);
}

TEST(sdl_platform_accepts_custom_bindings_and_mouse_capture_requests) {
    use_dummy_driver();
    auto platform = ge::make_window_platform();
    ge::EngineConfig cfg;
    cfg.capture_mouse = false;
    cfg.keys.fire = "F, Return";
    cfg.keys.forward = "Up";
    CHECK(platform->init(cfg).ok);
    platform->set_mouse_capture(false);  // menus: must not crash even when capture is disabled
    platform->set_mouse_capture(true);
    ge::InputState in;
    platform->poll_input(in);
    CHECK(!in.confirm && !in.pause && !in.menu_quit);
    platform->shutdown();
}

TEST(sdl_audio_device_pulls_and_plays_the_mixer_on_the_dummy_driver) {
#if defined(_WIN32)
    _putenv_s("SDL_AUDIODRIVER", "dummy");
#else
    setenv("SDL_AUDIODRIVER", "dummy", 1);
#endif
    ge::AudioMixer mixer(44100);
    const ge::SoundBank bank = ge::SoundBank::make(44100);
    ge::SDLAudioDevice device;
    const ge::Status s = device.open(mixer);
    CHECK(s.ok);
    if (!s.ok) return;
    CHECK(device.is_open());
    CHECK(!device.open(mixer).ok);  // already open
    mixer.play(bank.win);           // 1.3 s of sound
    // The dummy device consumes audio in real time; wait for the callback to drain a good part of it.
    for (int i = 0; i < 100 && device.frames_delivered() < 4000; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(20));
    CHECK(device.frames_delivered() >= 4000u);
    CHECK(mixer.active_voices() <= 1u);
    device.close();
    CHECK(!device.is_open());
}
