#pragma once

#include <cstdint>
#include <vector>

#include "engine/platform/Platform.hpp"

namespace ge {

struct InputSegment {
    std::uint64_t frames = 0;
    double forward = 0.0;
    double strafe = 0.0;
    double turn = 0.0;
    bool fire = false;
};

// Deterministic input playback: segment after segment, then zero input.
class ScriptedInput {
public:
    ScriptedInput() = default;
    explicit ScriptedInput(std::vector<InputSegment> segments) : segments_(std::move(segments)) {}

    InputState sample(std::uint64_t frame) const noexcept;
    std::uint64_t total_frames() const noexcept;

private:
    std::vector<InputSegment> segments_;
};

}  // namespace ge
