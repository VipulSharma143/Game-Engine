#include "engine/renderer/Framebuffer.hpp"

#include <algorithm>

#include "engine/optimized/OptimizedRoutines.hpp"
#include "engine/optimized/ScalarFallback.hpp"

namespace ge {

void Framebuffer::resize(int width, int height) {
    width_ = std::max(width, 0);
    height_ = std::max(height, 0);
    pixels_.assign(static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_), 0xFF000000u);
}

void Framebuffer::clear(std::uint32_t color) noexcept { opt::fill_u32(pixels_.data(), color, pixels_.size()); }

void Framebuffer::set_pixel(int x, int y, std::uint32_t color) noexcept {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) return;
    pixels_[static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(x)] = color;
}

std::uint32_t Framebuffer::pixel(int x, int y) const noexcept {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) return 0;
    return pixels_[static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(x)];
}

void Framebuffer::fill_column(int x, int y0, int y1, std::uint32_t color) noexcept {
    if (x < 0 || x >= width_) return;
    y0 = std::max(y0, 0);
    y1 = std::min(y1, height_);
    if (y1 <= y0) return;
    std::uint32_t* first = pixels_.data() + static_cast<std::size_t>(y0) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(x);
    opt::fill_column_u32(first, static_cast<std::size_t>(width_), color, static_cast<std::size_t>(y1 - y0));
}

std::uint64_t Framebuffer::checksum() const noexcept {
    return opt::fnv1a64_u32(pixels_.data(), pixels_.size(), scalar::kFnvOffsetBasis);
}

}  // namespace ge
