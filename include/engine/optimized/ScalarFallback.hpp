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

}  // namespace ge::scalar
