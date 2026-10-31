#include "engine/renderer/SpriteRenderer.hpp"

#include <algorithm>
#include <cmath>

#include "engine/optimized/OptimizedRoutines.hpp"

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

// Fixed-point light (8.8, 256 = unchanged) and flash (0..256) used by every sprite pixel, so that the picture path
// (NASM kernel) and the shaded-shape path (C++) agree on what a shaded pixel is: ge::scalar::shade_u32.
std::uint32_t light_fixed(double light) noexcept { return static_cast<std::uint32_t>(clamp(light * 256.0 + 0.5, 0.0, 1023.0)); }
std::uint32_t flash_fixed(double flash) noexcept { return static_cast<std::uint32_t>(clamp(flash * 256.0 + 0.5, 0.0, 256.0)); }

}  // namespace

int draw_sprites(const Camera& cam, std::span<const double> depth, std::span<const Sprite> sprites,
                 Framebuffer& target) {
    SpriteScratch scratch;
    return draw_sprites(cam, depth, sprites, target, scratch);
}

int draw_sprites(const Camera& cam, std::span<const double> depth, std::span<const Sprite> sprites,
                 Framebuffer& target, SpriteScratch& scratch) {
    const int w = target.width();
    const int h = target.height();
    if (w <= 0 || h <= 0 || depth.size() != static_cast<std::size_t>(w)) return 0;

    using Item = SpriteScratch::Item;
    std::vector<Item>& items = scratch.items;
    items.clear();
    for (const Sprite& s : sprites) {
        const Projection p = project_point(cam, s.position, w);
        if (p.visible) items.push_back({&s, p});
    }
    std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.projection.depth > b.projection.depth; });

    std::span<std::uint32_t> pixels = target.pixels();
    int drawn = 0;
    for (const Item& it : items) {
        const Sprite& spr = *it.sprite;
        const Projection& proj = it.projection;
        const double px = spr.size * h / proj.depth;  // sprite diameter (or picture height) in pixels
        if (px < 1.0) continue;
        const double r = px * 0.5;
        const double fog = clamp(1.0 / (1.0 + proj.depth * proj.depth * 0.02), 0.15, 1.0) * spr.light;
        const double lift = spr.z * h / proj.depth;  // pixels above the usual position
        const std::uint32_t flash = flash_fixed(spr.flash);
        bool any = false;

        if (spr.image && !spr.image->empty()) {
            // A picture: nearest-neighbour sampled, standing on the floor line, transparent pixels skipped.
            // The texel column depends only on x and the texel row only on y, so both are worked out once and the
            // pixels of each row are written by one span kernel call. A column hidden behind a wall is marked -1.
            const SpriteImage& img = *spr.image;
            const double half_w = r * static_cast<double>(img.width) / static_cast<double>(img.height);
            const double floor_y = h * 0.5 + 0.5 * h / proj.depth;
            const double top = floor_y - px - lift;
            const double left = proj.screen_x - half_w;
            const int x0 = std::max(static_cast<int>(std::floor(left)), 0);
            const int x1 = std::min(static_cast<int>(std::ceil(proj.screen_x + half_w)), w);
            const int y0 = std::max(static_cast<int>(std::floor(top)), 0);
            const int y1 = std::min(static_cast<int>(std::ceil(top + px)), h);
            if (x1 <= x0 || y1 <= y0) continue;
            const std::size_t n = static_cast<std::size_t>(x1 - x0);
            scratch.columns.resize(n);
            bool any_column = false;
            for (int x = x0; x < x1; ++x) {
                std::int32_t& column = scratch.columns[static_cast<std::size_t>(x - x0)];
                if (proj.depth >= depth[static_cast<std::size_t>(x)]) { column = -1; continue; }
                column = std::min(static_cast<int>((x + 0.5 - left) / (2.0 * half_w) * img.width), img.width - 1);
                any_column = true;
            }
            if (!any_column) continue;
            auto span = opt::make_sprite_span_params(light_fixed(fog), flash, scratch.columns.data());
            for (int y = y0; y < y1; ++y) {
                const int iy = std::min(static_cast<int>((y + 0.5 - top) / px * img.height), img.height - 1);
                if (iy < 0) continue;  // above the picture: transparent
                span.row = img.pixels.data() + static_cast<std::size_t>(iy) * static_cast<std::size_t>(img.width);
                any |= opt::sprite_span(pixels.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x0), n, span);
            }
            if (any) ++drawn;
            continue;
        }

        const int x0 = static_cast<int>(std::floor(proj.screen_x - r));
        const int x1 = static_cast<int>(std::ceil(proj.screen_x + r));
        const double cy = h * 0.5 - lift;
        for (int x = std::max(x0, 0); x < std::min(x1, w); ++x) {
            if (proj.depth >= depth[static_cast<std::size_t>(x)]) continue;  // hidden behind a wall
            const double u = (x + 0.5 - proj.screen_x) / r;                   // -1..1 across
            const double chord = spr.diamond ? 1.0 - std::fabs(u) : 1.0 - u * u;
            if (chord <= 0.0) continue;
            const double half_height = spr.diamond ? r * chord : r * std::sqrt(chord);
            const int y0 = std::max(static_cast<int>(std::floor(cy - half_height)), 0);
            const int y1 = std::min(static_cast<int>(std::ceil(cy + half_height)), h);
            for (int y = y0; y < y1; ++y) {
                const double v = (y + 0.5 - cy) / r;
                if (spr.diamond) {
                    const double edge = std::fabs(u) + std::fabs(v);
                    if (edge > 1.0) continue;
                    // Facets: the upper-left half is lighter, the lower-right darker, with a bright rim.
                    const double facet = (u + v < 0.0) ? 1.1 : 0.75;
                    const double rim = edge > 0.85 ? 1.3 : 1.0;
                    target.set_pixel(x, y, scalar::shade_u32(spr.color, light_fixed(fog * facet * rim), flash));
                    any = true;
                    continue;
                }
                const double d2 = u * u + v * v;
                if (d2 > 1.0) continue;
                // Sphere-like shading with a highlight toward the upper left.
                const double lit = 0.55 + 0.45 * (1.0 - d2) + 0.5 * std::max(0.0, 1.0 - 4.0 * ((u + 0.35) * (u + 0.35) + (v + 0.35) * (v + 0.35)));
                const double rim = d2 > 0.8 ? 0.55 : 1.0;
                target.set_pixel(x, y, scalar::shade_u32(spr.color, light_fixed(fog * lit * rim), flash));
                any = true;
            }
        }
        if (any) ++drawn;
    }
    return drawn;
}

}  // namespace ge
