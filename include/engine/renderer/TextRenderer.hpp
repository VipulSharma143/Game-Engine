#pragma once
// Built-in 5x7 bitmap font (A-Z, 0-9 and a few symbols; lower case is drawn as upper case).

#include <string_view>

#include "engine/renderer/Framebuffer.hpp"

namespace ge {

constexpr int kGlyphWidth = 5;
constexpr int kGlyphHeight = 7;
constexpr int kGlyphAdvance = 6;  // glyph + one pixel of spacing

int text_width(std::string_view text, int scale = 1) noexcept;
// Draws `text` with its top-left corner at (x, y). Unknown characters are drawn as a box.
void draw_text(Framebuffer& target, int x, int y, std::string_view text, std::uint32_t color, int scale = 1);
// Same, with a 1-pixel (times scale) drop shadow for readability on any background.
void draw_text_shadowed(Framebuffer& target, int x, int y, std::string_view text, std::uint32_t color, int scale = 1);

}  // namespace ge
