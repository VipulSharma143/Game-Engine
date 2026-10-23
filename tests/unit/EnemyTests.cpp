#include <cmath>
#include <string>
#include <vector>

#include "TestFramework.hpp"
#include "engine/world/LevelLoader.hpp"
#include "game/Enemy.hpp"

namespace {

ge::GridMap map_from(std::initializer_list<const char*> rows) {
    std::string error;
    std::vector<std::string_view> r(rows.begin(), rows.end());
    const auto level = ge::parse_level(r, error);
    return level->map;
}

double dist(ge::Vec2 a, ge::Vec2 b) { return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y)); }

// 9x5 room split by a wall with a gap at the bottom:  enemy left, player right.
ge::GridMap split_room() {
    return map_from({"#########",
                     "#...#...#",
                     "#...#...#",
                     "#...#...#",
                     "#.......#",
                     "#########"});
}

}  // namespace

TEST(line_of_sight_sees_through_open_space_and_stops_at_walls) {
    const ge::GridMap m = split_room();
    CHECK(game::line_of_sight(m, {1.5, 1.5}, {3.5, 3.5}));
    CHECK(!game::line_of_sight(m, {1.5, 2.5}, {6.5, 2.5}));  // the dividing wall is in between
    CHECK(game::line_of_sight(m, {1.5, 4.5}, {7.5, 4.5}));   // along the gap row
    CHECK(game::line_of_sight(m, {2.5, 2.5}, {2.5, 2.5}));   // same point
}

TEST(distance_field_routes_around_walls_and_reports_unreachable_cells) {
    const ge::GridMap m = split_room();
    game::DistanceField f;
    f.rebuild(m, 6, 2);
    CHECK_EQ(f.distance(6, 2), 0);
    CHECK_EQ(f.distance(5, 2), 1);
    CHECK_EQ(f.distance(4, 2), -1);                 // wall cell
    CHECK(f.distance(1, 1) > f.distance(1, 4));     // the left room must go through the bottom gap
    CHECK_EQ(f.distance(-1, 0), -1);                // outside the map
    // From the left room, the first move heads towards the gap (down/right), not into the wall.
    const ge::Vec2 d = f.direction_from({1.5, 1.5});
    CHECK_NEAR(std::sqrt(d.x * d.x + d.y * d.y), 1.0, 1e-9);
    CHECK(d.y > 0.0);
    const ge::Vec2 goal = f.direction_from({6.5, 2.5});
    CHECK_NEAR(goal.x, 0.0, 0.0);
    CHECK_NEAR(goal.y, 0.0, 0.0);
    const ge::Vec2 in_wall = f.direction_from({4.5, 2.5});
    CHECK_NEAR(in_wall.x + in_wall.y, 0.0, 0.0);
}

TEST(distance_field_does_not_cut_corners_and_treats_doors_as_solid) {
    // The only diagonal link between the two open cells passes between two wall corners.
    const ge::GridMap corner = map_from({"####", "#.##", "##.#", "####"});
    game::DistanceField f;
    f.rebuild(corner, 1, 1);
    CHECK_EQ(f.distance(2, 2), -1);

    const ge::GridMap doors = map_from({"#####", "#.D.#", "#####"});
    f.rebuild(doors, 3, 1);
    CHECK_EQ(f.distance(1, 1), -1);                  // closed door blocks the route
    ge::GridMap opened = doors;
    opened.set_cell(2, 1, 0);
    f.rebuild(opened, 3, 1);
    CHECK_EQ(f.distance(1, 1), 2);
}

TEST(enemy_wakes_only_for_a_visible_nearby_player_or_noise) {
    const ge::GridMap m = split_room();
    game::DistanceField f;
    f.rebuild(m, 6, 2);
    game::Enemy far_blind;
    game::enemy_think(far_blind, {1.5, 2.5}, {6.5, 2.5}, m, f, false, 0.016);   // wall between: no line of sight
    CHECK(far_blind.state == game::EnemyState::Idle);

    game::Enemy sees;
    game::enemy_think(sees, {1.5, 4.5}, {6.5, 4.5}, m, f, false, 0.016);        // clear line, within sight
    CHECK(sees.state == game::EnemyState::Chase);

    game::Enemy too_far;
    too_far.sight = 3.0;
    game::enemy_think(too_far, {1.5, 4.5}, {7.5, 4.5}, m, f, false, 0.016);     // visible but beyond its sight
    CHECK(too_far.state == game::EnemyState::Idle);

    game::Enemy hears;
    game::enemy_think(hears, {1.5, 2.5}, {6.5, 2.5}, m, f, true, 0.016);        // gunfire wakes it through the wall
    CHECK(hears.state == game::EnemyState::Chase);
}

TEST(enemy_chases_towards_the_player_and_attacks_on_a_cooldown) {
    const ge::GridMap m = map_from({"##########", "#........#", "#........#", "##########"});
    game::DistanceField f;
    f.rebuild(m, 7, 1);
    game::Enemy e;
    e.state = game::EnemyState::Chase;
    const game::EnemyDecision d = game::enemy_think(e, {2.5, 1.5}, {7.5, 1.5}, m, f, false, 0.1);
    CHECK(d.move.x > 0.1 && std::fabs(d.move.y) < 1e-9);  // straight at the player
    CHECK_NEAR(d.move.x, e.speed * 0.1, 1e-9);
    CHECK(!d.attack);

    // In range: stops, bites immediately, then waits for the cooldown.
    game::Enemy biter;
    biter.state = game::EnemyState::Chase;
    const ge::Vec2 p{6.5, 1.5};
    game::EnemyDecision b = game::enemy_think(biter, {6.0, 1.5}, p, m, f, false, 0.016);
    CHECK(biter.state == game::EnemyState::Attack);
    CHECK(b.attack);
    CHECK_NEAR(b.move.x, 0.0, 0.0);
    int bites = 0;
    for (int i = 0; i < 120; ++i)  // 1.9 s more at 60 Hz
        if (game::enemy_think(biter, {6.0, 1.5}, p, m, f, false, 1.0 / 60).attack) ++bites;
    CHECK_EQ(bites, 2);  // at 0.9 s and 1.8 s

    // The player backs away: it gives up the attack and chases again.
    game::enemy_think(biter, {6.0, 1.5}, {8.5, 1.5}, m, f, false, 0.016);
    CHECK(biter.state == game::EnemyState::Chase);
}

TEST(slide_move_stops_at_walls_and_slides_along_them) {
    const ge::GridMap m = map_from({"#####", "#...#", "#...#", "#####"});
    const ge::Vec2 free = game::slide_move(m, {2.5, 1.5}, {0.2, 0.1}, 0.3);
    CHECK_NEAR(free.x, 2.7, 1e-12);
    CHECK_NEAR(free.y, 1.6, 1e-12);
    const ge::Vec2 blocked = game::slide_move(m, {3.5, 1.5}, {0.5, 0.2}, 0.3);  // into the east wall, along the south
    CHECK_NEAR(blocked.x, 3.5, 1e-12);       // x is blocked
    CHECK_NEAR(blocked.y, 1.7, 1e-12);       // y still slides
    const ge::Vec2 stuck = game::slide_move(m, {3.5, 2.4}, {0.5, 0.5}, 0.3);
    CHECK(stuck.x <= 3.7 && stuck.y <= 2.7);  // never ends up inside a wall
}

TEST(enemy_finds_its_way_around_a_wall_to_the_player) {
    const ge::GridMap m = split_room();
    game::DistanceField f;
    game::Enemy e;
    ge::Vec2 pos{1.5, 1.5};
    const ge::Vec2 player{6.5, 1.5};  // right behind the wall: no line of sight at the start
    f.rebuild(m, 6, 1);
    bool attacked = false;
    for (int i = 0; i < 60 * 15 && !attacked; ++i) {
        const game::EnemyDecision d = game::enemy_think(e, pos, player, m, f, true, 1.0 / 60);
        pos = game::slide_move(m, pos, d.move, e.radius);
        attacked = d.attack;
    }
    CHECK(attacked);
    CHECK(dist(pos, player) <= e.attack_range + 1e-6);
}
