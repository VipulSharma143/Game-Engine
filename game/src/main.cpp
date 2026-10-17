#include <cstdio>
#include <memory>

#include "engine/core/Engine.hpp"
#include "engine/platform/HeadlessPlatform.hpp"
#include "game/DemoTour.hpp"
#include "game/GameApplication.hpp"

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

    if (config.max_frames == 0) {
        std::fprintf(stderr, "error: the headless platform has no window to close, so --frames must be > 0\n");
        return 64;
    }

    ge::Engine engine(config, std::make_unique<ge::HeadlessPlatform>(ge::ScriptedInput(game::demo_tour())));
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
    return 0;
}
