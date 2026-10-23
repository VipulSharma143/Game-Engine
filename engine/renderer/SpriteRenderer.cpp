#include "engine/renderer/SpriteRenderer.hpp"

#include <algorithm>
#include <cmath>

namespace ge {

Projection project_point(const Camera& cam, Vec2 world, int screen_width) noexcept {
    // Solve rel = a * direction + b * plane. A ray through column k is direction + plane * k,
    // so the point lands on k = b / a and its perpendicular depth is a.
    const Vec2 rel = world - cam.position;
    const double det = cam.direction.x * cam.plane.y - cam.direction.y * cam.plane.x;
    Projection p;
    if (std::fabs(det) < 1e-12) return p;
    const double a = (rel.x * cam.plane.y - rel.y * cam.plane.x) / det;
    const double b = (cam.direction.x * rel.y - cam.direction.y * rel.x) / det;
    if (a <= 0.05) return p;
    p.visible = true;
    p.depth = a;
    p.screen_x = (1.0 + b / a) * 0.5 * screen_width;
    return p;
}

namespace {
std::uint32_t shade(std::uint32_t base, double light, double flash) noexcept {
    auto ch = [&](unsigned c) {
        double v = static_cast<double>(c) * light;
        v = v + (255.0 - v) * flash;
        return static_cast<unsigned>(clamp(v, 0.0, 255.0));
    };
    return pack_rgb(ch(red_of(base)), ch(green_of(base)), ch(blue_of(base)));
}
}  // namespace

int draw_sprites(const Camera& cam, std::span<const double> depth, std::span<const Sprite> sprites,
                 Framebuffer& target) {
    const int w = target.width();
    const int h = target.height();
    if (w <= 0 || h <= 0 || depth.size() != static_cast<std::size_t>(w)) return 0;

    struct Item { const Sprite* s; Projection p; };
    std::vector<Item> items;
    items.reserve(sprites.size());
    for (const Sprite& s : sprites) {
        const Projection p = project_point(cam, s.position, w);
        if (p.visible) items.push_back({&s, p});
    }
    std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.p.depth > b.p.depth; });

    int drawn = 0;
    for (const Item& it : items) {
        const double px = it.s->size * h / it.p.depth;  // sprite diameter in pixels
        if (px < 1.0) continue;
        const double r = px * 0.5;
        const int x0 = static_cast<int>(std::floor(it.p.screen_x - r));
        const int x1 = static_cast<int>(std::ceil(it.p.screen_x + r));
        const double cy = h * 0.5;
        const double fog = clamp(1.0 / (1.0 + it.p.depth * it.p.depth * 0.02), 0.15, 1.0);
        bool any = false;
        for (int x = std::max(x0, 0); x < std::min(x1, w); ++x) {
            if (it.p.depth >= depth[static_cast<std::size_t>(x)]) continue;  // hidden behind a wall
            const double u = (x + 0.5 - it.p.screen_x) / r;                   // -1..1 across
            const double chord = it.s->diamond ? 1.0 - std::fabs(u) : 1.0 - u * u;
            if (chord <= 0.0) continue;
            const double half_height = it.s->diamond ? r * chord : r * std::sqrt(chord);
            const int y0 = std::max(static_cast<int>(std::floor(cy - half_height)), 0);
            const int y1 = std::min(static_cast<int>(std::ceil(cy + half_height)), h);
            for (int y = y0; y < y1; ++y) {
                const double v = (y + 0.5 - cy) / r;
                if (it.s->diamond) {
                    const double edge = std::fabs(u) + std::fabs(v);
                    if (edge > 1.0) continue;
                    // Facets: the upper-left half is lighter, the lower-right darker, with a bright rim.
                    const double facet = (u + v < 0.0) ? 1.1 : 0.75;
                    const double rim = edge > 0.85 ? 1.3 : 1.0;
                    target.set_pixel(x, y, shade(it.s->color, fog * facet * rim, it.s->flash));
                    any = true;
                    continue;
                }
                const double d2 = u * u + v * v;
                if (d2 > 1.0) continue;
                // Sphere-like shading with a highlight toward the upper left.
                const double lit = 0.55 + 0.45 * (1.0 - d2) + 0.5 * std::max(0.0, 1.0 - 4.0 * ((u + 0.35) * (u + 0.35) + (v + 0.35) * (v + 0.35)));
                const double rim = d2 > 0.8 ? 0.55 : 1.0;
                target.set_pixel(x, y, shade(it.s->color, fog * lit * rim, it.s->flash));
                any = true;
            }
        }
        if (any) ++drawn;
    }
    return drawn;
}

}  // namespace ge
