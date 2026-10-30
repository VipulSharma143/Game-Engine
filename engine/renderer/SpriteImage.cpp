#include "engine/renderer/SpriteImage.hpp"

namespace ge {

SpriteImage SpriteImage::from_ascii(std::span<const std::string_view> rows, std::span<const std::pair<char, std::uint32_t>> palette) {
    SpriteImage image;
    if (rows.empty() || rows[0].empty()) return image;
    for (std::string_view row : rows)
        if (row.size() != rows[0].size()) return image;
    image.width = static_cast<int>(rows[0].size());
    image.height = static_cast<int>(rows.size());
    image.pixels.assign(static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height), 0u);
    for (int y = 0; y < image.height; ++y)
        for (int x = 0; x < image.width; ++x) {
            const char c = rows[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
            for (const auto& [key, color] : palette)
                if (key == c) { image.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) + static_cast<std::size_t>(x)] = color; break; }
        }
    return image;
}

SpriteImage SpriteImage::mirrored() const {
    SpriteImage out = *this;
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            out.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)] = at(width - 1 - x, y);
    return out;
}

}  // namespace ge
