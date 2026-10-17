#include "engine/renderer/Renderer.hpp"

#include <algorithm>
#include <limits>

#include "engine/renderer/Raycaster.hpp"

namespace ge {
namespace {

constexpr std::uint32_t kCeiling = pack_rgb(40, 44, 64);
constexpr std::uint32_t kFloor = pack_rgb(70, 70, 70);

struct Rgb { unsigned r, g, b; };
constexpr Rgb kWallColors[10] = {
    {255, 0, 255},                  // 0 unused (empty cell)
    {200, 60, 60},  {60, 190, 60},  {70, 90, 220},  {210, 190, 60}, {60, 190, 190},
    {190, 70, 190}, {230, 230, 230}, {230, 140, 50}, {140, 90, 50},
};

std::uint32_t shade_wall(std::uint8_t id, bool y_side, double distance) noexcept {
    const Rgb base = kWallColors[id < 10 ? id : 0];
    double light = 1.0 / (1.0 + distance * distance * 0.02);
    if (y_side) light *= 0.7;
    light = clamp(light, 0.08, 1.0);
    return pack_rgb(static_cast<unsigned>(base.r * light), static_cast<unsigned>(base.g * light),
                    static_cast<unsigned>(base.b * light));
}

}  // namespace

void Renderer::render(const GridMap& map, const Camera& camera, Framebuffer& target) {
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
