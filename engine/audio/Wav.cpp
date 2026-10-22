#include "engine/audio/Wav.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>

namespace ge {
namespace {
void put32(std::ofstream& f, std::uint32_t v) { for (int i = 0; i < 4; ++i) f.put(static_cast<char>((v >> (8 * i)) & 0xFF)); }
void put16(std::ofstream& f, std::uint16_t v) { for (int i = 0; i < 2; ++i) f.put(static_cast<char>((v >> (8 * i)) & 0xFF)); }
}  // namespace

Status write_wav_stereo16(const std::filesystem::path& path, const std::vector<float>& samples, int rate) {
    if (rate <= 0) return Status::failure("invalid WAV sample rate");
    if (samples.size() % 2 != 0) return Status::failure("stereo WAV needs an even number of samples");
    std::ofstream f(path, std::ios::binary);
    if (!f) return Status::failure("cannot write '" + path.string() + "'");
    const std::uint32_t data_bytes = static_cast<std::uint32_t>(samples.size() * 2);
    f.write("RIFF", 4); put32(f, 36 + data_bytes); f.write("WAVE", 4);
    f.write("fmt ", 4); put32(f, 16); put16(f, 1); put16(f, 2);
    put32(f, static_cast<std::uint32_t>(rate)); put32(f, static_cast<std::uint32_t>(rate) * 4); put16(f, 4); put16(f, 16);
    f.write("data", 4); put32(f, data_bytes);
    for (float x : samples) {
        const float c = std::clamp(x, -1.0f, 1.0f);
        put16(f, static_cast<std::uint16_t>(static_cast<std::int16_t>(std::lrintf(c * 32767.0f))));
    }
    return f ? Status::success() : Status::failure("error while writing '" + path.string() + "'");
}

}  // namespace ge
