#include <cmath>
#include <string>
#include <vector>

#include "TestFramework.hpp"
#include "engine/core/Config.hpp"
#include "engine/core/GameLoop.hpp"
#include "engine/core/Logger.hpp"
#include "engine/core/Time.hpp"

namespace {
std::vector<std::string> g_lines;
void capture(ge::LogLevel, const char* message, void*) { g_lines.emplace_back(message); }
}  // namespace

TEST(config_defaults_are_valid) { CHECK(ge::validate_config(ge::EngineConfig{}).empty()); }

TEST(config_rejects_bad_values) {
    ge::EngineConfig c;
    c.width = 4;
    CHECK(!ge::validate_config(c).empty());
    c = {};
    c.fov_degrees = 200.0;
    CHECK(!ge::validate_config(c).empty());
    c = {};
    c.fixed_timestep = 0.0;
    CHECK(!ge::validate_config(c).empty());
    c = {};
    c.max_frame_time = c.fixed_timestep / 2.0;
    CHECK(!ge::validate_config(c).empty());
    c = {};
    c.max_catchup_steps = 0;
    CHECK(!ge::validate_config(c).empty());
}

TEST(config_parses_arguments) {
    ge::EngineConfig c;
    const char* argv[] = {"ge-demo", "--width", "640", "--height", "400", "--frames", "10", "--deterministic",
                          "--ascii", "--log-level", "warn", "--fov", "75.5"};
    const ge::ParseResult r = ge::parse_arguments(13, argv, c);
    CHECK(r.ok);
    CHECK_EQ(c.width, 640);
    CHECK_EQ(c.height, 400);
    CHECK_EQ(c.max_frames, 10u);
    CHECK(c.deterministic);
    CHECK(c.headless.ascii_summary);
    CHECK(c.log_level == ge::LogLevel::Warn);
    CHECK_NEAR(c.fov_degrees, 75.5, 1e-12);
}

TEST(config_reports_argument_errors) {
    ge::EngineConfig c;
    const char* unknown[] = {"x", "--bogus"};
    CHECK(!ge::parse_arguments(2, unknown, c).ok);
    const char* missing[] = {"x", "--width"};
    CHECK(!ge::parse_arguments(2, missing, c).ok);
    const char* bad[] = {"x", "--width", "12abc"};
    CHECK(!ge::parse_arguments(3, bad, c).ok);
    const char* level[] = {"x", "--log-level", "loud"};
    CHECK(!ge::parse_arguments(3, level, c).ok);
    const char* help[] = {"x", "--help"};
    CHECK(ge::parse_arguments(2, help, c).help_requested);
}

TEST(frame_timer_clamps_spikes_and_bad_values) {
    ge::FrameTimer t(0.1);
    CHECK_NEAR(t.advance(0.016), 0.016, 1e-12);
    CHECK_NEAR(t.advance(5.0), 0.1, 1e-12);  // stall clamped
    CHECK_NEAR(t.advance(-1.0), 0.0, 1e-12);  // clock going backwards
    CHECK_NEAR(t.advance(std::nan("")), 0.0, 1e-12);
    CHECK_EQ(t.frame_count(), 4u);
    CHECK_EQ(t.clamped_count(), 1u);
    CHECK_NEAR(t.total(), 0.116, 1e-12);
}

TEST(monotonic_clock_never_goes_backwards) {
    double prev = ge::monotonic_seconds();
    for (int i = 0; i < 1000; ++i) {
        const double now = ge::monotonic_seconds();
        CHECK(now >= prev);
        prev = now;
    }
}

TEST(fixed_stepper_counts_steps) {
    ge::FixedStepper s(0.01, 5);
    CHECK_EQ(s.consume(0.005), 0);
    CHECK_EQ(s.consume(0.005), 1);
    CHECK_EQ(s.consume(0.025), 2);
    CHECK(s.alpha() >= 0.0 && s.alpha() < 1.0);
}

TEST(fixed_stepper_prevents_catch_up_spiral) {
    ge::FixedStepper s(0.01, 5);
    CHECK_EQ(s.consume(10.0), 5);  // capped
    CHECK(s.accumulator() < 0.01);  // excess discarded, not carried over
    CHECK_EQ(s.consume(0.0), 0);
}

TEST(logger_filters_by_level_and_uses_sink) {
    g_lines.clear();
    ge::Logger::set_sink(capture);
    ge::Logger::set_level(ge::LogLevel::Warn);
    GE_LOG_INFO("hidden");
    GE_LOG_WARN("shown %d", 7);
    GE_LOG_ERROR("also shown");
    ge::Logger::set_level(ge::LogLevel::Off);
    GE_LOG_ERROR("silenced");
    ge::Logger::set_sink(nullptr);
    ge::Logger::set_level(ge::LogLevel::Info);
    CHECK_EQ(g_lines.size(), 2u);
    CHECK_EQ(g_lines[0], std::string("shown 7"));
}

TEST(logger_parses_level_names) {
    ge::LogLevel l = ge::LogLevel::Info;
    CHECK(ge::Logger::parse_level("debug", l));
    CHECK(l == ge::LogLevel::Debug);
    CHECK(!ge::Logger::parse_level("nope", l));
}
