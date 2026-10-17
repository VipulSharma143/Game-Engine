#include "engine/renderer/Camera.hpp"

namespace ge {

Camera Camera::from_angle(Vec2 position, double angle_radians, double fov_degrees) noexcept {
    Camera cam;
    cam.position = position;
    cam.direction = heading_vector(angle_radians);
    cam.plane = right_of(cam.direction) * std::tan(deg_to_rad(fov_degrees) * 0.5);
    return cam;
}

}  // namespace ge
