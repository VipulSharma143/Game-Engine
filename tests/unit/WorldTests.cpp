#include <array>
#include <string>
#include <string_view>

#include "TestFramework.hpp"
#include "engine/physics/Collision.hpp"
#include "engine/physics/Movement.hpp"
#include "engine/world/World.hpp"

TEST(gridmap_cells_and_out_of_bounds_are_solid) {
    const ge::GridMap m(2, 2, {0, 3, 0, 0});
    CHECK_EQ(m.cell(1, 0), 3);
    CHECK(!m.is_solid(0, 0));
    CHECK(m.is_solid(1, 0));
    CHECK(m.is_solid(-1, 0));
    CHECK(m.is_solid(0, 2));
    CHECK(m.is_solid(5, 5));
}

TEST(parse_level_reads_walls_and_spawn) {
    const std::array<std::string_view, 3> rows = {"####", "#P2#", "####"};
    std::string error;
    const auto level = ge::parse_level(rows, error);
    CHECK(level.has_value());
    CHECK_EQ(level->map.width(), 4);
    CHECK_EQ(level->map.height(), 3);
    CHECK_EQ(level->map.cell(2, 1), 2);
    CHECK_EQ(level->map.cell(0, 0), 1);
    CHECK_NEAR(level->spawn.x, 1.5, 1e-12);
    CHECK_NEAR(level->spawn.y, 1.5, 1e-12);
}

TEST(parse_level_rejects_bad_input) {
    std::string error;
    const std::array<std::string_view, 2> ragged = {"###", "##"};
    CHECK(!ge::parse_level(ragged, error).has_value());
    CHECK(!error.empty());
    const std::array<std::string_view, 1> bad_char = {"#x#"};
    CHECK(!ge::parse_level(bad_char, error).has_value());
    const std::array<std::string_view, 1> two_spawns = {"PP"};
    CHECK(!ge::parse_level(two_spawns, error).has_value());
    CHECK(!ge::parse_level(std::span<const std::string_view>{}, error).has_value());
}

TEST(builtin_test_level_is_sane) {
    const ge::Level& level = ge::test_level();
    CHECK_EQ(level.map.width(), 16);
    CHECK_EQ(level.map.height(), 16);
    CHECK(!ge::collides_aabb(level.map, level.spawn, 0.25));
    for (int i = 0; i < 16; ++i) {  // closed border
        CHECK(level.map.is_solid(i, 0));
        CHECK(level.map.is_solid(i, 15));
        CHECK(level.map.is_solid(0, i));
        CHECK(level.map.is_solid(15, i));
    }
}

TEST(aabb_collision_against_cells) {
    const ge::GridMap m(5, 5, [] {
        std::vector<std::uint8_t> c(25, 0);
        c[2 * 5 + 3] = 1;  // wall at (3, 2)
        return c;
    }());
    CHECK(!ge::collides_aabb(m, {1.5, 2.5}, 0.25));
    CHECK(ge::collides_aabb(m, {3.5, 2.5}, 0.25));
    CHECK(ge::collides_aabb(m, {2.9, 2.5}, 0.25));   // overlaps the wall's left edge
    CHECK(!ge::collides_aabb(m, {2.7, 2.5}, 0.25));  // just clear (right edge at 2.95)
    CHECK(ge::collides_aabb(m, {0.1, 0.1}, 0.25));   // sticks out of the map
}

TEST(movement_stops_at_wall_and_slides) {
    const ge::GridMap& map = ge::test_level().map;
    // Walk east from the spawn into the east border: must stop short of the wall.
    ge::Vec2 p = ge::move_with_collision(map, {13.5, 1.5}, {5.0, 0.0}, 0.25);
    CHECK(p.x < 15.0 - 0.25 + 1e-9);
    CHECK(p.x > 14.0);
    CHECK_NEAR(p.y, 1.5, 1e-12);
    // Diagonal into the north wall: x keeps moving, y is blocked (sliding).
    p = ge::move_with_collision(map, {5.5, 1.3}, {1.0, -1.0}, 0.25);
    CHECK(p.x > 6.0);
    CHECK(p.y >= 1.25 - 1e-9);
}

TEST(movement_does_not_tunnel_with_large_deltas) {
    const ge::GridMap& map = ge::test_level().map;
    const ge::Vec2 p = ge::move_with_collision(map, {1.5, 1.5}, {0.0, 40.0}, 0.25);
    CHECK(p.y < 15.0);  // never ends up past the south border
    CHECK(!ge::collides_aabb(map, p, 0.25));
}

TEST(movement_in_free_space_is_exact) {
    const ge::GridMap& map = ge::test_level().map;
    const ge::Vec2 p = ge::move_with_collision(map, {5.5, 5.5}, {0.5, -0.25}, 0.25);
    CHECK_NEAR(p.x, 6.0, 1e-12);
    CHECK_NEAR(p.y, 5.25, 1e-12);
}
