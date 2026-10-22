#include "engine/audio/AudioMixer.hpp"

#include <algorithm>
#include <cmath>

namespace ge {

void AudioMixer::set_master_volume(float volume) {
    std::lock_guard<std::mutex> lock(mutex_);
    master_ = std::clamp(volume, 0.0f, 1.0f);
}

float AudioMixer::master_volume() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return master_;
}

bool AudioMixer::play(const Sound& sound, float volume, float pan) {
    if (sound.samples.empty() || sound.sample_rate != sample_rate_) return false;
    volume = std::clamp(volume, 0.0f, 1.0f);
    pan = std::clamp(pan, -1.0f, 1.0f);
    // Constant-power pan law: centre is -3 dB per side, hard left/right is full on one side only.
    const float angle = (pan + 1.0f) * 0.25f * 3.14159265358979f;
    Voice v{&sound, 0, volume * std::cos(angle), volume * std::sin(angle)};

    std::lock_guard<std::mutex> lock(mutex_);
    if (voices_.size() >= kMaxVoices) voices_.erase(voices_.begin());  // steal the oldest voice
    voices_.push_back(v);
    ++started_[sound.name];
    ++total_started_;
    return true;
}

void AudioMixer::mix(float* out, std::size_t frames) {
    std::fill(out, out + frames * 2, 0.0f);
    std::lock_guard<std::mutex> lock(mutex_);
    for (Voice& v : voices_) {
        const std::vector<float>& s = v.sound->samples;
        const std::size_t n = std::min(frames, s.size() - v.position);
        for (std::size_t i = 0; i < n; ++i) {
            const float x = s[v.position + i];
            out[2 * i] += x * v.gain_left;
            out[2 * i + 1] += x * v.gain_right;
        }
        v.position += n;
    }
    voices_.erase(std::remove_if(voices_.begin(), voices_.end(), [](const Voice& v) { return v.position >= v.sound->samples.size(); }),
                  voices_.end());
    for (std::size_t i = 0; i < frames * 2; ++i) out[i] = std::clamp(out[i] * master_, -1.0f, 1.0f);
}

std::size_t AudioMixer::active_voices() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return voices_.size();
}

std::map<std::string, std::size_t> AudioMixer::started_counts() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return started_;
}

std::size_t AudioMixer::total_started() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return total_started_;
}

void AudioMixer::stop_all() {
    std::lock_guard<std::mutex> lock(mutex_);
    voices_.clear();
}

}  // namespace ge
