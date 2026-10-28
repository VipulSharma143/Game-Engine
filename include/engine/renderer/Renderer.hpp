#pragma once

#include <vector>

#include "engine/renderer/Camera.hpp"
#include "engine/renderer/Framebuffer.hpp"
#include "engine/renderer/Texture.hpp"
#include "engine/world/World.hpp"

namespace ge {

// CPU raycasting renderer. Owns a per-column depth buffer that is sized once
// and reused; render() performs no heap allocation once the size is stable.
class Renderer {
public:
    // With a texture set, walls, floor and ceiling are textured and fade into fog with
    // distance. Without one (the default), the flat-shaded renderer is used.
    void set_textures(const TextureSet* textures) noexcept { textures_ = textures; }
    bool textured() const noexcept { return textures_ != nullptr; }

    // Tints the wall faces of one map cell (the editor's selection). Pass a cell outside the map to clear it.
    void set_highlight_cell(int x, int y) noexcept { highlight_x_ = x; highlight_y_ = y; }
    void clear_highlight() noexcept { highlight_x_ = highlight_y_ = -1; }

    void render(const GridMap& map, const Camera& camera, Framebuffer& target);

    // Perpendicular wall distance per column from the last render (infinity if no hit).
    const std::vector<double>& depth() const noexcept { return depth_; }

private:
    void render_textured(const GridMap& map, const Camera& camera, Framebuffer& target);
    void render_flat(const GridMap& map, const Camera& camera, Framebuffer& target);

    std::vector<double> depth_;
    std::vector<std::uint32_t> column_;  // scratch for one wall column
    const TextureSet* textures_ = nullptr;
    int highlight_x_ = -1;
    int highlight_y_ = -1;
};

}  // namespace ge
