#pragma once
// Connects game events to sound effects. The game never calls the mixer directly: it
// publishes events, and this class (if present) turns them into sounds.

#include <functional>
#include <vector>

#include "engine/audio/AudioMixer.hpp"
#include "engine/audio/Synth.hpp"
#include "game/GameApplication.hpp"

namespace game {

// Stereo position of a world point relative to the camera: -1 = directly left, +1 = directly
// right, 0 = ahead or behind.
float audio_pan_for(const ge::Camera& camera, ge::Vec2 world) noexcept;

class GameAudio {
public:
    // `bank` must outlive this object (the mixer plays the bank's samples directly).
    GameAudio(GameApplication& app, ge::AudioMixer& mixer, const ge::SoundBank& bank);
    ~GameAudio();
    GameAudio(const GameAudio&) = delete;
    GameAudio& operator=(const GameAudio&) = delete;

    // Render the mixer output into `sink` in step with the game clock (no audio device needed).
    // Pass nullptr to stop. Do not use together with a device that also pulls from the mixer.
    void capture_offline(std::vector<float>* sink) { sink_ = sink; }

private:
    GameApplication& app_;
    ge::AudioMixer& mixer_;
    const ge::SoundBank& bank_;
    std::vector<std::function<void()>> unsubscribe_;
    std::vector<float>* sink_ = nullptr;
    double pending_frames_ = 0.0;
    std::vector<float> scratch_;
};

}  // namespace game
