#pragma once
// Square power-of-two RGBA textures and the built-in procedural texture set.
// Everything is generated from integer hashes at startup, so the engine needs no asset files
// and the output is identical on every platform.

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

#include "engine/renderer/Framebuffer.hpp"

namespace ge {

class Texture {
public:
    static constexpr int kSize = 64;  // width = height, must stay a power of two

    Texture() : pixels_(static_cast<std::size_t>(kSize) * kSize, 0xFF000000u) {}

    // Coordinates wrap in both directions.
    std::uint32_t at(int x, int y) const noexcept {
        return pixels_[static_cast<std::size_t>((y & (kSize - 1)) * kSize + (x & (kSize - 1)))];
    }
    void set(int x, int y, std::uint32_t c) noexcept {
        pixels_[static_cast<std::size_t>((y & (kSize - 1)) * kSize + (x & (kSize - 1)))] = c;
    }
    const std::vector<std::uint32_t>& pixels() const noexcept { return pixels_; }

private:
    std::vector<std::uint32_t> pixels_;
};

// Wall textures indexed by wall id (1-9 walls, 10 door, 11 locked door, 12-17 extra walls), two floor variants (laid
// out as a checkerboard) and one ceiling texture. `theme` picks the floor and ceiling ("default", "foundry", "cavern"
// or "lab"; an unknown name gives the default); the walls are the same in every theme.
class TextureSet {
public:
    static TextureSet make_procedural(std::string_view theme = "default");

    const Texture& wall(std::uint8_t id) const noexcept { return walls_[id < walls_.size() ? id : 0]; }
    const Texture& floor(int variant) const noexcept { return floors_[static_cast<std::size_t>(variant & 1)]; }
    const Texture& ceiling() const noexcept { return ceiling_; }

private:
    std::array<Texture, 18> walls_;  // index 0 is a magenta "missing" texture; 10 = door, 11 = locked door
    std::array<Texture, 2> floors_;
    Texture ceiling_;
};

// Deterministic integer hash used by the generators (exposed for tests).
std::uint32_t texture_hash(std::uint32_t x, std::uint32_t y, std::uint32_t seed) noexcept;

}  // namespace ge
