#pragma once
// 16-bit PCM stereo WAV output, used to save offline-rendered audio.

#include <cstdint>
#include <filesystem>
#include <vector>

#include "engine/core/Status.hpp"

namespace ge {

// `interleaved` holds left/right float samples in [-1, 1] (values are clamped).
Status write_wav_stereo16(const std::filesystem::path& path, const std::vector<float>& interleaved, int sample_rate);

}  // namespace ge
