#pragma once

#include <cstdint>

#include "engine/platform/Platform.hpp"
#include "engine/platform/ScriptedInput.hpp"

namespace ge {

// Portable platform with no window or display requirement. Input comes from a
// script; frames are counted and optionally dumped as PPM / ASCII at the end.
class HeadlessPlatform final : public IPlatform {
public:
    explicit HeadlessPlatform(ScriptedInput script = {}) : script_(std::move(script)) {}

    Status init(const EngineConfig& config) override;
    void shutdown() noexcept override {}
    void poll_input(InputState& input) override;
    void present(const Framebuffer& frame, std::uint64_t frame_index) override;
    void finalize(const Framebuffer& final_frame) override;

    std::uint64_t frames_presented() const noexcept { return presented_; }
    std::uint64_t last_checksum() const noexcept { return last_checksum_; }

private:
    ScriptedInput script_;
    HeadlessOptions options_;
    std::uint64_t polled_ = 0;
    std::uint64_t presented_ = 0;
    std::uint64_t last_checksum_ = 0;
};

}  // namespace ge
