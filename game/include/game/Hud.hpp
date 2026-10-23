#pragma once
// 2D overlay drawn on top of the 3D view: crosshair, counters, banner, weapon, debug text.

#include <cstddef>
#include <string>
#include <vector>

#include "engine/renderer/Framebuffer.hpp"

namespace game {

struct HudData {
    int ammo = 0;
    int magazine = 0;
    bool reloading = false;
    double reload_fraction = 0.0;  // 0..1 while reloading
    double weapon_flash = 0.0;     // 0..1, recoil and muzzle flash
    double hit_marker = 0.0;       // 0..1, > 0 shows a red hit marker
    int score = 0;
    int health = 100;
    int max_health = 100;
    int keys = 0;
    int wave = 1;
    int waves_to_win = 3;
    unsigned shots = 0;
    unsigned hits = 0;
    const char* level_name = "";
    std::vector<std::string> feed;  // recent event messages, oldest first
    int targets_left = 0;
    bool between_waves = false;
    bool debug = false;
    double fps = 0.0;
    double frame_ms = 0.0;
    std::size_t entities = 0;
    const char* backend = "";
};

void draw_weapon_view(ge::Framebuffer& fb, double flash, bool reloading);
void draw_hud(ge::Framebuffer& fb, const HudData& hud);

// Full-screen overlays: they darken the scene behind them.
void draw_menu_screen(ge::Framebuffer& fb, const HudData& hud);
void draw_pause_screen(ge::Framebuffer& fb);
void draw_victory_screen(ge::Framebuffer& fb, const HudData& hud);
void draw_gameover_screen(ge::Framebuffer& fb, const HudData& hud);

// Red flash over the whole view when the player is hit (amount 0..1).
void tint_red(ge::Framebuffer& fb, double amount);

}  // namespace game
