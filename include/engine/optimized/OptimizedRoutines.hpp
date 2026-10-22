#pragma once

#include <cstddef>
#include <cstdint>

#include "engine/optimized/ScalarFallback.hpp"

#if defined(GE_HAVE_ASM)
// Raw NASM entry points (asm/x86_64/common/pixels.asm; argument registers are mapped per ABI).
namespace ge::opt {
struct FogParams {
    std::uint32_t light;      // 0..256
    std::uint32_t fog_color;  // 0xAARRGGBB
};
}  // namespace ge::opt

extern "C" {
void ge_fog_blend_sse2(std::uint32_t* dst, const std::uint32_t* src, std::size_t count, const ge::opt::FogParams* params);
void ge_fog_blend_avx2(std::uint32_t* dst, const std::uint32_t* src, std::size_t count, const ge::opt::FogParams* params);
}

extern "C" {
void ge_fill_u32(std::uint32_t* dst, std::uint32_t value, std::size_t count);
void ge_fill_column_u32(std::uint32_t* dst, std::size_t stride_bytes, std::uint32_t value, std::size_t count);
std::uint64_t ge_fnv1a64_u32(const std::uint32_t* pixels, std::size_t count, std::uint64_t hash);
void ge_vec2_madd(double* dst, const double* src, const double* scale, std::size_t count);
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

// dst[i] += src[i] * scale over `count` doubles (SSE2 NASM routine, else the scalar loop).
inline void vec2_madd(double* dst, const double* src, double scale, std::size_t count) noexcept {
#if defined(GE_HAVE_ASM)
    ge_vec2_madd(dst, src, &scale, count);
#else
    scalar::vec2_madd(dst, src, scale, count);
#endif
}

// ---- fog blend: scalar C++, SSE2 or AVX2, chosen at run time ------------------------------------
enum class FogKernel { Scalar, Sse2, Avx2 };
const char* fog_kernel_name(FogKernel kernel) noexcept;
// Scalar is always available; SSE2/AVX2 need a build with NASM and a CPU (and OS) that supports them.
bool fog_kernel_available(FogKernel kernel) noexcept;
FogKernel best_fog_kernel() noexcept;      // fastest available (AVX2 > SSE2 > Scalar)
FogKernel active_fog_kernel() noexcept;    // what fog_blend() currently uses (default: best)
// Forces a kernel (tests, benchmarks). An unavailable kernel is ignored and false is returned.
bool set_fog_kernel(FogKernel kernel) noexcept;
// dst[i] = fog(src[i]) for `count` pixels; see scalar::fog_blend. `dst` may equal `src`.
void fog_blend(std::uint32_t* dst, const std::uint32_t* src, std::size_t count, std::uint32_t light,
               std::uint32_t fog_color) noexcept;

}  // namespace ge::opt
