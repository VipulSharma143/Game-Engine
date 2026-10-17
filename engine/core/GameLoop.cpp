#include "engine/core/GameLoop.hpp"

#include <cmath>

namespace ge {

int FixedStepper::consume(double frame_dt) noexcept {
    accumulator_ += frame_dt;
    int steps = 0;
    while (accumulator_ >= step_ && steps < max_steps_) {
        accumulator_ -= step_;
        ++steps;
    }
    if (accumulator_ >= step_) accumulator_ = std::fmod(accumulator_, step_);
    return steps;
}

}  // namespace ge
