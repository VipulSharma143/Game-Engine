#include "engine/renderer/Texture.hpp"

#include <algorithm>
#include <cstdlib>

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

// Door: vertical planks inside a dark frame with a handle; the locked variant is steel with a
// hazard stripe and a keyhole.
void make_door(Texture& t, bool locked, std::uint32_t seed) {
    const Rgb wood = locked ? Rgb{150, 150, 160} : Rgb{150, 100, 55};
    for (int y = 0; y < Texture::kSize; ++y)
        for (int x = 0; x < Texture::kSize; ++x) {
            const bool frame = x < 4 || x >= 60 || y < 4 || y >= 60;
            if (frame) { t.set(x, y, tint({60, 60, 62}, 100, noise_at(x, y, seed, 6))); continue; }
            const bool seam = locked ? ((y - 4) % 14) == 0 : ((x - 4) % 14) == 0;
            const bool stripe = locked && y >= 28 && y < 36 && ((x + y) / 6) % 2 == 0;
            const bool handle = !locked && x >= 48 && x < 52 && y >= 28 && y < 38;
            const bool keyhole = locked && ((x >= 30 && x < 34 && y >= 38 && y < 41) || (x >= 31 && x < 33 && y >= 41 && y < 47));
            if (handle) { t.set(x, y, pack_rgb(220, 200, 90)); continue; }
            if (keyhole) { t.set(x, y, pack_rgb(15, 15, 18)); continue; }
            if (stripe) { t.set(x, y, pack_rgb(210, 60, 50)); continue; }
            t.set(x, y, seam ? tint(wood, 55) : tint(wood, 100, noise_at(x, y, seed + 1, 12)));
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

// ---- extra walls (ids 12-17) ----

// Circuit board: dark base, bright traces with right-angle turns and solder pads.
void make_circuit(Texture& t, Rgb base, std::uint32_t seed) {
    for (int y = 0; y < Texture::kSize; ++y)
        for (int x = 0; x < Texture::kSize; ++x) {
            const int cx = x / 8, cy = y / 8;
            const std::uint32_t h = texture_hash(static_cast<std::uint32_t>(cx), static_cast<std::uint32_t>(cy), seed);
            const bool horizontal = (h & 1u) != 0 && (y % 8) == 3 + static_cast<int>((h >> 4) % 3u);
            const bool vertical = (h & 1u) == 0 && (x % 8) == 3 + static_cast<int>((h >> 4) % 3u);
            const bool pad = (h % 11u) == 0 && (x % 8) >= 2 && (x % 8) < 6 && (y % 8) >= 2 && (y % 8) < 6;
            if (pad) { t.set(x, y, pack_rgb(235, 220, 120)); continue; }
            if (horizontal || vertical) { t.set(x, y, tint(base, 150 + static_cast<int>(h % 40u))); continue; }
            const bool seam = (x % 32) == 0 || (y % 32) == 0;
            t.set(x, y, seam ? tint(base, 25) : tint(base, 38, noise_at(x, y, seed + 1, 6)));
        }
}

// Hazard stripes: diagonal yellow and black bands in a bevelled frame.
void make_hazard(Texture& t, std::uint32_t seed) {
    for (int y = 0; y < Texture::kSize; ++y)
        for (int x = 0; x < Texture::kSize; ++x) {
            const bool frame = x < 3 || y < 3 || x >= 61 || y >= 61;
            const bool yellow = ((x + y) / 10) % 2 == 0;
            const int pct = frame ? ((x < 3 || y < 3) ? 125 : 65) : 100;
            const Rgb c = frame ? Rgb{110, 110, 115} : (yellow ? Rgb{235, 195, 25} : Rgb{35, 35, 38});
            t.set(x, y, tint(c, pct, noise_at(x, y, seed, 8)));
        }
}

// Mossy stone: blocks with patches of green growing from the seams.
void make_moss(Texture& t, Rgb base, std::uint32_t seed) {
    make_blocks(t, base, seed);
    for (int y = 0; y < Texture::kSize; ++y)
        for (int x = 0; x < Texture::kSize; ++x) {
            const int patch = noise_at(x / 3, y / 3, seed + 9, 100) + (((y % 32) < 10 || (x % 32) < 6) ? 40 : -20);
            if (patch > 45) t.set(x, y, tint({60, 120 + noise_at(x, y, seed + 3, 40), 50}, 100 - (patch > 80 ? 20 : 0)));
        }
}

// Rusty steel plates: riveted panels covered in orange-brown corrosion.
void make_rust(Texture& t, Rgb base, std::uint32_t seed) {
    make_panel(t, base, seed);
    for (int y = 0; y < Texture::kSize; ++y)
        for (int x = 0; x < Texture::kSize; ++x) {
            const int r = noise_at(x / 2, y / 2, seed + 7, 100) + noise_at(x / 5, y / 4, seed + 8, 80);
            if (r > 40) t.set(x, y, tint({170 + noise_at(x, y, seed, 30), 85, 40}, 100 - (r > 100 ? 25 : 0)));
        }
}

// Lava rock: near-black stone with glowing cracks.
void make_lava(Texture& t, std::uint32_t seed) {
    for (int y = 0; y < Texture::kSize; ++y)
        for (int x = 0; x < Texture::kSize; ++x) {
            const int a = noise_at(x / 4, y / 4, seed, 100), b = noise_at((x + 3) / 4, (y + 5) / 4, seed + 1, 100);
            const int crack = std::abs(a - b);
            if (crack < 14) { t.set(x, y, pack_rgb(static_cast<unsigned>(clamp8(255 - crack * 4)), static_cast<unsigned>(clamp8(120 - crack * 4 + noise_at(x, y, seed, 30))), 20)); continue; }
            t.set(x, y, tint({60, 50, 52}, 100, noise_at(x, y, seed + 2, 14)));
        }
}

// Glass tiles: pale blue squares with a bright corner highlight and dark grout.
void make_glass(Texture& t, Rgb base, std::uint32_t seed) {
    for (int y = 0; y < Texture::kSize; ++y)
        for (int x = 0; x < Texture::kSize; ++x) {
            const int lx = x % 16, ly = y % 16;
            if (lx == 0 || ly == 0) { t.set(x, y, tint(base, 35)); continue; }
            const bool glint = (lx + ly) >= 3 && (lx + ly) < 6;
            const int shade = 70 + (lx + ly) * 2 + static_cast<int>(texture_hash(static_cast<std::uint32_t>(x / 16), static_cast<std::uint32_t>(y / 16), seed) % 18u);
            t.set(x, y, glint ? pack_rgb(235, 250, 255) : tint(base, shade, noise_at(x, y, seed + 1, 5)));
        }
}

// ---- themed floors and ceilings ----

void make_grate(Texture& t, Rgb base, std::uint32_t seed) {  // metal floor grating
    for (int y = 0; y < Texture::kSize; ++y)
        for (int x = 0; x < Texture::kSize; ++x) {
            const bool bar = (x % 8) < 2 || (y % 32) < 2;
            const bool hole = !bar && (x % 8) >= 3 && (x % 8) < 7 && (y % 8) >= 2 && (y % 8) < 6;
            t.set(x, y, hole ? tint(base, 22) : tint(base, bar ? 100 : 70, noise_at(x, y, seed, 10)));
        }
}

void make_rock(Texture& t, Rgb base, std::uint32_t seed) {  // rough cave rock, floor or ceiling
    for (int y = 0; y < Texture::kSize; ++y)
        for (int x = 0; x < Texture::kSize; ++x)
            t.set(x, y, tint(base, 85 + noise_at(x / 4, y / 4, seed, 30), noise_at(x, y, seed + 1, 14)));
}

void make_tiles(Texture& t, Rgb base, std::uint32_t seed) {  // clean lab floor, small square tiles
    for (int y = 0; y < Texture::kSize; ++y)
        for (int x = 0; x < Texture::kSize; ++x) {
            const bool grout = (x % 16) == 0 || (y % 16) == 0;
            const int variation = static_cast<int>(texture_hash(static_cast<std::uint32_t>(x / 16), static_cast<std::uint32_t>(y / 16), seed) % 14u);
            t.set(x, y, grout ? tint(base, 55) : tint(base, 92 + variation, noise_at(x, y, seed + 1, 4)));
        }
}

void make_pipes_ceiling(Texture& t, Rgb base, std::uint32_t seed) {  // dark ceiling with pipes and amber lamps
    for (int y = 0; y < Texture::kSize; ++y)
        for (int x = 0; x < Texture::kSize; ++x) {
            const bool pipe = (y % 32) >= 4 && (y % 32) < 10;
            const bool lamp = (x % 32) >= 12 && (x % 32) < 20 && (y % 32) >= 20 && (y % 32) < 26;
            if (lamp) { t.set(x, y, pack_rgb(255, 190, 90)); continue; }
            if (pipe) { t.set(x, y, tint({110, 85, 70}, 90 + ((y % 32) - 4) * 4, noise_at(x, y, seed, 8))); continue; }
            t.set(x, y, tint(base, 100, noise_at(x, y, seed + 1, 7)));
        }
}

}  // namespace

TextureSet TextureSet::make_procedural(std::string_view theme) {
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
    make_door(s.walls_[10], false, 10);
    make_door(s.walls_[11], true, 11);
    make_circuit(s.walls_[12], {50, 190, 150}, 12);
    make_hazard(s.walls_[13], 13);
    make_moss(s.walls_[14], {120, 120, 112}, 14);
    make_rust(s.walls_[15], {120, 125, 135}, 15);
    make_lava(s.walls_[16], 16);
    make_glass(s.walls_[17], {130, 200, 240}, 17);
    if (theme == "foundry") {
        make_grate(s.floors_[0], {105, 95, 90}, 22);
        make_grate(s.floors_[1], {80, 72, 68}, 23);
        make_pipes_ceiling(s.ceiling_, {48, 40, 40}, 31);
    } else if (theme == "cavern") {
        make_rock(s.floors_[0], {95, 85, 75}, 24);
        make_rock(s.floors_[1], {75, 68, 60}, 25);
        make_rock(s.ceiling_, {48, 44, 46}, 32);
    } else if (theme == "lab") {
        make_tiles(s.floors_[0], {205, 210, 215}, 26);
        make_tiles(s.floors_[1], {170, 178, 188}, 27);
        make_ceiling(s.ceiling_, {150, 160, 175}, 33);
    } else {
        make_floor(s.floors_[0], {105, 100, 95}, 20);
        make_floor(s.floors_[1], {80, 78, 76}, 21);
        make_ceiling(s.ceiling_, {55, 62, 88}, 30);
    }
    return s;
}

}  // namespace ge
