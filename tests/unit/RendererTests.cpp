#include <cmath>

#include "TestFramework.hpp"
#include "engine/renderer/Camera.hpp"
#include "engine/renderer/Framebuffer.hpp"
#include "engine/renderer/Raycaster.hpp"
#include "engine/renderer/Renderer.hpp"
#include "engine/world/World.hpp"

namespace {
ge::GridMap make_room() {
    // 8x8 room, solid border, empty inside.
    std::vector<std::uint8_t> cells(64, 0);
    for (int i = 0; i < 8; ++i) {
        cells[static_cast<std::size_t>(i)] = 1;
        cells[static_cast<std::size_t>(56 + i)] = 1;
        cells[static_cast<std::size_t>(i * 8)] = 1;
        cells[static_cast<std::size_t>(i * 8 + 7)] = 1;
    }
    return ge::GridMap(8, 8, std::move(cells));
}
}  // namespace

TEST(framebuffer_basics) {
    ge::Framebuffer fb(4, 3);
    CHECK_EQ(fb.width(), 4);
    CHECK_EQ(fb.height(), 3);
    CHECK_EQ(fb.pixels().size(), 12u);
    fb.clear(ge::pack_rgb(1, 2, 3));
    CHECK_EQ(fb.pixel(3, 2), ge::pack_rgb(1, 2, 3));
    fb.set_pixel(1, 1, ge::pack_rgb(255, 0, 0));
    CHECK_EQ(fb.pixel(1, 1), ge::pack_rgb(255, 0, 0));
    fb.set_pixel(-1, 0, 7);  // ignored
    fb.set_pixel(4, 0, 7);
    CHECK_EQ(fb.pixel(4, 0), 0u);
    CHECK_EQ(ge::red_of(ge::pack_rgb(10, 20, 30)), 10u);
    CHECK_EQ(ge::green_of(ge::pack_rgb(10, 20, 30)), 20u);
    CHECK_EQ(ge::blue_of(ge::pack_rgb(10, 20, 30)), 30u);
}

TEST(framebuffer_fill_column_clamps) {
    ge::Framebuffer fb(3, 5);
    fb.clear(0);
    fb.fill_column(1, -4, 99, 9);
    for (int y = 0; y < 5; ++y) CHECK_EQ(fb.pixel(1, y), 9u);
    CHECK_EQ(fb.pixel(0, 0), 0u);
    fb.fill_column(7, 0, 5, 9);  // off-screen column: no crash
    fb.fill_column(0, 3, 1, 9);  // empty range
    CHECK_EQ(fb.pixel(0, 2), 0u);
}

TEST(framebuffer_checksum_detects_changes) {
    ge::Framebuffer a(8, 8), b(8, 8);
    a.clear(5);
    b.clear(5);
    CHECK_EQ(a.checksum(), b.checksum());
    b.set_pixel(3, 3, 6);
    CHECK(a.checksum() != b.checksum());
}

TEST(framebuffer_resize_reuses_correct_size) {
    ge::Framebuffer fb(2, 2);
    fb.resize(10, 6);
    CHECK_EQ(fb.pixels().size(), 60u);
    fb.resize(-3, 4);
    CHECK_EQ(fb.pixels().size(), 0u);
}

TEST(ray_hits_known_wall_distance) {
    const ge::GridMap room = make_room();
    const ge::RayHit east = ge::cast_ray(room, {1.5, 3.5}, {1.0, 0.0});
    CHECK(east.hit);
    CHECK_EQ(east.cell_x, 7);
    CHECK(!east.y_side);
    CHECK_NEAR(east.distance, 7.0 - 1.5, 1e-9);
    const ge::RayHit south = ge::cast_ray(room, {1.5, 3.5}, {0.0, 1.0});
    CHECK(south.hit);
    CHECK(south.y_side);
    CHECK_NEAR(south.distance, 7.0 - 3.5, 1e-9);
    const ge::RayHit north = ge::cast_ray(room, {1.5, 3.5}, {0.0, -1.0});
    CHECK_NEAR(north.distance, 3.5 - 1.0, 1e-9);
    const ge::RayHit west = ge::cast_ray(room, {1.5, 3.5}, {-1.0, 0.0});
    CHECK_NEAR(west.distance, 1.5 - 1.0, 1e-9);
}

TEST(ray_diagonal_and_scaled_direction) {
    const ge::GridMap room = make_room();
    // Distance is in ray-parameter units, so doubling the direction halves it.
    const ge::RayHit a = ge::cast_ray(room, {1.5, 3.5}, {1.0, 0.25});
    const ge::RayHit b = ge::cast_ray(room, {1.5, 3.5}, {2.0, 0.5});
    CHECK(a.hit && b.hit);
    CHECK_NEAR(a.distance, 2.0 * b.distance, 1e-9);
}

TEST(ray_starting_inside_wall_reports_zero_distance) {
    const ge::GridMap room = make_room();
    const ge::RayHit h = ge::cast_ray(room, {0.5, 0.5}, {1.0, 0.0});
    CHECK(h.hit);
    CHECK_NEAR(h.distance, 0.0, 1e-12);
}

TEST(ray_outside_map_terminates) {
    const ge::GridMap room = make_room();
    const ge::RayHit h = ge::cast_ray(room, {100.0, 100.0}, {1.0, 1.0});
    CHECK(h.hit);  // out-of-range cells are solid
    const ge::RayHit empty = ge::cast_ray(ge::GridMap(), {0.5, 0.5}, {1.0, 0.0});
    CHECK(empty.hit);
}

TEST(camera_plane_is_right_hand_and_scaled_by_fov) {
    const ge::Camera c = ge::Camera::from_angle({2.0, 2.0}, 0.0, 90.0);
    CHECK_NEAR(c.direction.x, 1.0, 1e-12);
    CHECK_NEAR(c.plane.x, 0.0, 1e-12);
    CHECK_NEAR(c.plane.y, 1.0, 1e-9);  // tan(45 deg) = 1, pointing to +y = right of east
}

TEST(renderer_draws_wall_slices_and_depth) {
    const ge::GridMap room = make_room();
    ge::Framebuffer fb(64, 48);
    ge::Renderer renderer;
    const ge::Camera cam = ge::Camera::from_angle({3.5, 3.5}, 0.0, 66.0);
    renderer.render(room, cam, fb);
    CHECK_EQ(renderer.depth().size(), 64u);
    for (double d : renderer.depth()) CHECK(d > 0.0 && std::isfinite(d));
    // Dead ahead is the east wall (x = 7) seen from x = 3.5, so about 3.5 cells away.
    CHECK_NEAR(renderer.depth()[32], 3.5, 0.1);
    // Ceiling at the top, floor at the bottom, wall colour at the middle.
    CHECK(fb.pixel(32, 0) != fb.pixel(32, 24));
    CHECK(fb.pixel(32, 47) != fb.pixel(32, 24));
    CHECK(fb.pixel(32, 0) != fb.pixel(32, 47));
}

TEST(renderer_wall_height_decreases_with_distance) {
    const ge::GridMap room = make_room();
    ge::Framebuffer fb(32, 64);
    ge::Renderer renderer;
    auto wall_pixels = [&](double x) {
        renderer.render(room, ge::Camera::from_angle({x, 3.5}, 0.0, 66.0), fb);
        const std::uint32_t ceiling = fb.pixel(16, 0);
        const std::uint32_t floor_color = fb.pixel(16, 63);
        int n = 0;
        for (int y = 0; y < 64; ++y) {
            const std::uint32_t p = fb.pixel(16, y);
            if (p != ceiling && p != floor_color) ++n;
        }
        return n;
    };
    CHECK(wall_pixels(5.0) > wall_pixels(2.0));  // east wall at x = 7: nearer means taller
}

TEST(renderer_is_not_mirrored) {
    // Facing east in the test level: walls on the right (south) are closer than on the left.
    std::vector<std::uint8_t> cells(10 * 10, 0);
    for (int i = 0; i < 10; ++i) {
        cells[static_cast<std::size_t>(i)] = 1;
        cells[static_cast<std::size_t>(90 + i)] = 1;
        cells[static_cast<std::size_t>(i * 10)] = 1;
        cells[static_cast<std::size_t>(i * 10 + 9)] = 1;
    }
    cells[static_cast<std::size_t>(5 * 10 + 5)] = 2;  // block just south-east of the player
    cells[static_cast<std::size_t>(6 * 10 + 5)] = 2;
    const ge::GridMap map(10, 10, cells);
    ge::Framebuffer fb(80, 40);
    ge::Renderer renderer;
    renderer.render(map, ge::Camera::from_angle({3.5, 4.5}, 0.0, 66.0), fb);
    double left = 0.0, right = 0.0;
    for (int x = 0; x < 40; ++x) left += renderer.depth()[static_cast<std::size_t>(x)];
    for (int x = 40; x < 80; ++x) right += renderer.depth()[static_cast<std::size_t>(x)];
    CHECK(right < left);  // the block is on the right half of the view
}

TEST(renderer_is_deterministic) {
    ge::Framebuffer a(100, 60), b(100, 60);
    ge::Renderer ra, rb;
    const ge::Camera cam = ge::Camera::from_angle(ge::test_level().spawn, 0.3, 66.0);
    ra.render(ge::test_level().map, cam, a);
    rb.render(ge::test_level().map, cam, b);
    CHECK_EQ(a.checksum(), b.checksum());
    ra.render(ge::test_level().map, cam, a);  // re-render over old contents
    CHECK_EQ(a.checksum(), b.checksum());
}

TEST(renderer_handles_tiny_and_empty_targets) {
    ge::Renderer r;
    ge::Framebuffer empty;
    r.render(ge::test_level().map, ge::Camera{}, empty);  // no crash
    ge::Framebuffer tiny(1, 1);
    r.render(ge::test_level().map, ge::Camera::from_angle({1.5, 1.5}, 0.0, 66.0), tiny);
    CHECK_EQ(r.depth().size(), 1u);
}
