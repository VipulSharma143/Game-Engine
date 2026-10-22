#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "TestFramework.hpp"
#include "engine/audio/AudioMixer.hpp"
#include "engine/audio/Synth.hpp"
#include "engine/audio/Wav.hpp"
#include "engine/core/Config.hpp"
#include "engine/core/Settings.hpp"
#include "engine/world/LevelLoader.hpp"
#include "game/GameAudio.hpp"

namespace {

ge::Sound constant_sound(const char* name, std::size_t frames, float value, int rate = 44100) {
    ge::Sound s;
    s.name = name;
    s.sample_rate = rate;
    s.samples.assign(frames, value);
    return s;
}

double rms(const std::vector<float>& v) {
    if (v.empty()) return 0.0;
    double sum = 0.0;
    for (float x : v) sum += static_cast<double>(x) * x;
    return std::sqrt(sum / static_cast<double>(v.size()));
}

}  // namespace

// ---- mixer -----------------------------------------------------------------------------------------
TEST(mixer_plays_a_sound_once_then_goes_silent) {
    ge::AudioMixer m;
    const ge::Sound s = constant_sound("c", 100, 0.5f);
    CHECK(m.play(s, 1.0f, 0.0f));
    CHECK_EQ(m.active_voices(), 1u);
    std::vector<float> out(2 * 60);
    m.mix(out.data(), 60);
    CHECK_EQ(m.active_voices(), 1u);
    CHECK(out[0] > 0.3f);
    m.mix(out.data(), 60);                       // 40 frames of sound left, 20 of silence
    CHECK(out[2 * 39] > 0.3f);
    CHECK_NEAR(out[2 * 40], 0.0, 0.0);
    CHECK_EQ(m.active_voices(), 0u);
    m.mix(out.data(), 60);
    CHECK_NEAR(rms(out), 0.0, 0.0);
}

TEST(mixer_chunking_does_not_change_the_output) {
    const ge::SoundBank bank = ge::SoundBank::make();
    ge::AudioMixer a, b;
    a.play(bank.kill, 1.0f, 0.3f);
    b.play(bank.kill, 1.0f, 0.3f);
    std::vector<float> whole(2 * 5000), parts(2 * 5000);
    a.mix(whole.data(), 5000);
    for (std::size_t off = 0; off < 5000; off += 1000) b.mix(parts.data() + 2 * off, 1000);
    CHECK(whole == parts);
}

TEST(mixer_pan_law_is_constant_power) {
    const ge::Sound s = constant_sound("c", 8, 1.0f);
    float out[2];
    auto gains = [&](float pan) {
        ge::AudioMixer m;
        m.play(s, 1.0f, pan);
        m.mix(out, 1);
        return std::pair<float, float>(out[0], out[1]);
    };
    const auto [ll, lr] = gains(-1.0f);
    CHECK_NEAR(ll, 1.0, 1e-5); CHECK_NEAR(lr, 0.0, 1e-5);
    const auto [rl, rr] = gains(1.0f);
    CHECK_NEAR(rl, 0.0, 1e-5); CHECK_NEAR(rr, 1.0, 1e-5);
    const auto [cl, cr] = gains(0.0f);
    CHECK_NEAR(cl, cr, 1e-6);
    CHECK_NEAR(cl * cl + cr * cr, 1.0, 1e-5);   // power is the same at the centre as at the sides
    const auto [ql, qr] = gains(0.5f);
    CHECK(qr > ql);
    CHECK_NEAR(ql * ql + qr * qr, 1.0, 1e-5);
    const auto [xl, xr] = gains(7.0f);          // out-of-range pan is clamped
    CHECK_NEAR(xr, 1.0, 1e-5); CHECK_NEAR(xl, 0.0, 1e-5);
}

TEST(mixer_sums_voices_clips_and_applies_master_volume) {
    const ge::Sound a = constant_sound("a", 10, 0.25f), b = constant_sound("b", 10, 0.25f), loud = constant_sound("loud", 10, 0.9f);
    ge::AudioMixer m;
    m.play(a, 1.0f, -1.0f);
    m.play(b, 1.0f, -1.0f);
    float out[2 * 4];
    m.mix(out, 4);
    CHECK_NEAR(out[0], 0.5, 1e-6);             // two voices add

    ge::AudioMixer clip;
    clip.play(loud, 1.0f, -1.0f);
    clip.play(loud, 1.0f, -1.0f);
    clip.mix(out, 4);
    CHECK_NEAR(out[0], 1.0, 0.0);              // 1.8 is clamped, never wraps

    ge::AudioMixer half;
    half.set_master_volume(0.5f);
    CHECK_NEAR(half.master_volume(), 0.5, 0.0);
    half.play(a, 1.0f, -1.0f);
    half.mix(out, 4);
    CHECK_NEAR(out[0], 0.125, 1e-6);
    half.set_master_volume(9.0f);
    CHECK_NEAR(half.master_volume(), 1.0, 0.0);
}

TEST(mixer_rejects_wrong_rate_and_empty_sounds_and_steals_oldest_voice) {
    ge::AudioMixer m(44100);
    CHECK(!m.play(constant_sound("slow", 10, 0.5f, 22050)));
    CHECK(!m.play(constant_sound("empty", 0, 0.5f)));
    CHECK_EQ(m.total_started(), 0u);

    const ge::Sound s = constant_sound("s", 1000, 0.1f);
    for (std::size_t i = 0; i < ge::AudioMixer::kMaxVoices + 3; ++i) CHECK(m.play(s));
    CHECK_EQ(m.active_voices(), ge::AudioMixer::kMaxVoices);
    CHECK_EQ(m.total_started(), ge::AudioMixer::kMaxVoices + 3);
    CHECK_EQ(m.started_counts().at("s"), ge::AudioMixer::kMaxVoices + 3);
    m.stop_all();
    CHECK_EQ(m.active_voices(), 0u);
}

TEST(mixer_is_safe_to_use_from_two_threads) {
    ge::AudioMixer m;
    const ge::SoundBank bank = ge::SoundBank::make();
    std::thread audio([&m] {
        std::vector<float> out(2 * 256);
        for (int i = 0; i < 2000; ++i) m.mix(out.data(), 256);
    });
    for (int i = 0; i < 2000; ++i) m.play(bank.hit, 0.5f, 0.0f);
    audio.join();
    CHECK_EQ(m.total_started(), 2000u);
}

// ---- synthesised sounds --------------------------------------------------------------------------
TEST(synth_sounds_are_clean_loud_enough_and_deterministic) {
    const ge::SoundBank a = ge::SoundBank::make();
    const ge::SoundBank b = ge::SoundBank::make();
    const std::vector<const ge::Sound*> all = {&a.shot, &a.hit, &a.kill, &a.reload, &a.wave_start, &a.wave_clear, &a.win, &a.ui};
    const std::vector<const ge::Sound*> all_b = {&b.shot, &b.hit, &b.kill, &b.reload, &b.wave_start, &b.wave_clear, &b.win, &b.ui};
    std::set<std::string> names;
    for (std::size_t i = 0; i < all.size(); ++i) {
        const ge::Sound& s = *all[i];
        names.insert(s.name);
        CHECK(!s.samples.empty());
        CHECK_EQ(s.sample_rate, 44100);
        float peak = 0.0f;
        bool finite = true;
        for (float x : s.samples) { peak = std::max(peak, std::fabs(x)); finite = finite && std::isfinite(x); }
        CHECK(finite);
        CHECK(peak > 0.45f && peak <= 0.95f);      // audible, with headroom
        CHECK(rms(s.samples) > 0.01);
        CHECK(std::fabs(s.samples.front()) < 0.1f); // no click at the start...
        CHECK(std::fabs(s.samples.back()) < 0.02f); // ...or at the end
        CHECK(s.samples == all_b[i]->samples);      // identical on every run
    }
    CHECK_EQ(names.size(), all.size());             // every sound has its own name
}

TEST(synth_sound_lengths_and_shapes) {
    const ge::SoundBank b = ge::SoundBank::make();
    CHECK_NEAR(b.shot.duration_seconds(), 0.22, 0.001);
    CHECK_NEAR(b.win.duration_seconds(), 1.3, 0.001);
    CHECK(b.win.duration_seconds() > b.kill.duration_seconds());
    // A gunshot is loud at the start and has decayed by the end.
    const std::size_t n = b.shot.samples.size();
    const std::vector<float> head(b.shot.samples.begin(), b.shot.samples.begin() + static_cast<std::ptrdiff_t>(n / 5));
    const std::vector<float> tail(b.shot.samples.end() - static_cast<std::ptrdiff_t>(n / 5), b.shot.samples.end());
    CHECK(rms(head) > 5.0 * rms(tail));
    // The reload has two clicks with silence between them.
    const std::vector<float> gap(b.reload.samples.begin() + 12000, b.reload.samples.begin() + 24000);
    CHECK_NEAR(rms(gap), 0.0, 1e-6);
    const ge::SoundBank half = ge::SoundBank::make(22050);
    CHECK_EQ(half.hit.sample_rate, 22050);
    CHECK(half.hit.samples.size() * 2 <= b.hit.samples.size() + 2);
}

// ---- WAV -----------------------------------------------------------------------------------------
TEST(wav_writer_produces_a_valid_stereo_16_bit_file) {
    const auto path = std::filesystem::temp_directory_path() / "ge_test_audio.wav";
    const std::vector<float> samples = {0.0f, 0.5f, -0.5f, 1.0f, -1.0f, 2.0f};  // 3 stereo frames, last clamps
    CHECK(ge::write_wav_stereo16(path, samples, 44100).ok);
    std::ifstream f(path, std::ios::binary);
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    f.close();
    std::filesystem::remove(path);
    CHECK_EQ(bytes.size(), 44u + 12u);
    CHECK(std::string(bytes.begin(), bytes.begin() + 4) == "RIFF");
    CHECK(std::string(bytes.begin() + 8, bytes.begin() + 12) == "WAVE");
    auto u32 = [&](std::size_t o) { return static_cast<std::uint32_t>(bytes[o]) | (static_cast<std::uint32_t>(bytes[o + 1]) << 8) | (static_cast<std::uint32_t>(bytes[o + 2]) << 16) | (static_cast<std::uint32_t>(bytes[o + 3]) << 24); };
    auto s16 = [&](std::size_t o) { return static_cast<std::int16_t>(static_cast<unsigned>(bytes[o]) | (static_cast<unsigned>(bytes[o + 1]) << 8)); };
    CHECK_EQ(u32(4), 36u + 12u);                  // RIFF size
    CHECK_EQ(u32(24), 44100u);                    // sample rate
    CHECK_EQ(u32(28), 44100u * 4u);               // byte rate
    CHECK_EQ(s16(22), 2);                         // channels
    CHECK_EQ(s16(34), 16);                        // bits
    CHECK_EQ(u32(40), 12u);                       // data size
    CHECK_EQ(s16(44), 0);
    CHECK_EQ(s16(46), 16384);                     // 0.5
    CHECK_EQ(s16(48), -16384);
    CHECK_EQ(s16(50), 32767);
    CHECK_EQ(s16(52), -32767);
    CHECK_EQ(s16(54), 32767);                     // 2.0 clamped
    CHECK(!ge::write_wav_stereo16(path, {0.0f}, 44100).ok);       // odd sample count
    CHECK(!ge::write_wav_stereo16("/no/such/dir/x.wav", samples, 44100).ok);
}

// ---- panning from the camera ---------------------------------------------------------------------
TEST(audio_pan_follows_the_cameras_left_and_right) {
    const ge::Camera cam = ge::Camera::from_angle({5.0, 5.0}, 0.0, 66.0);  // facing +x; right is +y
    CHECK_NEAR(game::audio_pan_for(cam, {9.0, 5.0}), 0.0, 1e-6);
    CHECK(game::audio_pan_for(cam, {9.0, 7.0}) > 0.3f);
    CHECK(game::audio_pan_for(cam, {9.0, 3.0}) < -0.3f);
    CHECK_NEAR(game::audio_pan_for(cam, {5.0, 9.0}), 1.0, 1e-6);   // directly right
    CHECK_NEAR(game::audio_pan_for(cam, {5.0, 1.0}), -1.0, 1e-6);  // directly left
    CHECK_NEAR(game::audio_pan_for(cam, {5.0, 5.0}), 0.0, 0.0);    // on top of the listener
    const ge::Camera turned = ge::Camera::from_angle({5.0, 5.0}, ge::kPi, 66.0);  // facing -x: right is -y
    CHECK(game::audio_pan_for(turned, {5.0, 1.0}) > 0.9f);
}

// ---- game events -> sounds --------------------------------------------------------------------------
namespace {
struct Rig {
    std::unique_ptr<game::GameApplication> app;
    ge::AudioMixer mixer;
    ge::SoundBank bank = ge::SoundBank::make();
    std::unique_ptr<game::GameAudio> audio;

    explicit Rig(int waves = 3) {
        std::string error;
        auto level = ge::parse_level_text("waves: " + std::to_string(waves) + "\nmap:\n#########\n#P......#\n#.......#\n#########\n", error);
        app = std::make_unique<game::GameApplication>(*level);
        ge::EngineConfig cfg;
        CHECK(app->initialize(cfg).ok);
        audio = std::make_unique<game::GameAudio>(*app, mixer, bank);
    }
    std::size_t count(const char* name) const {
        const auto c = mixer.started_counts();
        const auto it = c.find(name);
        return it == c.end() ? 0u : it->second;
    }
};
}  // namespace

TEST(game_events_trigger_the_matching_sounds) {
    Rig rig;
    const ge::Vec2 p = rig.app->player().position;
    rig.app->place_single_target({p.x + 2.0, p.y});
    ge::InputState fire;
    fire.fire = true;
    for (int i = 0; i < 60; ++i) rig.app->fixed_update(fire, 1.0 / 60);
    CHECK(rig.count("shot") >= 3u);
    CHECK_EQ(rig.count("shot"), rig.app->shots_fired());
    CHECK_EQ(rig.count("hit"), rig.app->shots_hit());
    CHECK_EQ(rig.count("kill"), 1u);
    CHECK_EQ(rig.count("wave_clear"), 1u);
    CHECK_EQ(rig.count("win"), 0u);
}

TEST(reload_wave_start_win_and_menu_sounds) {
    Rig rig(1);
    ge::InputState reload_after_shot;
    reload_after_shot.fire = true;
    rig.app->fixed_update(reload_after_shot, 1.0 / 60);   // one shot so a reload is possible
    ge::InputState r;
    r.reload = true;
    rig.app->fixed_update(r, 1.0 / 60);
    CHECK_EQ(rig.count("reload"), 1u);

    ge::InputState pause;
    pause.pause = true;
    rig.app->fixed_update(pause, 1.0 / 60);
    rig.app->fixed_update(pause, 1.0 / 60);
    CHECK_EQ(rig.count("ui"), 2u);                         // pause + resume

    const ge::Vec2 p = rig.app->player().position;
    rig.app->place_single_target({p.x + 2.0, p.y});
    for (int i = 0; i < 200 && rig.app->state() == game::GameState::Playing; ++i) {
        ge::InputState fire;
        fire.fire = true;
        rig.app->fixed_update(fire, 1.0 / 60);
    }
    CHECK(rig.app->state() == game::GameState::Victory);
    CHECK_EQ(rig.count("win"), 1u);

    ge::InputState confirm;
    confirm.confirm = true;
    rig.app->fixed_update(confirm, 1.0 / 60);              // restart => wave 1 starts again
    CHECK_EQ(rig.count("wave_start"), 1u);
}

TEST(no_sounds_while_paused_and_none_after_the_audio_object_is_gone) {
    Rig rig;
    ge::InputState pause;
    pause.pause = true;
    rig.app->fixed_update(pause, 1.0 / 60);
    const std::size_t before = rig.mixer.total_started();
    ge::InputState fire;
    fire.fire = true;
    for (int i = 0; i < 30; ++i) rig.app->fixed_update(fire, 1.0 / 60);
    CHECK_EQ(rig.mixer.total_started(), before);          // paused: no shots, no sounds
    CHECK_EQ(rig.app->shots_fired(), 0u);

    rig.audio.reset();                                     // destroyed: handlers are unsubscribed
    rig.app->events().publish(game::ShotFired{});
    CHECK_EQ(rig.mixer.total_started(), before);
}

TEST(offline_capture_renders_audio_in_step_with_the_game_clock) {
    Rig rig;
    std::vector<float> sink;
    rig.audio->capture_offline(&sink);
    ge::InputState idle;
    for (int i = 0; i < 60; ++i) rig.app->fixed_update(idle, 1.0 / 60);
    CHECK(sink.size() / 2 >= 44099u && sink.size() / 2 <= 44101u);   // one second of audio
    CHECK_NEAR(rms(sink), 0.0, 0.0);                                  // nothing was playing

    ge::InputState fire;
    fire.fire = true;
    rig.app->fixed_update(fire, 1.0 / 60);
    const std::vector<float> after_one_second(sink.begin() + 88000, sink.end());
    CHECK(rms(after_one_second) > 0.0);  // the shot is in the stream
    rig.audio->capture_offline(nullptr);
    const std::size_t frozen = sink.size();
    rig.app->fixed_update(idle, 1.0 / 60);
    CHECK_EQ(sink.size(), frozen);
}

TEST(config_and_settings_audio_options) {
    ge::EngineConfig cfg;
    const char* argv[] = {"ge", "--no-audio", "--volume", "0.25", "--wav", "out.wav"};
    CHECK(ge::parse_arguments(6, argv, cfg).ok);
    CHECK(!cfg.audio && cfg.wav_path == "out.wav");
    CHECK_NEAR(cfg.volume, 0.25, 0.0);
    const char* bad[] = {"ge", "--volume", "1.5"};
    CHECK(!ge::parse_arguments(3, bad, cfg).ok);

    ge::EngineConfig s;
    CHECK(ge::apply_settings_text("volume = 0.3\naudio = off\n", s).ok);
    CHECK_NEAR(s.volume, 0.3, 0.0);
    CHECK(!s.audio);
    CHECK(!ge::apply_settings_text("volume = 2\n", s).ok);
    CHECK(!ge::apply_settings_text("audio = loud\n", s).ok);
}
