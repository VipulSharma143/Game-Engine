#pragma once
// Billboard sprites for the raycaster: projected with the camera, depth-tested per column
// against the wall depth buffer from Renderer::depth(), drawn far to near.

#include <cstdint>
#include <span>
#include <vector>

#include "engine/core/Math.hpp"
#include "engine/renderer/Camera.hpp"
#include "engine/renderer/Framebuffer.hpp"
#include "engine/renderer/SpriteImage.hpp"

namespace ge {

struct Sprite {
    Vec2 position{};
    double size = 0.5;          // world-space diameter (1.0 = full wall height)
    std::uint32_t color = 0;    // base 0xAARRGGBB
    double flash = 0.0;         // 0..1 blend towards white (hit feedback)
    bool diamond = false;       // draw a flat-shaded diamond (items) instead of a shaded sphere
    // Optional extras (the defaults draw exactly what plain sprites always drew):
    const SpriteImage* image = nullptr;  // draw this picture instead of a sphere or diamond; `size` is then its height
                                         // in wall heights and it stands on the floor
    double z = 0.0;             // height above where the sprite would sit, in wall heights (sparks fly up, then fall)
    double light = 1.0;         // brightness multiplier (below 1 dims the sprite)
};

// Camera-space projection of a point: depth along the view direction, and the screen column.
struct Projection {
    bool visible = false;  // in front of the camera
    double depth = 0.0;
    double screen_x = 0.0;
};
Projection project_point(const Camera& camera, Vec2 world, int screen_width) noexcept;

// Draws shaded round sprites. `depth` holds one wall distance per screen column.
// Returns how many sprites had at least one visible column.
int draw_sprites(const Camera& camera, std::span<const double> depth, std::span<const Sprite> sprites,
                 Framebuffer& target);

}  // namespace ge
