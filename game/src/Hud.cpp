#include "game/Hud.hpp"

#include <cstdio>
#include <string>

#include "engine/renderer/TextRenderer.hpp"

namespace game {
namespace {

void fill_rect(ge::Framebuffer& fb, int x0, int y0, int x1, int y1, std::uint32_t c) {
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x) fb.set_pixel(x, y, c);
}

void fill_disc(ge::Framebuffer& fb, int cx, int cy, int r, std::uint32_t c) {
    for (int y = -r; y <= r; ++y)
        for (int x = -r; x <= r; ++x)
            if (x * x + y * y <= r * r) fb.set_pixel(cx + x, cy + y, c);
}

}  // namespace

void draw_weapon_view(ge::Framebuffer& fb, double flash, bool reloading) {
    const int w = fb.width(), h = fb.height();
    const int cx = w / 2;
    const int kick = static_cast<int>(flash * 6.0) + (reloading ? 14 : 0);
    const int base = h + kick;
    fill_rect(fb, cx - 14, base - 22, cx + 14, h, ge::pack_rgb(70, 72, 80));       // body
    fill_rect(fb, cx - 14, base - 22, cx + 14, base - 20, ge::pack_rgb(120, 124, 135));  // top highlight
    fill_rect(fb, cx - 4, base - 40, cx + 4, base - 22, ge::pack_rgb(55, 57, 64));  // barrel
    fill_rect(fb, cx - 4, base - 40, cx - 2, base - 22, ge::pack_rgb(100, 104, 114));
    if (flash > 0.6) {  // muzzle flash
        fill_disc(fb, cx, base - 44, 7, ge::pack_rgb(255, 210, 90));
        fill_disc(fb, cx, base - 44, 3, ge::pack_rgb(255, 255, 220));
    }
}

void draw_hud(ge::Framebuffer& fb, const HudData& d) {
    const int w = fb.width(), h = fb.height();
    const std::uint32_t white = ge::pack_rgb(235, 235, 235);
    const std::uint32_t yellow = ge::pack_rgb(250, 220, 90);
    const std::uint32_t red = ge::pack_rgb(255, 70, 60);

    // Crosshair (gap in the middle) and hit marker.
    const int cx = w / 2, cy = h / 2;
    const std::uint32_t cross = d.hit_marker > 0.0 ? red : white;
    for (int i = 3; i <= 6; ++i) {
        fb.set_pixel(cx - i, cy, cross); fb.set_pixel(cx + i, cy, cross);
        fb.set_pixel(cx, cy - i, cross); fb.set_pixel(cx, cy + i, cross);
    }
    if (d.hit_marker > 0.0)
        for (int i = 2; i <= 5; ++i) {
            fb.set_pixel(cx - i, cy - i, red); fb.set_pixel(cx + i, cy - i, red);
            fb.set_pixel(cx - i, cy + i, red); fb.set_pixel(cx + i, cy + i, red);
        }

    char buf[64];
    std::snprintf(buf, sizeof buf, "WAVE %d", d.wave);
    ge::draw_text_shadowed(fb, 4, 4, buf, yellow);
    std::snprintf(buf, sizeof buf, "TARGETS %d", d.targets_left);
    ge::draw_text_shadowed(fb, 4, 14, buf, white);

    std::snprintf(buf, sizeof buf, "SCORE %05d", d.score);
    ge::draw_text_shadowed(fb, w - ge::text_width(buf) - 4, 4, buf, white);

    if (d.reloading) {
        ge::draw_text_shadowed(fb, 4, h - 12, "RELOADING", red);
        const int bar = static_cast<int>(d.reload_fraction * 40.0);
        fill_rect(fb, 4, h - 18, 4 + 40, h - 15, ge::pack_rgb(40, 40, 40));
        fill_rect(fb, 4, h - 18, 4 + bar, h - 15, yellow);
    } else {
        std::snprintf(buf, sizeof buf, "AMMO %02d/%02d", d.ammo, d.magazine);
        ge::draw_text_shadowed(fb, 4, h - 12, buf, d.ammo <= 3 ? red : white);
    }

    if (d.between_waves) {
        const char* msg = "WAVE CLEARED!";
        ge::draw_text_shadowed(fb, (w - ge::text_width(msg, 2)) / 2, h / 4, msg, yellow, 2);
    }

    if (d.debug) {
        std::snprintf(buf, sizeof buf, "FPS %.0f  %.1f MS", d.fps, d.frame_ms);
        ge::draw_text_shadowed(fb, w - ge::text_width(buf) - 4, 16, buf, ge::pack_rgb(120, 255, 120));
        std::snprintf(buf, sizeof buf, "ENTITIES %zu", d.entities);
        ge::draw_text_shadowed(fb, w - ge::text_width(buf) - 4, 26, buf, ge::pack_rgb(120, 255, 120));
        std::snprintf(buf, sizeof buf, "MATH: %s", d.backend);
        ge::draw_text_shadowed(fb, w - ge::text_width(buf) - 4, 36, buf, ge::pack_rgb(120, 255, 120));
    }
}

}  // namespace game
