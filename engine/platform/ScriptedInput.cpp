#include "engine/platform/ScriptedInput.hpp"

namespace ge {

InputState ScriptedInput::sample(std::uint64_t frame) const noexcept {
    InputState state;
    std::uint64_t start = 0;
    for (const InputSegment& s : segments_) {
        if (frame < start + s.frames) {
            state.forward = s.forward;
            state.strafe = s.strafe;
            state.turn = s.turn;
            return state;
        }
        start += s.frames;
    }
    return state;
}

std::uint64_t ScriptedInput::total_frames() const noexcept {
    std::uint64_t total = 0;
    for (const InputSegment& s : segments_) total += s.frames;
    return total;
}

}  // namespace ge
