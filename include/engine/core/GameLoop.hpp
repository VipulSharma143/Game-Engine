#pragma once

namespace ge {

// Fixed-timestep accumulator. Excess time beyond max_steps is discarded so a
// stall can never cause an unbounded catch-up spiral.
class FixedStepper {
public:
    FixedStepper(double step, int max_steps) noexcept : step_(step), max_steps_(max_steps) {}

    // Adds frame time; returns how many fixed steps to simulate now.
    int consume(double frame_dt) noexcept;

    double step() const noexcept { return step_; }
    double accumulator() const noexcept { return accumulator_; }
    // Fraction of the next step already elapsed, in [0, 1).
    double alpha() const noexcept { return accumulator_ / step_; }

private:
    double step_;
    int max_steps_;
    double accumulator_ = 0.0;
};

}  // namespace ge
