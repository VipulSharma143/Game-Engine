#include "engine/renderer/Texture.hpp"

#include <algorithm>

namespace ge {

std::uint32_t texture_hash(std::uint32_t x, std::uint32_t y, std::uint32_t seed) noexcept {
    std::uint32_t h = x * 0x8DA6B343u ^ y * 0xD8163841u ^ seed * 0xCB1AB31Fu;
    h ^= h >> 15; h *= 0x2C1B3C6Du;
    h ^= h >> 12; h *= 0x297A2D39u;
    h ^= h >> 15;
    return h;
}

namespace {

struct Rgb { int r, g, b; };

constexpr int clamp8(int v) noexcept { return v < 0 ? 0 : (v > 255 ? 255 : v); }

// Multiplies by `percent` / 100 and adds a signed per-pixel noise term.
std::uint32_t tint(Rgb c, int percent, int noise = 0) noexcept {
    return pack_rgb(static_cast<unsigned>(clamp8(c.r * percent / 100 + noise)),
                    static_cast<unsigned>(clamp8(c.g * percent / 100 + noise)),
                    static_cast<unsigned>(clamp8(c.b * percent / 100 + noise)));
}

int noise_at(int x, int y, std::uint32_t seed, int amplitude) noexcept {
    const int n = static_cast<int>(texture_hash(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), seed) & 0xFFu);
    return (n - 128) * amplitude / 128;
}

// Staggered bricks: 16 px tall rows, 32 px wide bricks, 2 px mortar.
void make_brick(Texture& t, Rgb base, std::uint32_t seed) {
    for (int y = 0; y < Texture::kSize; ++y) {
        const int row = y / 16;
        const int shift = (row & 1) ? 16 : 0;
        for (int x = 0; x < Texture::kSize; ++x) {
            const int bx = (x + shift) & (Texture::kSize - 1);
            const bool mortar = (y % 16) < 2 || (bx % 32) < 2;
            if (mortar) {
                t.set(x, y, tint({150, 150, 145}, 70, noise_at(x, y, seed, 10)));
                continue;
            }
            const int brick_tint = 85 + static_cast<int>(texture_hash(static_cast<std::uint32_t>((x + shift) / 32),
                                                                       static_cast<std::uint32_t>(row), seed) % 31u);
            const int edge = ((y % 16) == 2 || (bx % 32) == 2) ? 112 : 100;  // lit top/left edge
            t.set(x, y, tint(base, brick_tint * edge / 100, noise_at(x, y, seed + 1, 14)));
        }
    }
}

// Large bevelled blocks (stone / marble).
void make_blocks(Texture& t, Rgb base, std::uint32_t seed) {
    for (int y = 0; y < Texture::kSize; ++y)
        for (int x = 0; x < Texture::kSize; ++x) {
            const int lx = x % 32, ly = y % 32;
            int pct = 100;
            if (lx == 0 || ly == 0) pct = 55;                   // seam
            else if (lx == 1 || ly == 1) pct = 118;             // lit bevel
            else if (lx == 31 || ly == 31) pct = 80;            // shadowed bevel
            const int block = static_cast<int>(texture_hash(static_cast<std::uint32_t>(x / 32), static_cast<std::uint32_t>(y / 32), seed) % 17u);
            t.set(x, y, tint(base, pct * (92 + block) / 100, noise_at(x, y, seed + 2, 12)));
        }
}

// Riveted metal panels with an accent stripe.
void make_panel(Texture& t, Rgb base, std::uint32_t seed) {
    for (int y = 0; y < Texture::kSize; ++y)
        for (int x = 0; x < Texture::kSize; ++x) {
            const int lx = x % 32, ly = y % 32;
            int pct = 100;
            if (lx < 2 || ly < 2) pct = 60;
            else if (lx > 29 || ly > 29) pct = 130;
            const bool rx = (lx >= 4 && lx < 6) || (lx >= 26 && lx < 28);
            const bool ry = (ly >= 4 && ly < 6) || (ly >= 26 && ly < 28);
            const bool rivet = rx && ry;
            const bool stripe = ly >= 14 && ly < 18;
            if (rivet) { t.set(x, y, tint({230, 230, 230}, 100)); continue; }
            if (stripe) { t.set(x, y, tint({base.b, base.r, base.g}, 85, noise_at(x, y, seed, 6))); continue; }
            t.set(x, y, tint(base, pct, noise_at(x, y, seed + 3, 8)));
        }
}

// Vertical planks with grain.
void make_planks(Texture& t, Rgb base, std::uint32_t seed) {
    for (int y = 0; y < Texture::kSize; ++y)
        for (int x = 0; x < Texture::kSize; ++x) {
            const int plank = x / 16;
            const bool seam = (x % 16) == 0;
            const int plank_tint = 88 + static_cast<int>(texture_hash(static_cast<std::uint32_t>(plank), 0, seed) % 25u);
            const int grain = noise_at(x / 2, y / 5 + plank * 7, seed + 4, 14);
            t.set(x, y, seam ? tint(base, 45) : tint(base, plank_tint, grain + noise_at(x, y, seed + 5, 5)));
        }
}

void make_floor(Texture& t, Rgb base, std::uint32_t seed) {
    for (int y = 0; y < Texture::kSize; ++y)
        for (int x = 0; x < Texture::kSize; ++x) {
            const int lx = x % 32, ly = y % 32;
            const bool grout = lx == 0 || ly == 0;
            t.set(x, y, grout ? tint(base, 55) : tint(base, 100, noise_at(x, y, seed, 12)));
        }
}

void make_ceiling(Texture& t, Rgb base, std::uint32_t seed) {
    for (int y = 0; y < Texture::kSize; ++y)
        for (int x = 0; x < Texture::kSize; ++x) {
            const bool grid = (x % 16) < 1 || (y % 16) < 1;
            const bool light = (x % 32) >= 12 && (x % 32) < 20 && (y % 32) >= 12 && (y % 32) < 20;
            if (light) { t.set(x, y, tint({200, 205, 170}, 100)); continue; }
            t.set(x, y, grid ? tint(base, 60) : tint(base, 100, noise_at(x, y, seed, 6)));
        }
}

}  // namespace

TextureSet TextureSet::make_procedural() {
    TextureSet s;
    // Wall id -> pattern and colour (colours follow the flat-shaded palette of the old renderer).
    for (int y = 0; y < Texture::kSize; ++y)  // missing-texture marker: magenta/black checker
        for (int x = 0; x < Texture::kSize; ++x)
            s.walls_[0].set(x, y, (((x / 8) + (y / 8)) & 1) ? pack_rgb(255, 0, 255) : pack_rgb(0, 0, 0));
    make_brick(s.walls_[1], {200, 60, 60}, 1);
    make_blocks(s.walls_[2], {70, 170, 80}, 2);
    make_panel(s.walls_[3], {70, 95, 210}, 3);
    make_blocks(s.walls_[4], {215, 190, 90}, 4);
    make_panel(s.walls_[5], {70, 185, 190}, 5);
    make_brick(s.walls_[6], {170, 80, 190}, 6);
    make_blocks(s.walls_[7], {225, 225, 230}, 7);
    make_panel(s.walls_[8], {225, 140, 55}, 8);
    make_planks(s.walls_[9], {150, 100, 55}, 9);
    make_floor(s.floors_[0], {105, 100, 95}, 20);
    make_floor(s.floors_[1], {80, 78, 76}, 21);
    make_ceiling(s.ceiling_, {55, 62, 88}, 30);
    return s;
}

}  // namespace ge
