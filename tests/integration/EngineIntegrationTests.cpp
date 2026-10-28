#include <cmath>
#include <memory>

#include "TestFramework.hpp"
#include "engine/core/Engine.hpp"
#include "engine/physics/Collision.hpp"
#include "engine/platform/HeadlessPlatform.hpp"
#include "game/DemoTour.hpp"
#include "game/GameApplication.hpp"

namespace {

ge::EngineConfig small_config(std::uint64_t frames) {
    ge::EngineConfig c;
    c.width = 96;
    c.height = 60;
    c.max_frames = frames;
    c.deterministic = true;
    c.log_level = ge::LogLevel::Off;
    return c;
}

struct RunResult {
    std::uint64_t checksum = 0;
    ge::Vec2 position;
    double angle = 0.0;
    ge::EngineStats stats;
};

RunResult run_demo(std::uint64_t frames) {
    ge::Engine engine(small_config(frames),
                      std::make_unique<ge::HeadlessPlatform>(ge::ScriptedInput(game::demo_tour())));
    CHECK(engine.initialize().ok);
    game::GameApplication app(ge::test_level());
    CHECK_EQ(engine.run(app), 0);
    return {engine.framebuffer().checksum(), app.player().position, app.player().angle, engine.stats()};
}

class FailingApp final : public ge::Application {
public:
    ge::Status initialize(const ge::EngineConfig&) override { return ge::Status::failure("nope"); }
    void fixed_update(const ge::InputState&, double) override { ++updates; }
    void render(ge::Framebuffer&) override { ++renders; }
    void shutdown() noexcept override { ++shutdowns; }
    bool wants_quit() const noexcept override { return false; }
    int updates = 0, renders = 0, shutdowns = 0;
};

class CountingApp final : public ge::Application {
public:
    ge::Status initialize(const ge::EngineConfig&) override { return ge::Status::success(); }
    void fixed_update(const ge::InputState&, double) override { ++updates; }
    void render(ge::Framebuffer& fb) override { fb.clear(1); ++renders; }
    void shutdown() noexcept override { ++shutdowns; }
    bool wants_quit() const noexcept override { return updates >= quit_after; }
    int updates = 0, renders = 0, shutdowns = 0, quit_after = 1000000;
};

// Reports one `use` press on the very first poll and nothing afterwards.
class OnePressPlatform final : public ge::IPlatform {
public:
    ge::Status init(const ge::EngineConfig&) override { return ge::Status::success(); }
    void shutdown() noexcept override {}
    void poll_input(ge::InputState& input) override { input.use = !pressed_; pressed_ = true; }
    void present(const ge::Framebuffer&, std::uint64_t) override {}
    void finalize(const ge::Framebuffer&) override {}
private:
    bool pressed_ = false;
};

class PressCountingApp final : public ge::Application {
public:
    ge::Status initialize(const ge::EngineConfig&) override { return ge::Status::success(); }
    void fixed_update(const ge::InputState& in, double) override { ++updates; if (in.use) ++presses; }
    void render(ge::Framebuffer&) override {}
    void shutdown() noexcept override {}
    bool wants_quit() const noexcept override { return updates >= 3; }
    int updates = 0, presses = 0;
};

}  // namespace

// The first frame of a real-time run is almost always shorter than one fixed step, so it runs no step. A press
// that arrives on such a frame must be delivered on the next step (exactly once), not lost.
TEST(engine_delivers_a_press_that_arrives_on_a_frame_without_a_fixed_step) {
    ge::EngineConfig c = small_config(0);
    c.deterministic = false;
    c.fixed_timestep = 0.02;
    ge::Engine engine(c, std::make_unique<OnePressPlatform>());
    CHECK(engine.initialize().ok);
    PressCountingApp app;
    CHECK_EQ(engine.run(app), 0);
    CHECK_EQ(app.presses, 1);
}

TEST(engine_initializes_and_shuts_down_cleanly) {
    ge::Engine engine(small_config(1), std::make_unique<ge::HeadlessPlatform>());
    CHECK(!engine.initialized());
    CHECK(engine.initialize().ok);
    CHECK(engine.initialized());
    CHECK(!engine.initialize().ok);  // double init rejected
    CHECK_EQ(engine.framebuffer().width(), 96);
    engine.shutdown();
    CHECK(!engine.initialized());
    engine.shutdown();  // idempotent
}

TEST(engine_rejects_invalid_config_and_missing_platform) {
    ge::EngineConfig bad = small_config(1);
    bad.width = 0;
    ge::Engine a(bad, std::make_unique<ge::HeadlessPlatform>());
    CHECK(!a.initialize().ok);
    ge::Engine b(small_config(1), nullptr);
    CHECK(!b.initialize().ok);
}

TEST(engine_run_requires_initialize) {
    ge::Engine engine(small_config(1), std::make_unique<ge::HeadlessPlatform>());
    CountingApp app;
    CHECK(engine.run(app) != 0);
    CHECK_EQ(app.renders, 0);
}

TEST(engine_does_not_shut_down_app_that_failed_to_start) {
    ge::Engine engine(small_config(5), std::make_unique<ge::HeadlessPlatform>());
    CHECK(engine.initialize().ok);
    FailingApp app;
    CHECK(engine.run(app) != 0);
    CHECK_EQ(app.updates, 0);
    CHECK_EQ(app.renders, 0);
    CHECK_EQ(app.shutdowns, 0);
}

TEST(engine_deterministic_loop_runs_exact_frame_and_step_counts) {
    ge::Engine engine(small_config(120), std::make_unique<ge::HeadlessPlatform>());
    CHECK(engine.initialize().ok);
    CountingApp app;
    CHECK_EQ(engine.run(app), 0);
    CHECK_EQ(app.renders, 120);
    CHECK_EQ(app.updates, 120);
    CHECK_EQ(app.shutdowns, 1);
    CHECK_EQ(engine.stats().frames, 120u);
    CHECK_NEAR(engine.stats().simulated_seconds, 2.0, 1e-9);
}

TEST(engine_stops_when_application_wants_to_quit) {
    ge::Engine engine(small_config(0), std::make_unique<ge::HeadlessPlatform>());
    CHECK(engine.initialize().ok);
    CountingApp app;
    app.quit_after = 10;
    CHECK_EQ(engine.run(app), 0);
    CHECK_EQ(app.updates, 10);
    CHECK_EQ(app.shutdowns, 1);
}

TEST(engine_realtime_clock_runs_and_clamps) {
    ge::EngineConfig c = small_config(20);
    c.deterministic = false;
    ge::Engine engine(c, std::make_unique<ge::HeadlessPlatform>());
    CHECK(engine.initialize().ok);
    CountingApp app;
    CHECK_EQ(engine.run(app), 0);
    CHECK_EQ(app.renders, 20);
    CHECK(app.updates <= 20 * c.max_catchup_steps);
}

TEST(headless_demo_is_deterministic_and_moves_the_player) {
    const RunResult a = run_demo(300);
    const RunResult b = run_demo(300);
    CHECK_EQ(a.checksum, b.checksum);
    CHECK_NEAR(a.position.x, b.position.x, 0.0);
    CHECK_NEAR(a.position.y, b.position.y, 0.0);
    const ge::Vec2 spawn = ge::test_level().spawn;
    CHECK(std::fabs(a.position.x - spawn.x) + std::fabs(a.position.y - spawn.y) > 1.0);
    CHECK(!ge::collides_aabb(ge::test_level().map, a.position, 0.25));
    CHECK(a.angle != 0.0);
    CHECK_EQ(a.stats.fixed_steps, 300u);
}

TEST(different_inputs_produce_different_frames) {
    const RunResult early = run_demo(10);
    const RunResult late = run_demo(200);
    CHECK(early.checksum != late.checksum);
}
