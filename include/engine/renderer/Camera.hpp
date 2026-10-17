#pragma once

#include "engine/core/Math.hpp"

namespace ge {

// Raycaster camera: `direction` is a unit vector, `plane` is the right-hand
// camera plane with length tan(fov / 2).
struct Camera {
    Vec2 position{0.0, 0.0};
    Vec2 direction{1.0, 0.0};
    Vec2 plane{0.0, 0.66};

    static Camera from_angle(Vec2 position, double angle_radians, double fov_degrees) noexcept;
};

}  // namespace ge
