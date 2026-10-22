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

}  // namespace ge::scalar
