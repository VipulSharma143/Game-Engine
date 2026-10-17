#include <cmath>
#include "TestFramework.hpp"
#include "engine/physics/Collision.hpp"
#include "game/PlayerController.hpp"

namespace {
ge::InputState input(double forward, double strafe, double turn) {
    ge::InputState s;
    s.forward = forward;
    s.strafe = strafe;
    s.turn = turn;
    return s;
}
}  // namespace

TEST(player_moves_forward_along_heading) {
    game::Player p;
    p.position = {5.5, 5.5};
    p.angle = 0.0;
    game::update_player(p, input(1, 0, 0), ge::test_level().map, 0.5);
    CHECK_NEAR(p.position.x, 5.5 + 0.5 * p.move_speed, 1e-9);
    CHECK_NEAR(p.position.y, 5.5, 1e-9);
}

TEST(player_strafes_to_the_right) {
    game::Player p;
    p.position = {5.5, 5.5};
    p.angle = 0.0;  // facing +x, right is +y on the y-down map
    game::update_player(p, input(0, 1, 0), ge::test_level().map, 0.1);
    CHECK(p.position.y > 5.5);
    CHECK_NEAR(p.position.x, 5.5, 1e-9);
}

TEST(player_turns_and_wraps_angle) {
    game::Player p;
    p.position = {5.5, 5.5};
    p.angle = ge::kPi - 0.01;
    game::update_player(p, input(0, 0, 1), ge::test_level().map, 0.1);
    CHECK(p.angle < 0.0);  // wrapped past pi
    CHECK(p.angle >= -ge::kPi);
}

TEST(player_diagonal_speed_is_normalised) {
    game::Player p;
    p.position = {5.5, 5.5};
    game::update_player(p, input(1, 1, 0), ge::test_level().map, 0.1);
    const double dx = p.position.x - 5.5, dy = p.position.y - 5.5;
    CHECK_NEAR(std::sqrt(dx * dx + dy * dy), p.move_speed * 0.1, 1e-9);
}

TEST(player_cannot_walk_through_walls) {
    game::Player p;
    p.position = {1.5, 1.5};
    p.angle = ge::kPi;  // facing west into the border
    for (int i = 0; i < 120; ++i) game::update_player(p, input(1, 0, 0), ge::test_level().map, 1.0 / 60.0);
    CHECK(!ge::collides_aabb(ge::test_level().map, p.position, p.half_extent));
    CHECK(p.position.x >= 1.0 + p.half_extent - 1e-9);
}

TEST(player_never_ends_inside_a_wall_under_random_input) {
    game::Player p;
    p.position = ge::test_level().spawn;
    unsigned seed = 12345;
    auto next = [&seed] {
        seed = seed * 1664525u + 1013904223u;
        return static_cast<double>((seed >> 8) & 0xFFFF) / 32768.0 - 1.0;
    };
    for (int i = 0; i < 5000; ++i) {
        game::update_player(p, input(next(), next(), next()), ge::test_level().map, 1.0 / 60.0);
        CHECK(!ge::collides_aabb(ge::test_level().map, p.position, p.half_extent));
    }
}

TEST(player_input_is_clamped) {
    game::Player p;
    p.position = {5.5, 5.5};
    game::update_player(p, input(50, 0, 0), ge::test_level().map, 0.1);
    CHECK_NEAR(p.position.x - 5.5, p.move_speed * 0.1, 1e-9);
}

TEST(camera_follows_player) {
    game::Player p;
    p.position = {3.0, 4.0};
    p.angle = 1.0;
    const ge::Camera c = game::camera_for(p, 66.0);
    CHECK_NEAR(c.position.x, 3.0, 1e-12);
    CHECK_NEAR(c.direction.x, std::cos(1.0), 1e-12);
}
