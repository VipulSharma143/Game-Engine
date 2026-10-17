#pragma once

#include <vector>

#include "engine/renderer/Camera.hpp"
#include "engine/renderer/Framebuffer.hpp"
#include "engine/world/World.hpp"

namespace ge {

// CPU raycasting renderer. Owns a per-column depth buffer that is sized once
// and reused; render() performs no heap allocation once the size is stable.
class Renderer {
public:
    void render(const GridMap& map, const Camera& camera, Framebuffer& target);

    // Perpendicular wall distance per column from the last render (infinity if no hit).
    const std::vector<double>& depth() const noexcept { return depth_; }

private:
    std::vector<double> depth_;
};

}  // namespace ge
