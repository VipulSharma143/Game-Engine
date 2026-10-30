// Runs the real SDL3 platform on SDL's "dummy" video driver, so no display is needed.
#include <SDL3/SDL.h>

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

namespace {
void push_key(SDL_Scancode sc) {
    SDL_Event e{};
    e.type = SDL_EVENT_KEY_DOWN;
    e.key.scancode = sc;
    e.key.repeat = false;
    SDL_PushEvent(&e);
}
void push_click(Uint8 button) {
    SDL_Event e{};
    e.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    e.button.button = button;
    SDL_PushEvent(&e);
}
void push_wheel(float y) {
    SDL_Event e{};
    e.type = SDL_EVENT_MOUSE_WHEEL;
    e.wheel.y = y;
    e.wheel.direction = SDL_MOUSEWHEEL_NORMAL;
    SDL_PushEvent(&e);
}
}  // namespace

TEST(sdl_platform_maps_number_keys_to_weapon_slots_and_the_latest_press_wins) {
    use_dummy_driver();
    auto platform = ge::make_window_platform();
    ge::EngineConfig cfg;
    cfg.capture_mouse = false;
    CHECK(platform->init(cfg).ok);
    push_key(SDL_SCANCODE_2);
    ge::InputState in;
    platform->poll_input(in);
    CHECK_EQ(in.weapon_slot, 2);
    push_key(SDL_SCANCODE_1);
    push_key(SDL_SCANCODE_4);
    push_key(SDL_SCANCODE_9);
    ge::InputState several;
    platform->poll_input(several);
    CHECK_EQ(several.weapon_slot, 9);     // the last key of the frame
    ge::InputState none;
    platform->poll_input(none);
    CHECK_EQ(none.weapon_slot, 0);
    platform->shutdown();
}

TEST(sdl_platform_maps_editing_keys_clicks_and_wheel) {
    use_dummy_driver();
    auto platform = ge::make_window_platform();
    ge::EngineConfig cfg;
    cfg.capture_mouse = false;
    CHECK(platform->init(cfg).ok);

    for (SDL_Scancode sc : {SDL_SCANCODE_F2, SDL_SCANCODE_X, SDL_SCANCODE_M, SDL_SCANCODE_C, SDL_SCANCODE_BACKSPACE,
                            SDL_SCANCODE_F5, SDL_SCANCODE_F9, SDL_SCANCODE_RIGHTBRACKET, SDL_SCANCODE_RIGHTBRACKET,
                            SDL_SCANCODE_LEFTBRACKET, SDL_SCANCODE_PERIOD, SDL_SCANCODE_PERIOD, SDL_SCANCODE_COMMA})
        push_key(sc);
    push_click(SDL_BUTTON_LEFT);
    push_click(SDL_BUTTON_RIGHT);
    ge::InputState in;
    platform->poll_input(in);
    CHECK(in.edit_toggle && in.edit_remove && in.edit_move && in.edit_change && in.edit_deselect);
    CHECK(in.edit_save && in.edit_load && in.edit_select && in.edit_insert);
    CHECK_EQ(in.edit_palette, 1);   // two ] and one [
    CHECK_EQ(in.edit_rotate, 1);    // two . and one ,
    CHECK(!in.use && !in.pause);    // editing keys do not trigger gameplay actions

    push_key(SDL_SCANCODE_DELETE);  // Delete is a second key for remove
    ge::InputState second;
    platform->poll_input(second);
    CHECK(second.edit_remove && !second.edit_toggle && !second.edit_select);

    push_wheel(1.0f); push_wheel(1.0f); push_wheel(-1.0f);
    push_wheel(0.4f); push_wheel(0.4f);   // touchpad fractions: not yet a notch
    ge::InputState wheel;
    platform->poll_input(wheel);
    CHECK_EQ(wheel.edit_palette, 1);
    CHECK_EQ(wheel.weapon_cycle, 1);      // the same wheel cycles weapons in the game
    push_wheel(0.4f);                     // the third 0.4 completes a notch (3 x 0.4 = 1.2)
    ge::InputState later;
    platform->poll_input(later);
    CHECK_EQ(later.edit_palette, 1);
    platform->shutdown();
}
