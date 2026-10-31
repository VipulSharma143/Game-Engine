#pragma once

#include <cstddef>
#include <cstdint>

namespace ge::scalar {

// Portable reference implementations. Two's-complement wrap-around on overflow
// (defined behaviour), matching the NASM routines bit for bit.
std::int64_t add_i64(std::int64_t a, std::int64_t b) noexcept;
std::int64_t multiply_i64(std::int64_t a, std::int64_t b) noexcept;

inline constexpr std::uint64_t kFnvOffsetBasis = 14695981039346656037ull;

// The pixel references are header-inline so the C++ fallback path costs the same
// as hand-written loops at the call site.

// dst[0..count) = value
inline void fill_u32(std::uint32_t* dst, std::uint32_t value, std::size_t count) noexcept {
    for (std::size_t i = 0; i < count; ++i) dst[i] = value;
}

// dst[0], dst[stride], ... (count pixels) = value; stride is in elements
inline void fill_column_u32(std::uint32_t* dst, std::size_t stride, std::uint32_t value, std::size_t count) noexcept {
    for (std::size_t i = 0; i < count; ++i, dst += stride) *dst = value;
}

// FNV-1a 64 over the little-endian bytes of each pixel, continuing from `hash`
inline std::uint64_t fnv1a64_u32(const std::uint32_t* pixels, std::size_t count, std::uint64_t hash) noexcept {
    for (std::size_t i = 0; i < count; ++i) {
        for (unsigned shift = 0; shift < 32; shift += 8) {
            hash ^= (pixels[i] >> shift) & 0xFFu;
            hash *= 1099511628211ull;
        }
    }
    return hash;
}

// dst[i] += src[i] * scale for i in [0, count). `count` is in doubles. Each element is a
// separate multiply then add (no fused multiply-add), matching ge_vec2_madd bit for bit.
inline void vec2_madd(double* dst, const double* src, double scale, std::size_t count) noexcept {
    for (std::size_t i = 0; i < count; ++i) {
        const volatile double product = src[i] * scale;  // volatile: forbid FMA contraction
        dst[i] = dst[i] + product;
    }
}

// Fog blend of `count` 0xAARRGGBB pixels towards `fog_color`:
//   out = (channel * light + fog_channel * (256 - light)) >> 8, for all four channels.
// `light` is in [0, 256] (256 = unchanged, 0 = pure fog). `dst` may equal `src`.
//
// Two channels are processed per 32-bit multiply (even bytes and odd bytes separately); each
// 16-bit lane stays below 65536 (255 * 256), so lanes never carry into each other. The result is
// identical to fog_blend_reference below.
inline void fog_blend(std::uint32_t* dst, const std::uint32_t* src, std::size_t count, std::uint32_t light,
                      std::uint32_t fog_color) noexcept {
    const std::uint32_t inv = 256u - light;
    const std::uint32_t fog_even = ((fog_color & 0x00FF00FFu) * inv);
    const std::uint32_t fog_odd = (((fog_color >> 8) & 0x00FF00FFu) * inv);
    for (std::size_t i = 0; i < count; ++i) {
        const std::uint32_t c = src[i];
        const std::uint32_t even = (((c & 0x00FF00FFu) * light + fog_even) >> 8) & 0x00FF00FFu;
        const std::uint32_t odd = ((((c >> 8) & 0x00FF00FFu) * light + fog_odd) >> 8) & 0x00FF00FFu;
        dst[i] = even | (odd << 8);
    }
}

// Obvious per-channel version: the ground truth the fast versions are tested against.
inline void fog_blend_reference(std::uint32_t* dst, const std::uint32_t* src, std::size_t count, std::uint32_t light,
                                std::uint32_t fog_color) noexcept {
    const std::uint32_t inv = 256u - light;
    for (std::size_t i = 0; i < count; ++i) {
        const std::uint32_t c = src[i];
        std::uint32_t out = 0;
        for (unsigned shift = 0; shift < 32; shift += 8) {
            const std::uint32_t ch = (c >> shift) & 0xFFu;
            const std::uint32_t fg = (fog_color >> shift) & 0xFFu;
            out |= (((ch * light + fg * inv) >> 8) & 0xFFu) << shift;
        }
        dst[i] = out;
    }
}

// ---- floor / ceiling row ------------------------------------------------------------------------
// floor(v) as an int for |v| < 2^30. Truncate, then step down for negative non-integers; this is the
// same operation the SIMD kernels perform (SSE2 has no floor instruction).
inline int floor_to_int(double v) noexcept {
    const int t = static_cast<int>(v);
    return t - (v < static_cast<double>(t) ? 1 : 0);
}

// One screen row of floor or ceiling: pixel i samples a 64x64 texture at the world position
// (fx + i*sx, fy + i*sy). The multiply and the add are separate operations (no FMA) so that the
// NASM kernels, which cannot fuse them either, produce the same bits.
//   dst[i] = texels[((cx + cy) & parity_mask) << 12 | (ty & 63) << 6 | (tx & 63)]
// with cx = floor(x), tx = int((x - cx) * 64), likewise for y. `parity_mask` is 0 or 1: 1 reads the
// checkerboard of two textures stored back to back (floor), 0 always reads the first (ceiling).
inline void floor_span(std::uint32_t* dst, std::size_t count, const std::uint32_t* texels, double fx, double fy,
                       double sx, double sy, std::uint32_t parity_mask) noexcept {
    double di = 0.0;  // == double(i) exactly (small integers), without a conversion per pixel
    for (std::size_t i = 0; i < count; ++i, di += 1.0) {
        const double x = fx + di * sx;
        const double y = fy + di * sy;
        const int cx = floor_to_int(x);
        const int cy = floor_to_int(y);
        const std::uint32_t tx = static_cast<std::uint32_t>(static_cast<int>((x - static_cast<double>(cx)) * 64.0)) & 63u;
        const std::uint32_t ty = static_cast<std::uint32_t>(static_cast<int>((y - static_cast<double>(cy)) * 64.0)) & 63u;
        const std::uint32_t variant = (static_cast<std::uint32_t>(cx) + static_cast<std::uint32_t>(cy)) & parity_mask;
        dst[i] = texels[(variant << 12) | (ty << 6) | tx];
    }
}

// ---- sprite pixels ------------------------------------------------------------------------------
// Shades one texel: per colour channel c, v = min(255, (c * light) >> 8), then v += ((255 - v) * flash) >> 8.
// `light` is 8.8 fixed point in [0, 1023] (256 = unchanged), `flash` in [0, 256] (256 = white). The result is opaque.
inline std::uint32_t shade_u32(std::uint32_t c, std::uint32_t light, std::uint32_t flash) noexcept {
    std::uint32_t out = 0xFF000000u;
    for (unsigned shift = 0; shift < 24; shift += 8) {
        std::uint32_t v = (((c >> shift) & 0xFFu) * light) >> 8;
        if (v > 255u) v = 255u;
        v += ((255u - v) * flash) >> 8;
        out |= v << shift;
    }
    return out;
}

// One row of a sprite: for each i with ix[i] >= 0 and alpha(row[ix[i]]) >= 128, dst[i] = shade_u32(row[ix[i]]).
// All other pixels are left alone. Returns true if any pixel was written.
inline bool sprite_span(std::uint32_t* dst, std::size_t count, const std::uint32_t* row, const std::int32_t* ix,
                        std::uint32_t light, std::uint32_t flash) noexcept {
    bool any = false;
    for (std::size_t i = 0; i < count; ++i) {
        if (ix[i] < 0) continue;
        const std::uint32_t c = row[ix[i]];
        if ((c >> 24) < 128u) continue;
        dst[i] = shade_u32(c, light, flash);
        any = true;
    }
    return any;
}

// ---- full-screen tint ---------------------------------------------------------------------------
// In place, per channel (order b, g, r, a): out = min(255, ((c * mul) >> 8) + add), mul in [0, 256], add in [0, 255].
inline void tint_u32(std::uint32_t* px, std::size_t count, const std::uint16_t* mul_bgra, const std::uint16_t* add_bgra) noexcept {
    for (std::size_t i = 0; i < count; ++i) {
        std::uint32_t out = 0;
        for (unsigned k = 0; k < 4; ++k) {
            const unsigned shift = k * 8u;
            std::uint32_t v = ((((px[i] >> shift) & 0xFFu) * mul_bgra[k]) >> 8) + add_bgra[k];
            if (v > 255u) v = 255u;
            out |= v << shift;
        }
        px[i] = out;
    }
}

}  // namespace ge::scalar
