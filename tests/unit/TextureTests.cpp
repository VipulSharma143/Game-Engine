#include <cmath>
#include <set>
#include <vector>

#include "TestFramework.hpp"
#include "engine/optimized/OptimizedRoutines.hpp"
#include "engine/renderer/Camera.hpp"
#include "engine/renderer/Raycaster.hpp"
#include "engine/renderer/Renderer.hpp"
#include "engine/renderer/Texture.hpp"
#include "engine/world/World.hpp"

namespace {
ge::GridMap room8() {
    std::vector<std::uint8_t> c(64, 0);
    for (std::size_t i = 0; i < 8; ++i) { c[i] = 1; c[56 + i] = 1; c[i * 8] = 1; c[i * 8 + 7] = 1; }
    return ge::GridMap(8, 8, std::move(c));
}

std::size_t distinct_colours(const ge::Texture& t) {
    std::set<std::uint32_t> s(t.pixels().begin(), t.pixels().end());
    return s.size();
}
}  // namespace

TEST(texture_hash_is_deterministic_and_spreads) {
    CHECK_EQ(ge::texture_hash(3, 4, 5), ge::texture_hash(3, 4, 5));
    CHECK(ge::texture_hash(3, 4, 5) != ge::texture_hash(4, 3, 5));
    CHECK(ge::texture_hash(3, 4, 5) != ge::texture_hash(3, 4, 6));
}

TEST(texture_wraps_coordinates) {
    ge::Texture t;
    t.set(0, 0, 0xFF112233u);
    CHECK_EQ(t.at(ge::Texture::kSize, ge::Texture::kSize), 0xFF112233u);
    CHECK_EQ(t.at(-ge::Texture::kSize, 2 * ge::Texture::kSize), 0xFF112233u);
}

TEST(procedural_set_is_deterministic_detailed_and_distinct) {
    const ge::TextureSet a = ge::TextureSet::make_procedural();
    const ge::TextureSet b = ge::TextureSet::make_procedural();
    for (int id = 0; id <= 9; ++id) {
        CHECK(a.wall(static_cast<std::uint8_t>(id)).pixels() == b.wall(static_cast<std::uint8_t>(id)).pixels());
        if (id >= 1) CHECK(distinct_colours(a.wall(static_cast<std::uint8_t>(id))) > 20u);  // not flat
    }
    CHECK(a.wall(1).pixels() != a.wall(2).pixels());
    CHECK(a.floor(0).pixels() != a.floor(1).pixels());
    CHECK(distinct_colours(a.ceiling()) > 5u);
    CHECK_EQ(a.wall(0).at(8, 0), ge::pack_rgb(255, 0, 255));         // "missing" marker checker
    CHECK_EQ(a.wall(0).at(0, 0), ge::pack_rgb(0, 0, 0));
    CHECK(a.wall(200).pixels() == a.wall(0).pixels());               // unknown ids use it
    CHECK(a.floor(7).pixels() == a.floor(1).pixels());               // variant is taken mod 2
}

TEST(wall_texture_u_is_in_range_and_straight_on_is_centred) {
    const ge::GridMap room = room8();
    const ge::Vec2 origin{3.5, 3.5};
    const ge::Vec2 ray{1.0, 0.0};
    const ge::RayHit hit = ge::cast_ray(room, origin, ray);
    CHECK(hit.hit);
    CHECK_NEAR(ge::wall_texture_u(origin, ray, hit), 0.5, 1e-9);
    for (int i = 0; i < 360; i += 7) {
        const double a = i * ge::kPi / 180.0;
        const ge::Vec2 r{std::cos(a), std::sin(a)};
        const double u = ge::wall_texture_u(origin, r, ge::cast_ray(room, origin, r));
        CHECK(u >= 0.0 && u < 1.0);
    }
}

TEST(wall_texture_is_never_mirrored_in_any_facing_direction) {
    // For a camera looking at a flat wall, u must increase from the left of the screen to the right.
    const ge::GridMap room = room8();
    for (int quarter = 0; quarter < 4; ++quarter) {
        const ge::Camera cam = ge::Camera::from_angle({3.5, 3.5}, quarter * ge::kPi / 2.0, 66.0);
        // The wall spans several cells, so u wraps at cell borders: step to step it must advance
        // by a small positive amount modulo 1 (a mirrored texture would step backwards, ~0.93+).
        double previous = -1.0;
        for (int x = 0; x < 64; ++x) {
            const double k = 2.0 * (x + 0.5) / 64 - 1.0;
            const ge::Vec2 ray = cam.direction + cam.plane * k;
            const ge::RayHit hit = ge::cast_ray(room, cam.position, ray);
            const double u = ge::wall_texture_u(cam.position, ray, hit);
            if (previous >= 0.0) {
                double d = u - previous;
                if (d < 0.0) d += 1.0;
                CHECK(d > 0.0 && d < 0.2);
            }
            previous = u;
        }
    }
}

TEST(textured_renderer_adds_detail_and_fades_into_fog) {
    const ge::GridMap room = room8();
    const ge::TextureSet tex = ge::TextureSet::make_procedural();
    const ge::Camera cam = ge::Camera::from_angle({1.5, 3.5}, 0.0, 66.0);
    ge::Framebuffer flat(160, 100), textured(160, 100);
    ge::Renderer rf, rt;
    rt.set_textures(&tex);
    CHECK(rt.textured() && !rf.textured());
    rf.render(room, cam, flat);
    rt.render(room, cam, textured);
    CHECK(flat.checksum() != textured.checksum());

    // Far wall column has texture detail: many distinct colours along it, unlike flat shading.
    std::set<std::uint32_t> wall_colours, flat_colours;
    for (int y = 44; y < 56; ++y) { wall_colours.insert(textured.pixel(80, y)); flat_colours.insert(flat.pixel(80, y)); }
    CHECK(wall_colours.size() > 3u);
    CHECK_EQ(flat_colours.size(), 1u);

    // Floor rows are textured too, and the horizon row is darker (fogged) than the bottom row.
    std::set<std::uint32_t> floor_colours;
    for (int x = 0; x < 160; ++x) floor_colours.insert(textured.pixel(x, 95));
    CHECK(floor_colours.size() > 8u);
    auto brightness = [](std::uint32_t c) { return ge::red_of(c) + ge::green_of(c) + ge::blue_of(c); };
    unsigned near_sum = 0, far_sum = 0;
    for (int x = 0; x < 160; ++x) { near_sum += brightness(textured.pixel(x, 97)); far_sum += brightness(textured.pixel(x, 52)); }
    CHECK(near_sum > far_sum);
    CHECK_EQ(rt.depth().size(), 160u);
}

TEST(textured_render_is_deterministic) {
    const ge::GridMap room = room8();
    const ge::TextureSet tex = ge::TextureSet::make_procedural();
    const ge::Camera cam = ge::Camera::from_angle({2.2, 4.1}, 0.7, 66.0);
    ge::Framebuffer a(96, 64), b(96, 64);
    ge::Renderer r1, r2;
    r1.set_textures(&tex);
    r2.set_textures(&tex);
    r1.render(room, cam, a);
    r2.render(room, cam, b);
    CHECK_EQ(a.checksum(), b.checksum());
}

TEST(textured_render_is_pixel_identical_for_every_fog_kernel) {
    const ge::GridMap room = room8();
    const ge::TextureSet tex = ge::TextureSet::make_procedural();
    const ge::opt::FogKernel saved = ge::opt::active_fog_kernel();
    std::vector<std::uint64_t> sums;
    for (ge::opt::FogKernel k : {ge::opt::FogKernel::Scalar, ge::opt::FogKernel::Sse2, ge::opt::FogKernel::Avx2}) {
        if (!ge::opt::set_fog_kernel(k)) continue;
        std::uint64_t sum = 0;
        for (int i = 0; i < 24; ++i) {  // several sizes (odd widths and heights hit the SIMD tails) and angles
            const int w = 61 + i * 7, h = 40 + i * 3;
            ge::Framebuffer fb(w, h);
            ge::Renderer r;
            r.set_textures(&tex);
            r.render(room, ge::Camera::from_angle({2.3 + 0.1 * i, 3.7}, 0.4 * i, 66.0), fb);
            sum = sum * 1099511628211ull + fb.checksum();
        }
        sums.push_back(sum);
    }
    ge::opt::set_fog_kernel(saved);
    CHECK(!sums.empty());
    for (std::uint64_t s : sums) CHECK_EQ(s, sums.front());
}
