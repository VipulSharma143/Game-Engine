#include "engine/renderer/Particles.hpp"

#include <cmath>

namespace ge {

namespace {
constexpr double kFloor = -0.5;  // z of the floor
}

double ParticleSystem::random01() noexcept {
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return static_cast<double>(rng_ >> 8) / 16777216.0;
}

bool ParticleSystem::emit(const Particle& p) noexcept {
    if (count_ >= kCapacity) return false;
    pool_[count_++] = p;
    return true;
}

void ParticleSystem::burst(Vec2 position, double z, int count, double speed, double up_speed, const Particle& t) noexcept {
    for (int i = 0; i < count; ++i) {
        Particle p = t;
        const double angle = random01() * 2.0 * kPi;
        const double s = speed * (0.25 + 0.75 * random01());
        p.position = position;
        p.z = z;
        p.velocity = {std::cos(angle) * s, std::sin(angle) * s};
        p.vz = up_speed * (random01() * 1.2 - 0.2);
        p.life = t.life * (0.6 + 0.4 * random01());
        p.age = 0.0;
        if (!emit(p)) return;
    }
}

void ParticleSystem::step(double dt, const GridMap& map) noexcept {
    for (std::size_t i = 0; i < count_;) {
        Particle& p = pool_[i];
        p.age += dt;
        bool dead = p.age >= p.life;
        if (!dead) {
            p.vz -= p.gravity * dt;
            p.z += p.vz * dt;
            if (p.z < kFloor) {  // bounce, losing most of the energy
                p.z = kFloor;
                p.vz = -p.vz * 0.35;
                p.velocity = {p.velocity.x * 0.6, p.velocity.y * 0.6};
            }
            const Vec2 next{p.position.x + p.velocity.x * dt, p.position.y + p.velocity.y * dt};
            if (map.is_solid(static_cast<int>(std::floor(next.x)), static_cast<int>(std::floor(next.y)))) dead = true;
            else p.position = next;
        }
        if (dead) pool_[i] = pool_[--count_];  // swap-remove; the swapped-in particle is processed next
        else ++i;
    }
}

void ParticleSystem::append_sprites(std::vector<Sprite>& out) const {
    for (std::size_t i = 0; i < count_; ++i) {
        const Particle& p = pool_[i];
        const double t = p.life > 0.0 ? p.age / p.life : 1.0;
        Sprite s;
        s.position = p.position;
        s.size = p.size * (p.glow ? 1.0 : 1.0 - 0.4 * t);
        s.color = p.color;
        s.z = p.z;
        s.light = p.glow ? 1.6 : 1.0 - 0.5 * t;
        out.push_back(s);
    }
}

}  // namespace ge
