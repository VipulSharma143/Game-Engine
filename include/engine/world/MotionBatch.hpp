#pragma once
// Structure-of-arrays batch of moving points. The per-frame position update runs through
// ge::opt::vec2_madd (NASM when available, C++ otherwise); results are identical either way.

#include <cstddef>
#include <vector>

#include "engine/core/Math.hpp"
#include "engine/world/World.hpp"

namespace ge {

class MotionBatch {
public:
    // Returns the index of the new point.
    std::size_t add(Vec2 position, Vec2 velocity);
    // Removes point `i` by moving the last point into its slot (indices of the last point change).
    void remove(std::size_t i);
    std::size_t size() const noexcept { return pos_.size() / 2; }

    Vec2 position(std::size_t i) const noexcept { return {pos_[2 * i], pos_[2 * i + 1]}; }
    Vec2 velocity(std::size_t i) const noexcept { return {vel_[2 * i], vel_[2 * i + 1]}; }

    // Advance every point by dt seconds. Points that would enter a solid cell stay put on
    // the blocked axis and have that velocity component reversed (bounce).
    void step(double dt, const GridMap& map);

    // Order-sensitive hash of all positions and velocities (determinism checks).
    std::uint64_t checksum() const noexcept;

private:
    std::vector<double> pos_;  // x0,y0,x1,y1,...
    std::vector<double> vel_;
    std::vector<double> prev_;
};

}  // namespace ge
