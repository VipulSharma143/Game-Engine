#include "game/Hud.hpp"

#include <algorithm>
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

    // Health bar above the ammo line.
    {
        const int bar_w = 60;
        const int filled = d.max_health > 0 ? std::clamp(d.health * bar_w / d.max_health, 0, bar_w) : 0;
        const std::uint32_t colour = d.health > 50 ? ge::pack_rgb(80, 220, 90) : d.health > 25 ? ge::pack_rgb(240, 200, 60) : ge::pack_rgb(240, 60, 50);
        std::snprintf(buf, sizeof buf, "HEALTH %d", d.health);
        ge::draw_text_shadowed(fb, 4, h - 34, buf, colour);
        fill_rect(fb, 4, h - 24, 4 + bar_w, h - 20, ge::pack_rgb(40, 40, 40));
        fill_rect(fb, 4, h - 24, 4 + filled, h - 20, colour);
    }
    if (d.keys > 0) {
        std::snprintf(buf, sizeof buf, "KEYS %d", d.keys);
        ge::draw_text_shadowed(fb, (w - ge::text_width(buf)) / 2, 4, buf, ge::pack_rgb(110, 180, 255));
    }

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

    // Event feed, newest at the bottom, right-aligned above the ammo line.
    int fy = h - 12 - 10 * static_cast<int>(d.feed.size());
    for (const std::string& line : d.feed) {
        ge::draw_text_shadowed(fb, w - ge::text_width(line) - 4, fy, line, ge::pack_rgb(190, 230, 255));
        fy += 10;
    }

    if (d.debug) {
        std::snprintf(buf, sizeof buf, "FPS %.0f  %.1f MS", d.fps, d.frame_ms);
        ge::draw_text_shadowed(fb, w - ge::text_width(buf) - 4, 16, buf, ge::pack_rgb(120, 255, 120));
        std::snprintf(buf, sizeof buf, "ENTITIES %zu", d.entities);
        ge::draw_text_shadowed(fb, w - ge::text_width(buf) - 4, 26, buf, ge::pack_rgb(120, 255, 120));
        std::snprintf(buf, sizeof buf, "FOG: %s", d.backend);
        ge::draw_text_shadowed(fb, w - ge::text_width(buf) - 4, 36, buf, ge::pack_rgb(120, 255, 120));
    }
}

namespace {

void dim(ge::Framebuffer& fb, int percent) {
    for (std::uint32_t& px : fb.pixels())
        px = ge::pack_rgb(ge::red_of(px) * static_cast<unsigned>(percent) / 100, ge::green_of(px) * static_cast<unsigned>(percent) / 100,
                          ge::blue_of(px) * static_cast<unsigned>(percent) / 100);
}

void centered(ge::Framebuffer& fb, int y, const std::string& text, std::uint32_t color, int scale = 1) {
    ge::draw_text_shadowed(fb, (fb.width() - ge::text_width(text, scale)) / 2, y, text, color, scale);
}

}  // namespace

void draw_menu_screen(ge::Framebuffer& fb, const HudData& d) {
    dim(fb, 45);
    const int h = fb.height();
    centered(fb, h / 5, "GAME ENGINE", ge::pack_rgb(250, 220, 90), 3);
    centered(fb, h / 5 + 32, "TARGET RANGE", ge::pack_rgb(235, 235, 235), 2);
    centered(fb, h / 2 - 4, std::string("LEVEL: ") + d.level_name, ge::pack_rgb(190, 230, 255));
    char buf[48];
    std::snprintf(buf, sizeof buf, "CLEAR %d WAVES TO WIN", d.waves_to_win);
    centered(fb, h / 2 + 8, buf, ge::pack_rgb(190, 230, 255));
    centered(fb, h * 3 / 4 - 6, "PRESS ENTER OR CLICK TO START", ge::pack_rgb(120, 255, 120));
    centered(fb, h * 3 / 4 + 6, "Q QUIT", ge::pack_rgb(200, 200, 200));
}

void draw_pause_screen(ge::Framebuffer& fb) {
    dim(fb, 40);
    const int h = fb.height();
    centered(fb, h / 3, "PAUSED", ge::pack_rgb(250, 220, 90), 3);
    centered(fb, h / 2 + 8, "ESC OR ENTER RESUME", ge::pack_rgb(235, 235, 235));
    centered(fb, h / 2 + 20, "Q QUIT", ge::pack_rgb(200, 200, 200));
}

void draw_victory_screen(ge::Framebuffer& fb, const HudData& d) {
    dim(fb, 40);
    const int h = fb.height();
    centered(fb, h / 5, "YOU WIN!", ge::pack_rgb(250, 220, 90), 3);
    char buf[64];
    std::snprintf(buf, sizeof buf, "SCORE %05d", d.score);
    centered(fb, h / 2 - 14, buf, ge::pack_rgb(235, 235, 235), 2);
    const unsigned accuracy = d.shots == 0 ? 0 : (d.hits * 100u) / d.shots;
    std::snprintf(buf, sizeof buf, "ACCURACY %u%%  %u OF %u SHOTS", accuracy, d.hits, d.shots);
    centered(fb, h / 2 + 8, buf, ge::pack_rgb(190, 230, 255));
    centered(fb, h * 3 / 4 - 6, "ENTER PLAY AGAIN", ge::pack_rgb(120, 255, 120));
    centered(fb, h * 3 / 4 + 6, "Q QUIT", ge::pack_rgb(200, 200, 200));
}

void draw_gameover_screen(ge::Framebuffer& fb, const HudData& d) {
    dim(fb, 35);
    const int h = fb.height();
    centered(fb, h / 5, "YOU DIED", ge::pack_rgb(255, 70, 60), 3);
    char buf[64];
    std::snprintf(buf, sizeof buf, "SCORE %05d  WAVE %d", d.score, d.wave);
    centered(fb, h / 2 - 6, buf, ge::pack_rgb(235, 235, 235), 1);
    centered(fb, h * 3 / 4 - 6, "ENTER TRY AGAIN", ge::pack_rgb(120, 255, 120));
    centered(fb, h * 3 / 4 + 6, "Q QUIT", ge::pack_rgb(200, 200, 200));
}

void tint_red(ge::Framebuffer& fb, double amount) {
    const unsigned add = static_cast<unsigned>(std::clamp(amount, 0.0, 1.0) * 110.0);
    for (std::uint32_t& px : fb.pixels())
        px = ge::pack_rgb(std::min(255u, ge::red_of(px) + add), ge::green_of(px) * (200u - add / 2) / 200u, ge::blue_of(px) * (200u - add / 2) / 200u);
}

}  // namespace game
