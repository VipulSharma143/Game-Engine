#pragma once
// A fixed-size pool of short-lived particles (sparks, smoke, blood, explosions). Particles live in the world
// (a cell position plus a height), fall with gravity, bounce off the floor and die on walls. They are drawn through
// the sprite renderer. Everything is deterministic: the same calls always give the same particles, and nothing
// allocates after construction.

#include <array>
#include <cstdint>
#include <vector>

#include "engine/core/Math.hpp"
#include "engine/renderer/SpriteRenderer.hpp"
#include "engine/world/World.hpp"

namespace ge {

struct Particle {
    Vec2 position{};
    double z = 0.0;          // height in wall heights above the middle of the view: -0.5 is the floor
    Vec2 velocity{};         // cells per second
    double vz = 0.0;
    double age = 0.0;
    double life = 1.0;       // seconds
    double size = 0.06;      // sprite diameter in wall heights
    double gravity = 1.5;    // wall heights per second squared (0 floats, e.g. smoke)
    std::uint32_t color = 0xFFFFFFFFu;
    bool glow = false;       // stays bright until it dies instead of dimming
};

class ParticleSystem {
public:
    static constexpr std::size_t kCapacity = 768;

    // Adds one particle. Returns false (and drops it) when the pool is full.
    bool emit(const Particle& p) noexcept;

    // `count` particles from one point flying out in random directions at up to `speed` cells per second and up to
    // `up_speed` upwards. `template_particle` supplies colour, size, life, gravity and glow.
    void burst(Vec2 position, double z, int count, double speed, double up_speed, const Particle& template_particle) noexcept;

    // Moves everything on by dt seconds and removes particles that expired or hit a wall.
    void step(double dt, const GridMap& map) noexcept;

    // Appends one sprite per live particle.
    void append_sprites(std::vector<Sprite>& out) const;

    std::size_t alive() const noexcept { return count_; }
    void clear() noexcept { count_ = 0; }

private:
    double random01() noexcept;  // [0, 1), xorshift32

    std::array<Particle, kCapacity> pool_{};
    std::size_t count_ = 0;
    std::uint32_t rng_ = 0x2545F491u;
};

}  // namespace ge
