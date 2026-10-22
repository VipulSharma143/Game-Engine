#pragma once

#include <string>
#include <vector>

namespace ge {

// A mono sound effect held in memory as float samples in [-1, 1].
struct Sound {
    std::string name;
    int sample_rate = 44100;
    std::vector<float> samples;

    double duration_seconds() const noexcept {
        return sample_rate > 0 ? static_cast<double>(samples.size()) / sample_rate : 0.0;
    }
};

}  // namespace ge
