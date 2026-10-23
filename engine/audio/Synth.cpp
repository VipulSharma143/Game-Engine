#include "engine/audio/Synth.hpp"

#include <cmath>
#include <cstdint>

namespace ge {
namespace {

constexpr double kTwoPi = 6.283185307179586;

// Deterministic white noise in [-1, 1] (LCG), so sounds are bit-identical everywhere.
class Noise {
public:
    explicit Noise(std::uint32_t seed) : state_(seed) {}
    double next() noexcept {
        state_ = state_ * 1664525u + 1013904223u;
        return static_cast<double>(static_cast<std::int32_t>(state_)) / 2147483648.0;
    }
private:
    std::uint32_t state_;
};

Sound make_sound(const char* name, int rate, double seconds) {
    Sound s;
    s.name = name;
    s.sample_rate = rate;
    s.samples.assign(static_cast<std::size_t>(seconds * rate), 0.0f);
    return s;
}

// Adds a decaying tone (fundamental plus a little 2nd/3rd harmonic) starting at `start` seconds.
void add_tone(Sound& s, double start, double length, double freq, double amp, double decay_rate) {
    const std::size_t first = static_cast<std::size_t>(start * s.sample_rate);
    const std::size_t count = static_cast<std::size_t>(length * s.sample_rate);
    for (std::size_t i = 0; i < count && first + i < s.samples.size(); ++i) {
        const double t = static_cast<double>(i) / s.sample_rate;
        const double ph = kTwoPi * freq * t;
        const double v = std::sin(ph) + 0.3 * std::sin(2 * ph) + 0.15 * std::sin(3 * ph);
        // 3 ms attack and a 5 ms release at the end keep the edges click-free.
        const double attack = std::min(1.0, t / 0.003);
        const double release = std::min(1.0, (length - t) / 0.005);
        s.samples[first + i] += static_cast<float>(amp * v * std::exp(-decay_rate * t) * attack * release);
    }
}

void add_noise_burst(Sound& s, Noise& n, double start, double length, double amp, double decay_rate) {
    const std::size_t first = static_cast<std::size_t>(start * s.sample_rate);
    const std::size_t count = static_cast<std::size_t>(length * s.sample_rate);
    for (std::size_t i = 0; i < count && first + i < s.samples.size(); ++i) {
        const double t = static_cast<double>(i) / s.sample_rate;
        s.samples[first + i] += static_cast<float>(amp * n.next() * std::exp(-decay_rate * t) * std::min(1.0, t / 0.001) *
                                                    std::min(1.0, (length - t) / 0.004));
    }
}

void normalize_peak(Sound& s, float target) {
    float peak = 0.0f;
    for (float x : s.samples) peak = std::max(peak, std::fabs(x));
    if (peak <= 0.0f) return;
    const float k = target / peak;
    for (float& x : s.samples) x *= k;
}

}  // namespace

SoundBank SoundBank::make(int rate) {
    SoundBank b;

    b.shot = make_sound("shot", rate, 0.22);
    {
        Noise n(0x1234u);
        add_noise_burst(b.shot, n, 0.0, 0.20, 0.8, 22.0);
        // Falling square-ish thump: frequency sweeps 240 -> 55 Hz.
        double phase = 0.0;
        for (std::size_t i = 0; i < b.shot.samples.size(); ++i) {
            const double t = static_cast<double>(i) / rate;
            const double f = 55.0 + 185.0 * std::exp(-t * 28.0);
            phase += kTwoPi * f / rate;
            const double sq = std::sin(phase) > 0 ? 1.0 : -1.0;
            b.shot.samples[i] += static_cast<float>(0.45 * sq * std::exp(-t * 16.0) * std::min(1.0, t / 0.001) * std::min(1.0, (0.22 - t) / 0.01));
        }
        normalize_peak(b.shot, 0.9f);
    }

    b.hit = make_sound("hit", rate, 0.07);
    add_tone(b.hit, 0.0, 0.07, 980.0, 0.7, 25.0);
    normalize_peak(b.hit, 0.6f);

    b.kill = make_sound("kill", rate, 0.32);
    add_tone(b.kill, 0.00, 0.14, 523.25, 0.6, 9.0);
    add_tone(b.kill, 0.08, 0.14, 659.25, 0.6, 9.0);
    add_tone(b.kill, 0.16, 0.16, 783.99, 0.6, 7.0);
    normalize_peak(b.kill, 0.7f);

    b.reload = make_sound("reload", rate, 0.9);
    {
        Noise n(0x9E37u);
        add_noise_burst(b.reload, n, 0.05, 0.03, 0.9, 60.0);
        add_tone(b.reload, 0.05, 0.05, 1700.0, 0.3, 70.0);
        add_noise_burst(b.reload, n, 0.70, 0.04, 1.0, 50.0);
        add_tone(b.reload, 0.70, 0.06, 1100.0, 0.4, 60.0);
        normalize_peak(b.reload, 0.55f);
    }

    b.wave_start = make_sound("wave_start", rate, 0.34);
    add_tone(b.wave_start, 0.00, 0.14, 440.0, 0.6, 5.0);
    add_tone(b.wave_start, 0.15, 0.18, 660.0, 0.6, 5.0);
    normalize_peak(b.wave_start, 0.6f);

    b.wave_clear = make_sound("wave_clear", rate, 0.55);
    add_tone(b.wave_clear, 0.00, 0.14, 523.25, 0.5, 6.0);
    add_tone(b.wave_clear, 0.11, 0.14, 659.25, 0.5, 6.0);
    add_tone(b.wave_clear, 0.22, 0.14, 783.99, 0.5, 6.0);
    add_tone(b.wave_clear, 0.33, 0.22, 1046.5, 0.5, 5.0);
    normalize_peak(b.wave_clear, 0.7f);

    b.win = make_sound("win", rate, 1.3);
    add_tone(b.win, 0.00, 0.20, 523.25, 0.5, 3.0);
    add_tone(b.win, 0.18, 0.20, 659.25, 0.5, 3.0);
    add_tone(b.win, 0.36, 0.20, 783.99, 0.5, 3.0);
    add_tone(b.win, 0.54, 0.76, 1046.5, 0.5, 2.0);
    add_tone(b.win, 0.54, 0.76, 783.99, 0.35, 2.0);
    normalize_peak(b.win, 0.75f);

    b.ui = make_sound("ui", rate, 0.06);
    add_tone(b.ui, 0.0, 0.06, 1200.0, 0.6, 30.0);
    normalize_peak(b.ui, 0.5f);

    b.hurt = make_sound("hurt", rate, 0.22);
    {
        Noise n(0x51AEu);
        add_noise_burst(b.hurt, n, 0.0, 0.12, 0.6, 30.0);
        add_tone(b.hurt, 0.0, 0.22, 140.0, 0.9, 14.0);
        normalize_peak(b.hurt, 0.85f);
    }

    b.die = make_sound("die", rate, 0.95);
    {
        double phase = 0.0;
        for (std::size_t i = 0; i < b.die.samples.size(); ++i) {
            const double t = static_cast<double>(i) / rate;
            phase += kTwoPi * (60.0 + 340.0 * std::exp(-t * 3.2)) / rate;  // falling pitch
            b.die.samples[i] = static_cast<float>(std::sin(phase) * std::exp(-t * 2.2) * std::min(1.0, t / 0.004) * std::min(1.0, (0.95 - t) / 0.02));
        }
        normalize_peak(b.die, 0.8f);
    }

    b.pickup = make_sound("pickup", rate, 0.24);
    add_tone(b.pickup, 0.00, 0.10, 880.0, 0.6, 12.0);
    add_tone(b.pickup, 0.08, 0.16, 1320.0, 0.6, 10.0);
    normalize_peak(b.pickup, 0.55f);

    b.door = make_sound("door", rate, 0.5);
    {
        Noise n(0xD00Fu);
        add_noise_burst(b.door, n, 0.0, 0.45, 0.25, 6.0);
        double phase = 0.0;
        for (std::size_t i = 0; i < b.door.samples.size(); ++i) {
            const double t = static_cast<double>(i) / rate;
            phase += kTwoPi * (50.0 + 40.0 * t) / rate;  // low rising rumble
            b.door.samples[i] += static_cast<float>(0.7 * std::sin(phase) * std::exp(-t * 4.0) * std::min(1.0, t / 0.01) * std::min(1.0, (0.5 - t) / 0.03));
        }
        normalize_peak(b.door, 0.7f);
    }

    b.locked = make_sound("locked", rate, 0.3);
    {
        double phase = 0.0;
        for (std::size_t i = 0; i < b.locked.samples.size(); ++i) {
            const double t = static_cast<double>(i) / rate;
            const double local = t < 0.14 ? t : t - 0.16;  // two short buzzes
            if (local < 0.0 || local > 0.1) continue;
            phase += kTwoPi * 150.0 / rate;
            const double sq = std::sin(phase) > 0 ? 1.0 : -1.0;
            b.locked.samples[i] = static_cast<float>(0.6 * sq * std::min(1.0, local / 0.003) * std::min(1.0, (0.1 - local) / 0.005));
        }
        normalize_peak(b.locked, 0.55f);
    }

    return b;
}

}  // namespace ge
