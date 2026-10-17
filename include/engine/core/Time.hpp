#pragma once

#include <cstdint>

namespace ge {

// Seconds from an arbitrary fixed origin, from the monotonic steady clock.
double monotonic_seconds() noexcept;

// Converts raw frame deltas into safe simulation deltas: negative values become
// zero and long stalls (debugger pauses, window drags) are clamped.
class FrameTimer {
public:
    explicit FrameTimer(double max_frame_time) noexcept : max_frame_time_(max_frame_time) {}

    double advance(double raw_delta) noexcept;

    double delta() const noexcept { return delta_; }
    double total() const noexcept { return total_; }
    std::uint64_t frame_count() const noexcept { return frames_; }
    std::uint64_t clamped_count() const noexcept { return clamped_; }

private:
    double max_frame_time_;
    double delta_ = 0.0;
    double total_ = 0.0;
    std::uint64_t frames_ = 0;
    std::uint64_t clamped_ = 0;
};

}  // namespace ge
