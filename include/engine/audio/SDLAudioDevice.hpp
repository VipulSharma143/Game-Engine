#pragma once
// Plays an AudioMixer through the default SDL3 audio output. Only compiled with GE_HAVE_SDL.

#include <atomic>
#include <cstdint>
#include <memory>

#include "engine/audio/AudioMixer.hpp"
#include "engine/core/Status.hpp"

namespace ge {

class SDLAudioDevice {
public:
    SDLAudioDevice();
    ~SDLAudioDevice();
    SDLAudioDevice(const SDLAudioDevice&) = delete;
    SDLAudioDevice& operator=(const SDLAudioDevice&) = delete;

    // Opens the default playback device at the mixer's sample rate (stereo float).
    Status open(AudioMixer& mixer);
    void close() noexcept;
    bool is_open() const noexcept;
    // Frames handed to the device so far (grows while the device is consuming audio).
    std::uint64_t frames_delivered() const noexcept;

    struct Impl;  // defined in the .cpp; public only so the audio callback can name it

private:
    std::unique_ptr<Impl> impl_;
};

}  // namespace ge
