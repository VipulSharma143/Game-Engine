#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

#include "engine/audio/AudioMixer.hpp"
#include "engine/audio/Synth.hpp"
#include "engine/audio/Wav.hpp"
#include "engine/core/Engine.hpp"
#include "engine/core/Settings.hpp"
#include "engine/resources/AssetPath.hpp"
#include "engine/resources/ResourceManager.hpp"
#include "engine/optimized/OptimizedRoutines.hpp"
#include "engine/platform/HeadlessPlatform.hpp"
#include "engine/platform/PlatformFactory.hpp"
#include "engine/world/LevelLoader.hpp"
#include "game/DemoTour.hpp"
#include "game/GameApplication.hpp"
#include "game/GameAudio.hpp"

#if defined(GE_HAVE_SDL)
#include "engine/audio/SDLAudioDevice.hpp"
#endif

namespace {
bool has_flag(int argc, char** argv, const char* flag) {
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], flag) == 0) return true;
    return false;
}
const char* arg_value(int argc, char** argv, const char* flag) {
    for (int i = 1; i + 1 < argc; ++i)
        if (std::strcmp(argv[i], flag) == 0) return argv[i + 1];
    return nullptr;
}
}  // namespace

int main(int argc, char** argv) {
    ge::EngineConfig config;
    config.max_frames = 300;
    config.deterministic = true;

    // Precedence: built-in defaults < settings file < command line.
    {
        const char* explicit_path = arg_value(argc, argv, "--settings");
        const std::filesystem::path path = explicit_path ? explicit_path : "settings.ini";
        std::error_code ec;
        if (explicit_path != nullptr || std::filesystem::is_regular_file(path, ec)) {
            const ge::Status s = ge::apply_settings_file(path, config);
            if (!s) {
                std::fprintf(stderr, "error: %s\n", s.message.c_str());
                return 64;
            }
        }
    }

    const ge::ParseResult parsed = ge::parse_arguments(argc, argv, config);
    if (!parsed.ok) {
        std::fprintf(stderr, "error: %s\n\n%s", parsed.error.c_str(), ge::usage_text());
        return 64;
    }
    if (parsed.help_requested) {
        std::fputs(ge::usage_text(), stdout);
        return 0;
    }

    // ---- assets and level ----------------------------------------------------------------------
    std::optional<ge::ResourceManager> resources;
    {
        std::optional<std::filesystem::path> assets = config.assets_dir.empty()
                                                          ? ge::find_assets_dir(argv[0])
                                                          : std::optional<std::filesystem::path>(config.assets_dir);
        if (assets && ge::looks_like_assets_dir(*assets)) resources.emplace(*assets);
        else if (!config.assets_dir.empty()) {
            std::fprintf(stderr, "error: '%s' is not an assets directory (it has no 'levels' folder)\n", config.assets_dir.c_str());
            return 1;
        }
    }
    if (config.list_levels) {
        if (!resources) { std::fprintf(stderr, "error: assets directory not found (use --assets DIR or set GE_ASSETS)\n"); return 1; }
        for (const std::string& n : resources->list_levels()) std::printf("%s\n", n.c_str());
        return 0;
    }
    ge::Level level = ge::test_level();
    if (!config.level.empty()) {
        std::string error;
        std::optional<ge::Level> loaded;
        if (resources) loaded = resources->load_level(config.level, error);
        else loaded = ge::load_level_file(config.level, error);
        if (!loaded) {
            std::fprintf(stderr, "error: %s\n", error.c_str());
            return 1;
        }
        level = std::move(*loaded);
    }

    // ---- choose window or headless -------------------------------------------------------
    bool window = false;
    if (config.mode == ge::RunMode::Window) {
        window = true;
    } else if (config.mode == ge::RunMode::Auto) {
        if (ge::window_platform_compiled() && ge::display_available()) {
            window = true;
        } else {
            std::printf("[demo] no window: %s; running the scripted headless demo (use --window to force)\n",
                        ge::window_platform_compiled() ? "no display found" : "built without SDL3");
        }
    }

    std::unique_ptr<ge::IPlatform> platform;
    if (window) {
        platform = ge::make_window_platform();
        if (!platform) {
            std::fprintf(stderr, "error: this build has no SDL3 support. Install SDL3 and rebuild "
                                 "(./build.sh --sdl on), or use --headless.\n");
            return 1;
        }
        // Interactive defaults: run until the window closes, on the real clock.
        if (!has_flag(argc, argv, "--frames")) config.max_frames = 0;
        if (!has_flag(argc, argv, "--deterministic")) config.deterministic = false;
        if (!has_flag(argc, argv, "--no-menu")) config.start_in_menu = true;
        std::printf("[demo] window mode. WASD / arrows move, mouse or <- -> look, click or SPACE fire,\n"
                    "       R reload, F3 debug overlay, TAB release mouse, ESC pause (Q quits from a menu).\n");
    } else {
        if (config.max_frames == 0) {
            std::fprintf(stderr, "error: the headless platform has no window to close, so --frames must be > 0\n");
            return 64;
        }
        platform = std::make_unique<ge::HeadlessPlatform>(ge::ScriptedInput(game::demo_tour()));
    }

    ge::Engine engine(config, std::move(platform));
    const ge::Status started = engine.initialize();
    if (!started) {
        std::fprintf(stderr, "error: %s\n", started.message.c_str());
        return 1;
    }

    game::GameApplication app(std::move(level));

    // ---- audio -----------------------------------------------------------------------------------
    // The mixer is always the same code; a window plays it through SDL, headless can render it to a WAV.
    ge::AudioMixer mixer(44100);
    mixer.set_master_volume(static_cast<float>(config.volume));
    const ge::SoundBank bank = ge::SoundBank::make(mixer.sample_rate());
    std::unique_ptr<game::GameAudio> audio;
    std::vector<float> captured;
#if defined(GE_HAVE_SDL)
    ge::SDLAudioDevice audio_device;
#endif
    bool audio_device_open = false;
    if (config.audio) {
        audio = std::make_unique<game::GameAudio>(app, mixer, bank);
        if (window) {
#if defined(GE_HAVE_SDL)
            const ge::Status opened = audio_device.open(mixer);
            audio_device_open = opened.ok;
            if (!opened) std::fprintf(stderr, "[demo] audio disabled: %s\n", opened.message.c_str());
#endif
            if (!config.wav_path.empty()) std::fprintf(stderr, "[demo] --wav is only used in headless mode\n");
        } else if (!config.wav_path.empty()) {
            audio->capture_offline(&captured);
        }
    }
    (void)audio_device_open;
    const int rc = engine.run(app);
    if (rc != 0) return rc;

    const ge::EngineStats& stats = engine.stats();
    std::printf("frames=%llu steps=%llu simulated=%.3fs player=(%.3f, %.3f) angle=%.3f checksum=%016llx\n",
                static_cast<unsigned long long>(stats.frames), static_cast<unsigned long long>(stats.fixed_steps),
                stats.simulated_seconds, app.player().position.x, app.player().position.y, app.player().angle,
                static_cast<unsigned long long>(engine.framebuffer().checksum()));
    std::printf("world: %zu entities, %zu moving targets, motion backend: %s, target checksum=%016llx\n",
                app.entities().size(), app.target_count(),
                ge::opt::asm_enabled() ? "NASM x86-64 (ge_vec2_madd, SSE2)" : "C++ fallback",
                static_cast<unsigned long long>(app.target_checksum()));
    std::printf("level: %s\n", app.level().name.c_str());
    if (config.audio) {
        std::printf("audio: %zu sounds triggered", mixer.total_started());
        const char* sep = " (";
        for (const auto& [name, count] : mixer.started_counts()) { std::printf("%s%s=%zu", sep, name.c_str(), count); sep = ", "; }
        std::printf("%s\n", mixer.total_started() ? ")" : "");
        if (!config.wav_path.empty() && !window) {
            const ge::Status w = ge::write_wav_stereo16(config.wav_path, captured, mixer.sample_rate());
            if (w) std::printf("audio: wrote %s (%.2f s, 44.1 kHz stereo)\n", config.wav_path.c_str(),
                               static_cast<double>(captured.size() / 2) / mixer.sample_rate());
            else std::fprintf(stderr, "error: %s\n", w.message.c_str());
        }
    }
    std::printf("game: wave=%d score=%d shots=%u hits=%u destroyed=%u ammo=%d/%d\n", app.wave(), app.score(),
                app.shots_fired(), app.shots_hit(), app.targets_destroyed(), app.weapon().ammo, app.weapon().magazine);
    return 0;
}
