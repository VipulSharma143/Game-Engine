#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace ge {

// Pixels are 0xAARRGGBB.
constexpr std::uint32_t pack_rgb(unsigned r, unsigned g, unsigned b) noexcept {
    return 0xFF000000u | ((r & 0xFFu) << 16) | ((g & 0xFFu) << 8) | (b & 0xFFu);
}
constexpr unsigned red_of(std::uint32_t c) noexcept { return (c >> 16) & 0xFFu; }
constexpr unsigned green_of(std::uint32_t c) noexcept { return (c >> 8) & 0xFFu; }
constexpr unsigned blue_of(std::uint32_t c) noexcept { return c & 0xFFu; }

// Owns one row-major pixel buffer. Allocates only in the constructor / resize().
class Framebuffer {
public:
    Framebuffer() = default;
    Framebuffer(int width, int height) { resize(width, height); }

    void resize(int width, int height);

    int width() const noexcept { return width_; }
    int height() const noexcept { return height_; }
    std::span<std::uint32_t> pixels() noexcept { return pixels_; }
    std::span<const std::uint32_t> pixels() const noexcept { return pixels_; }

    void clear(std::uint32_t color) noexcept;
    // Out-of-range coordinates are ignored / return 0.
    void set_pixel(int x, int y, std::uint32_t color) noexcept;
    std::uint32_t pixel(int x, int y) const noexcept;
    // Fills rows [y0, y1) of column x, clamped to the buffer.
    void fill_column(int x, int y0, int y1, std::uint32_t color) noexcept;

    // FNV-1a over all pixels; used to compare frames for determinism.
    std::uint64_t checksum() const noexcept;

private:
    int width_ = 0;
    int height_ = 0;
    std::vector<std::uint32_t> pixels_;
};

}  // namespace ge
