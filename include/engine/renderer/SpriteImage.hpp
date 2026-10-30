#pragma once
// A small RGBA picture drawn as a billboard (enemies, items). Built from rows of text so pixel art lives in the
// source: no image files, identical on every platform.

#include <cstdint>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace ge {

struct SpriteImage {
    int width = 0;
    int height = 0;
    std::vector<std::uint32_t> pixels;  // 0xAARRGGBB, row-major, alpha below 128 is transparent

    // One character per pixel; characters not in `palette` (and '.' or ' ') are transparent. All rows must have
    // the same length, otherwise the result is empty (width 0).
    static SpriteImage from_ascii(std::span<const std::string_view> rows,
                                  std::span<const std::pair<char, std::uint32_t>> palette);

    bool empty() const noexcept { return width <= 0 || height <= 0; }
    std::uint32_t at(int x, int y) const noexcept {
        if (x < 0 || y < 0 || x >= width || y >= height) return 0;
        return pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)];
    }
    SpriteImage mirrored() const;  // left-right flipped copy
};

}  // namespace ge
