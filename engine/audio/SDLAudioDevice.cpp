#include "engine/audio/SDLAudioDevice.hpp"

#include <SDL3/SDL.h>

#include <string>
#include <vector>

namespace ge {

struct SDLAudioDevice::Impl {
    SDL_AudioStream* stream = nullptr;
    AudioMixer* mixer = nullptr;
    bool audio_started = false;
    std::atomic<std::uint64_t> frames{0};
    std::vector<float> scratch;
};

namespace {
// Runs on SDL's audio thread whenever the device wants more samples.
void SDLCALL pull(void* userdata, SDL_AudioStream* stream, int additional_amount, int /*total_amount*/) {
    auto* impl = static_cast<SDLAudioDevice::Impl*>(userdata);
    if (additional_amount <= 0) return;
    const std::size_t frames = static_cast<std::size_t>(additional_amount) / (2 * sizeof(float));
    if (frames == 0) return;
    impl->scratch.resize(frames * 2);
    impl->mixer->mix(impl->scratch.data(), frames);
    SDL_PutAudioStreamData(stream, impl->scratch.data(), static_cast<int>(frames * 2 * sizeof(float)));
    impl->frames += frames;
}
}  // namespace

SDLAudioDevice::SDLAudioDevice() : impl_(std::make_unique<Impl>()) {}
SDLAudioDevice::~SDLAudioDevice() { close(); }

Status SDLAudioDevice::open(AudioMixer& mixer) {
    Impl& s = *impl_;
    if (s.stream) return Status::failure("audio device already open");
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) return Status::failure(std::string("SDL audio init: ") + SDL_GetError());
    s.audio_started = true;
    s.mixer = &mixer;
    SDL_AudioSpec spec;
    SDL_zero(spec);
    spec.format = SDL_AUDIO_F32;
    spec.channels = 2;
    spec.freq = mixer.sample_rate();
    s.stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, pull, &s);
    if (!s.stream) {
        const std::string msg = std::string("SDL_OpenAudioDeviceStream: ") + SDL_GetError();
        close();
        return Status::failure(msg);
    }
    if (!SDL_ResumeAudioStreamDevice(s.stream)) {
        const std::string msg = std::string("SDL_ResumeAudioStreamDevice: ") + SDL_GetError();
        close();
        return Status::failure(msg);
    }
    return Status::success();
}

void SDLAudioDevice::close() noexcept {
    Impl& s = *impl_;
    if (s.stream) { SDL_DestroyAudioStream(s.stream); s.stream = nullptr; }
    if (s.audio_started) { SDL_QuitSubSystem(SDL_INIT_AUDIO); s.audio_started = false; }
}

bool SDLAudioDevice::is_open() const noexcept { return impl_->stream != nullptr; }
std::uint64_t SDLAudioDevice::frames_delivered() const noexcept { return impl_->frames.load(); }

}  // namespace ge
