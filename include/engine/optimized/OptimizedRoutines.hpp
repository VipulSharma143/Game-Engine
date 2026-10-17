#pragma once

#include <cstddef>
#include <cstdint>

#include "engine/optimized/ScalarFallback.hpp"

#if defined(GE_HAVE_ASM)
// Raw NASM entry points (asm/x86_64/common/pixels.asm; argument registers are mapped per ABI).
extern "C" {
void ge_fill_u32(std::uint32_t* dst, std::uint32_t value, std::size_t count);
void ge_fill_column_u32(std::uint32_t* dst, std::size_t stride_bytes, std::uint32_t value, std::size_t count);
std::uint64_t ge_fnv1a64_u32(const std::uint32_t* pixels, std::size_t count, std::uint64_t hash);
}
#endif

namespace ge::opt {

// True when the NASM implementations are linked into this build (GE_HAVE_ASM).
bool asm_enabled() noexcept;

// Dispatch to NASM when enabled, otherwise to the scalar fallback. Results are
// identical either way; see tests/assembly.
std::int64_t add_i64(std::int64_t a, std::int64_t b) noexcept;
std::int64_t multiply_i64(std::int64_t a, std::int64_t b) noexcept;

// Pixel routines: same contracts as the ge::scalar:: versions of the same name.
// Inline so a call goes straight to the NASM routine (or the inlined C++ loop).
inline void fill_u32(std::uint32_t* dst, std::uint32_t value, std::size_t count) noexcept {
#if defined(GE_HAVE_ASM)
    ge_fill_u32(dst, value, count);
#else
    scalar::fill_u32(dst, value, count);
#endif
}

inline void fill_column_u32(std::uint32_t* dst, std::size_t stride, std::uint32_t value, std::size_t count) noexcept {
#if defined(GE_HAVE_ASM)
    ge_fill_column_u32(dst, stride * sizeof(std::uint32_t), value, count);
#else
    scalar::fill_column_u32(dst, stride, value, count);
#endif
}

inline std::uint64_t fnv1a64_u32(const std::uint32_t* pixels, std::size_t count, std::uint64_t hash) noexcept {
#if defined(GE_HAVE_ASM)
    return ge_fnv1a64_u32(pixels, count, hash);
#else
    return scalar::fnv1a64_u32(pixels, count, hash);
#endif
}

}  // namespace ge::opt
