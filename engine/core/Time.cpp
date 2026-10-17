#include "engine/core/Time.hpp"

#include <chrono>

namespace ge {

double monotonic_seconds() noexcept {
    using clock = std::chrono::steady_clock;
    return std::chrono::duration<double>(clock::now().time_since_epoch()).count();
}

double FrameTimer::advance(double raw_delta) noexcept {
    double d = raw_delta;
    if (!(d > 0.0)) d = 0.0;  // also maps NaN to zero
    if (d > max_frame_time_) {
        d = max_frame_time_;
        ++clamped_;
    }
    delta_ = d;
    total_ += d;
    ++frames_;
    return d;
}

}  // namespace ge
