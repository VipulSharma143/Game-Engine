#pragma once
// Software mixer: any number of Sounds play as voices and are summed into an interleaved
// stereo float stream. It is plain C++ with no device dependency, so it can be tested and
// rendered offline; SDLAudioDevice pulls from it on the audio thread. All methods are
// thread-safe (one mutex; the work per call is tiny).

#include <cstddef>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "engine/audio/Sound.hpp"

namespace ge {

class AudioMixer {
public:
    static constexpr std::size_t kMaxVoices = 16;

    explicit AudioMixer(int sample_rate = 44100) : sample_rate_(sample_rate) {}

    int sample_rate() const noexcept { return sample_rate_; }

    // Master volume in [0, 1].
    void set_master_volume(float volume);
    float master_volume() const;

    // Starts a voice. `pan` is -1 (left) .. +1 (right), `volume` 0..1. The Sound must outlive the
    // voice and must have the mixer's sample rate (other rates are rejected). If all voices are
    // busy the oldest one is stopped. Returns false if the sound was rejected.
    bool play(const Sound& sound, float volume = 1.0f, float pan = 0.0f);

    // Writes `frames` stereo frames (2 * frames floats) to `out`, replacing its contents.
    void mix(float* out, std::size_t frames);

    std::size_t active_voices() const;
    // How many times each sound (by name) has been started, and the total.
    std::map<std::string, std::size_t> started_counts() const;
    std::size_t total_started() const;
    void stop_all();

private:
    struct Voice {
        const Sound* sound;
        std::size_t position;
        float gain_left;
        float gain_right;
    };

    int sample_rate_;
    mutable std::mutex mutex_;
    std::vector<Voice> voices_;
    float master_ = 1.0f;
    std::map<std::string, std::size_t> started_;
    std::size_t total_started_ = 0;
};

}  // namespace ge
