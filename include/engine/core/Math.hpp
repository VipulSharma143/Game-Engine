#pragma once

#include <cmath>

namespace ge {

inline constexpr double kPi = 3.14159265358979323846;

struct Vec2 {
    double x = 0.0;
    double y = 0.0;
};

constexpr Vec2 operator+(Vec2 a, Vec2 b) noexcept { return {a.x + b.x, a.y + b.y}; }
constexpr Vec2 operator-(Vec2 a, Vec2 b) noexcept { return {a.x - b.x, a.y - b.y}; }
constexpr Vec2 operator*(Vec2 v, double s) noexcept { return {v.x * s, v.y * s}; }

constexpr double deg_to_rad(double degrees) noexcept { return degrees * kPi / 180.0; }

constexpr double clamp(double v, double lo, double hi) noexcept {
    return v < lo ? lo : (v > hi ? hi : v);
}

// Unit forward vector for a heading. Map space is y-down: angle 0 faces +x,
// increasing angles turn clockwise as seen on a top-down map.
inline Vec2 heading_vector(double angle) noexcept { return {std::cos(angle), std::sin(angle)}; }

// Right-hand vector of a forward vector in y-down map space.
constexpr Vec2 right_of(Vec2 forward) noexcept { return {-forward.y, forward.x}; }

}  // namespace ge
