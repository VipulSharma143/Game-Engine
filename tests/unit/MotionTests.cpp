#include <array>
#include <string_view>

#include "TestFramework.hpp"
#include "engine/world/MotionBatch.hpp"
#include "game/GameApplication.hpp"

namespace {
ge::GridMap open_room() {  // 6x6, walls around the border
    std::vector<std::uint8_t> c(36, 0);
    for (std::size_t i = 0; i < 6; ++i) { c[i] = 1; c[30 + i] = 1; c[6 * i] = 1; c[6 * i + 5] = 1; }
    return ge::GridMap(6, 6, c);
}
}  // namespace

TEST(motion_moves_points_in_open_space) {
    ge::MotionBatch m;
    m.add({2.0, 2.0}, {1.0, 0.5});
    m.step(0.5, open_room());
    CHECK_NEAR(m.position(0).x, 2.5, 1e-12);
    CHECK_NEAR(m.position(0).y, 2.25, 1e-12);
}

TEST(motion_bounces_off_walls_and_never_enters_them) {
    const ge::GridMap room = open_room();
    ge::MotionBatch m;
    m.add({4.5, 3.0}, {2.0, 0.0});
    m.add({3.0, 1.5}, {0.3, -2.0});
    m.add({1.5, 1.5}, {-1.7, -1.3});
    for (int i = 0; i < 2000; ++i) {
        m.step(1.0 / 60.0, room);
        for (std::size_t k = 0; k < m.size(); ++k) {
            const ge::Vec2 p = m.position(k);
            CHECK(!room.is_solid(static_cast<int>(p.x), static_cast<int>(p.y)));
        }
    }
}

TEST(motion_reverses_velocity_on_hit) {
    ge::MotionBatch m;
    m.add({4.9, 3.0}, {1.0, 0.0});
    m.step(0.5, open_room());  // would enter x=5.4 (wall column)
    CHECK_NEAR(m.position(0).x, 4.9, 1e-12);
    CHECK_NEAR(m.velocity(0).x, -1.0, 1e-12);
}

TEST(motion_is_deterministic) {
    auto run = [] {
        ge::MotionBatch m;
        for (int i = 0; i < 5; ++i) m.add({1.5 + i * 0.5, 1.5 + i * 0.3}, {0.7 + i, 1.1 - i * 0.4});
        for (int s = 0; s < 500; ++s) m.step(1.0 / 60.0, open_room());
        return m.checksum();
    };
    CHECK_EQ(run(), run());
}

TEST(game_spawns_targets_as_entities_and_moves_them) {
    game::GameApplication app(ge::test_level());
    ge::EngineConfig cfg;
    CHECK(app.initialize(cfg).ok);
    CHECK_EQ(app.target_count(), 8u);
    CHECK_EQ(app.entities().size(), 8u);
    const ge::Vec2 before = app.target_position(0);
    const std::uint64_t sum_before = app.target_checksum();
    ge::InputState idle;
    for (int i = 0; i < 60; ++i) app.fixed_update(idle, 1.0 / 60.0);
    const ge::Vec2 after = app.target_position(0);
    CHECK(before.x != after.x || before.y != after.y);
    CHECK(sum_before != app.target_checksum());
}
