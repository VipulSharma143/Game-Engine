#include "game/GameAudio.hpp"

#include <algorithm>
#include <cmath>

namespace game {

float audio_pan_for(const ge::Camera& camera, ge::Vec2 world) noexcept {
    const ge::Vec2 rel = world - camera.position;
    const double len = std::sqrt(rel.x * rel.x + rel.y * rel.y);
    const double plane_len = std::sqrt(camera.plane.x * camera.plane.x + camera.plane.y * camera.plane.y);
    if (len < 1e-6 || plane_len < 1e-12) return 0.0f;
    const double lateral = (rel.x * camera.plane.x + rel.y * camera.plane.y) / (len * plane_len);
    return static_cast<float>(std::clamp(lateral, -1.0, 1.0));
}

GameAudio::GameAudio(GameApplication& app, ge::AudioMixer& mixer, const ge::SoundBank& bank)
    : app_(app), mixer_(mixer), bank_(bank) {
    ge::EventBus& bus = app_.events();
    auto on = [this, &bus](auto type_tag, auto&& handler) {
        using E = decltype(type_tag);
        const auto token = bus.subscribe<E>(std::forward<decltype(handler)>(handler));
        unsubscribe_.push_back([&bus, token] { bus.unsubscribe<E>(token); });
    };

    on(ShotFired{}, [this](const ShotFired&) { mixer_.play(bank_.shot, 0.9f); });
    on(TargetHit{}, [this](const TargetHit& e) { mixer_.play(bank_.hit, 0.8f, audio_pan_for(app_.camera(), e.position)); });
    on(TargetDestroyed{}, [this](const TargetDestroyed& e) { mixer_.play(bank_.kill, 0.9f, audio_pan_for(app_.camera(), e.position)); });
    on(ReloadStarted{}, [this](const ReloadStarted&) { mixer_.play(bank_.reload, 0.7f); });
    on(WaveStarted{}, [this](const WaveStarted&) { mixer_.play(bank_.wave_start, 0.7f); });
    on(WaveCleared{}, [this](const WaveCleared&) { mixer_.play(bank_.wave_clear, 0.8f); });
    on(GameWon{}, [this](const GameWon&) { mixer_.play(bank_.win, 0.9f); });
    on(StateChanged{}, [this](const StateChanged& e) {
        // Menu navigation blips; entering Victory/Quit has its own sound or none.
        if (e.to == GameState::Playing || e.to == GameState::Paused) mixer_.play(bank_.ui, 0.6f);
    });
    on(FixedTick{}, [this](const FixedTick& t) {
        if (!sink_) return;
        pending_frames_ += t.dt * mixer_.sample_rate();
        const auto frames = static_cast<std::size_t>(pending_frames_);
        pending_frames_ -= static_cast<double>(frames);
        scratch_.resize(frames * 2);
        mixer_.mix(scratch_.data(), frames);
        sink_->insert(sink_->end(), scratch_.begin(), scratch_.end());
    });
}

GameAudio::~GameAudio() {
    for (auto& undo : unsubscribe_) undo();
}

}  // namespace game
