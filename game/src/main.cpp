#include <cstdio>
#include <cstring>
#include <memory>

#include "engine/core/Engine.hpp"
#include "engine/optimized/OptimizedRoutines.hpp"
#include "engine/platform/HeadlessPlatform.hpp"
#include "engine/platform/PlatformFactory.hpp"
#include "game/DemoTour.hpp"
#include "game/GameApplication.hpp"

namespace {
bool has_flag(int argc, char** argv, const char* flag) {
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], flag) == 0) return true;
    return false;
}
}  // namespace

int main(int argc, char** argv) {
    ge::EngineConfig config;
    config.max_frames = 300;
    config.deterministic = true;

    const ge::ParseResult parsed = ge::parse_arguments(argc, argv, config);
    if (!parsed.ok) {
        std::fprintf(stderr, "error: %s\n\n%s", parsed.error.c_str(), ge::usage_text());
        return 64;
    }
    if (parsed.help_requested) {
        std::fputs(ge::usage_text(), stdout);
        return 0;
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
        std::printf("[demo] window mode. WASD / arrows move, mouse or <- -> look, click or SPACE fire,\n"
                    "       R reload, F3 debug overlay, TAB release mouse, ESC quit.\n");
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

    game::GameApplication app(ge::test_level());
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
    std::printf("game: wave=%d score=%d shots=%u hits=%u destroyed=%u ammo=%d/%d\n", app.wave(), app.score(),
                app.shots_fired(), app.shots_hit(), app.targets_destroyed(), app.weapon().ammo, app.weapon().magazine);
    return 0;
}
