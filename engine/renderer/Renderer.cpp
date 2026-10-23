#include "engine/renderer/Renderer.hpp"

#include <algorithm>
#include <limits>

#include <cmath>

#include "engine/optimized/OptimizedRoutines.hpp"
#include "engine/renderer/Raycaster.hpp"

namespace ge {
namespace {

constexpr std::uint32_t kCeiling = pack_rgb(40, 44, 64);
constexpr std::uint32_t kFloor = pack_rgb(70, 70, 70);

struct Rgb { unsigned r, g, b; };
constexpr Rgb kWallColors[12] = {
    {255, 0, 255},                  // 0 unused (empty cell)
    {200, 60, 60},  {60, 190, 60},  {70, 90, 220},  {210, 190, 60}, {60, 190, 190},
    {190, 70, 190}, {230, 230, 230}, {230, 140, 50}, {140, 90, 50},
    {120, 80, 40},  {170, 40, 40},  // 10 door, 11 locked door
};

std::uint32_t shade_wall(std::uint8_t id, bool y_side, double distance) noexcept {
    const Rgb base = kWallColors[id < 12 ? id : 0];
    double light = 1.0 / (1.0 + distance * distance * 0.02);
    if (y_side) light *= 0.7;
    light = clamp(light, 0.08, 1.0);
    return pack_rgb(static_cast<unsigned>(base.r * light), static_cast<unsigned>(base.g * light),
                    static_cast<unsigned>(base.b * light));
}

}  // namespace

void Renderer::render(const GridMap& map, const Camera& camera, Framebuffer& target) {
    if (textures_) render_textured(map, camera, target);
    else render_flat(map, camera, target);
}

void Renderer::render_flat(const GridMap& map, const Camera& camera, Framebuffer& target) {
    const int w = target.width();
    const int h = target.height();
    if (w <= 0 || h <= 0) return;
    if (depth_.size() != static_cast<std::size_t>(w)) depth_.assign(static_cast<std::size_t>(w), 0.0);

    constexpr double kNoHit = std::numeric_limits<double>::infinity();
    const int half = h / 2;
    for (int x = 0; x < w; ++x) {
        const double camera_x = 2.0 * (x + 0.5) / w - 1.0;
        const Vec2 ray = camera.direction + camera.plane * camera_x;
        const RayHit hit = cast_ray(map, camera.position, ray);
        if (!hit.hit) {
            depth_[static_cast<std::size_t>(x)] = kNoHit;
            target.fill_column(x, 0, half, kCeiling);
            target.fill_column(x, half, h, kFloor);
            continue;
        }
        const double dist = std::max(hit.distance, 1e-4);
        depth_[static_cast<std::size_t>(x)] = dist;
        const double half_line = std::min(0.5 * h / dist, static_cast<double>(h));
        const int top = half - static_cast<int>(half_line);
        const int bottom = half + static_cast<int>(half_line);
        target.fill_column(x, 0, top, kCeiling);
        target.fill_column(x, top, bottom, shade_wall(hit.wall_id, hit.y_side, dist));
        target.fill_column(x, bottom, h, kFloor);
    }
}

}  // namespace ge

namespace ge {
namespace {

// Fog colour; each pixel is blended towards it by opt::fog_blend (NASM SSE2/AVX2 or C++).
constexpr std::uint32_t kFogColor = 0xFF080A12u;  // pack_rgb(8, 10, 18)

// light is 0..256: 256 = unshaded, 0 = fully fogged.
inline int light_for(double distance, double side_factor) noexcept {
    const double l = side_factor / (1.0 + distance * distance * 0.02);
    return static_cast<int>(clamp(l, 0.0, 1.0) * 256.0);
}

}  // namespace

void Renderer::render_textured(const GridMap& map, const Camera& camera, Framebuffer& target) {
    const int w = target.width();
    const int h = target.height();
    if (w <= 0 || h <= 0) return;
    if (depth_.size() != static_cast<std::size_t>(w)) depth_.assign(static_cast<std::size_t>(w), 0.0);

    const TextureSet& tex = *textures_;
    const int half = h / 2;
    std::span<std::uint32_t> px = target.pixels();

    // ---- floor and ceiling, one horizontal span per row -------------------------------------
    const Vec2 left = camera.direction - camera.plane;
    const Vec2 span = camera.plane * 2.0;
    for (int y = 0; y < h; ++y) {
        const bool is_floor = y >= half;
        const double p = is_floor ? (y + 0.5 - half) : (half - y - 0.5);
        if (p <= 0.0) continue;
        const double row_distance = 0.5 * h / p;
        const int light = light_for(row_distance, 1.0);
        double fx = camera.position.x + row_distance * (left.x + span.x * 0.5 / w);
        double fy = camera.position.y + row_distance * (left.y + span.y * 0.5 / w);
        const double sx = row_distance * span.x / w;
        const double sy = row_distance * span.y / w;
        std::uint32_t* row = px.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(w);
        for (int x = 0; x < w; ++x, fx += sx, fy += sy) {
            const double cx = std::floor(fx), cy = std::floor(fy);
            const int tx = static_cast<int>((fx - cx) * Texture::kSize);
            const int ty = static_cast<int>((fy - cy) * Texture::kSize);
            const Texture& t = is_floor ? tex.floor(static_cast<int>(cx) + static_cast<int>(cy)) : tex.ceiling();
            row[x] = t.at(tx, ty);  // raw texel; the whole row is fogged in one kernel call below
        }
        opt::fog_blend(row, row, static_cast<std::size_t>(w), static_cast<std::uint32_t>(light), kFogColor);
    }

    // ---- walls, drawn over the floor and ceiling --------------------------------------------
    constexpr double kNoHit = std::numeric_limits<double>::infinity();
    for (int x = 0; x < w; ++x) {
        const double camera_x = 2.0 * (x + 0.5) / w - 1.0;
        const Vec2 ray = camera.direction + camera.plane * camera_x;
        const RayHit hit = cast_ray(map, camera.position, ray);
        if (!hit.hit) {
            depth_[static_cast<std::size_t>(x)] = kNoHit;
            continue;
        }
        const double dist = std::max(hit.distance, 1e-4);
        depth_[static_cast<std::size_t>(x)] = dist;

        const double line_height = h / dist;
        const double top_f = half - line_height * 0.5;
        const int y0 = std::max(0, static_cast<int>(std::floor(top_f)));
        const int y1 = std::min(h, static_cast<int>(std::ceil(half + line_height * 0.5)));
        const Texture& t = tex.wall(hit.wall_id);
        const int tx = static_cast<int>(wall_texture_u(camera.position, ray, hit) * Texture::kSize);
        const int light = light_for(dist, hit.y_side ? 0.75 : 1.0);
        const double step = Texture::kSize / line_height;
        double tpos = (y0 + 0.5 - top_f) * step;
        const int n = y1 - y0;
        if (n <= 0) continue;
        column_.resize(static_cast<std::size_t>(n));
        for (int i = 0; i < n; ++i, tpos += step) column_[static_cast<std::size_t>(i)] = t.at(tx, static_cast<int>(tpos));
        opt::fog_blend(column_.data(), column_.data(), static_cast<std::size_t>(n), static_cast<std::uint32_t>(light), kFogColor);
        for (int i = 0; i < n; ++i)
            px[static_cast<std::size_t>(y0 + i) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x)] = column_[static_cast<std::size_t>(i)];
    }
}

}  // namespace ge
