#include "engine/world/MotionBatch.hpp"

#include <cmath>
#include <cstring>

#include "engine/optimized/OptimizedRoutines.hpp"

namespace ge {

namespace {
bool solid_at(const GridMap& map, double x, double y) noexcept {
    return map.is_solid(static_cast<int>(std::floor(x)), static_cast<int>(std::floor(y)));
}
}  // namespace

std::size_t MotionBatch::add(Vec2 position, Vec2 velocity) {
    pos_.push_back(position.x);
    pos_.push_back(position.y);
    vel_.push_back(velocity.x);
    vel_.push_back(velocity.y);
    return size() - 1;
}

void MotionBatch::remove(std::size_t i) {
    if (i >= size()) return;
    const std::size_t last = size() - 1;
    if (i != last) {
        pos_[2 * i] = pos_[2 * last];
        pos_[2 * i + 1] = pos_[2 * last + 1];
        vel_[2 * i] = vel_[2 * last];
        vel_[2 * i + 1] = vel_[2 * last + 1];
    }
    pos_.resize(2 * last);
    vel_.resize(2 * last);
}

void MotionBatch::step(double dt, const GridMap& map) {
    const std::size_t n = pos_.size();
    if (n == 0) return;
    prev_ = pos_;
    opt::vec2_madd(pos_.data(), vel_.data(), dt, n);  // pos += vel * dt for all points

    for (std::size_t i = 0; i < n; i += 2) {
        const double ox = prev_[i], oy = prev_[i + 1];
        const double nx = pos_[i], ny = pos_[i + 1];
        if (!solid_at(map, nx, ny)) continue;
        const bool hit_x = solid_at(map, nx, oy);
        const bool hit_y = solid_at(map, ox, ny);
        const bool corner = !hit_x && !hit_y;  // only the diagonal cell is solid: reverse both
        if (hit_x || corner) { pos_[i] = ox; vel_[i] = -vel_[i]; }
        if (hit_y || corner) { pos_[i + 1] = oy; vel_[i + 1] = -vel_[i + 1]; }
    }
}

std::uint64_t MotionBatch::checksum() const noexcept {
    std::uint64_t h = scalar::kFnvOffsetBasis;
    auto mix = [&h](const std::vector<double>& v) {
        for (double d : v) {
            std::uint64_t bits;
            std::memcpy(&bits, &d, sizeof bits);
            for (unsigned s = 0; s < 64; s += 8) { h ^= (bits >> s) & 0xFFu; h *= 1099511628211ull; }
        }
    };
    mix(pos_);
    mix(vel_);
    return h;
}

}  // namespace ge
