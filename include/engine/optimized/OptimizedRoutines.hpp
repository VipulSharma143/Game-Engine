#pragma once

#include <cstddef>
#include <cstdint>

#include "engine/optimized/ScalarFallback.hpp"

// Parameter blocks for the span kernels. The NASM routines read them by offset (asm/x86_64/common/span.asm,
// tint.asm), so the layout is fixed and checked below. They are passed by pointer so the two calling
// conventions need no per-ABI argument mapping, and 32-byte aligned for the SIMD loads.
namespace ge::opt {

struct alignas(32) FloorSpanParams {
    double fx[2];                 // +0   position of pixel 0 (both lanes equal)
    double fy[2];                 // +16
    double sx[2];                 // +32  step per pixel
    double sy[2];                 // +48
    std::uint32_t parity[4];      // +64  0 or 1 in every lane
    const std::uint32_t* texels;  // +80
};
static_assert(offsetof(FloorSpanParams, fy) == 16 && offsetof(FloorSpanParams, sx) == 32 &&
              offsetof(FloorSpanParams, sy) == 48 && offsetof(FloorSpanParams, parity) == 64 &&
              offsetof(FloorSpanParams, texels) == 80, "FloorSpanParams is read by span.asm");

inline FloorSpanParams make_floor_span_params(const std::uint32_t* texels, double fx, double fy, double sx, double sy,
                                              std::uint32_t parity_mask) noexcept {
    FloorSpanParams p{};
    p.fx[0] = p.fx[1] = fx;
    p.fy[0] = p.fy[1] = fy;
    p.sx[0] = p.sx[1] = sx;
    p.sy[0] = p.sy[1] = sy;
    for (std::uint32_t& lane : p.parity) lane = parity_mask & 1u;
    p.texels = texels;
    return p;
}

struct alignas(32) SpriteSpanParams {
    std::uint16_t light16[16];    // +0   light (8.8 fixed point, 0..1023) in every word
    std::uint16_t flash16[16];    // +32  flash (0..256) in every word
    const std::uint32_t* row;     // +64  texels of the image row being drawn; set per row
    const std::int32_t* ix;       // +72  texel column per pixel, negative = leave the pixel alone
};
static_assert(offsetof(SpriteSpanParams, flash16) == 32 && offsetof(SpriteSpanParams, row) == 64 &&
              offsetof(SpriteSpanParams, ix) == 72, "SpriteSpanParams is read by span.asm");

inline SpriteSpanParams make_sprite_span_params(std::uint32_t light, std::uint32_t flash, const std::int32_t* ix) noexcept {
    SpriteSpanParams p{};
    if (light > 1023u) light = 1023u;
    if (flash > 256u) flash = 256u;
    for (std::uint16_t& w : p.light16) w = static_cast<std::uint16_t>(light);
    for (std::uint16_t& w : p.flash16) w = static_cast<std::uint16_t>(flash);
    p.ix = ix;
    return p;
}

struct alignas(32) TintParams {
    std::uint16_t mul16[16];      // +0   per channel, repeating b, g, r, a: 0..256 (256 = unchanged)
    std::uint16_t add16[16];      // +32  per channel: 0..255
};
static_assert(offsetof(TintParams, add16) == 32, "TintParams is read by tint.asm");

// out = min(255, ((c * mul) >> 8) + add) per colour channel; alpha is left alone.
inline TintParams make_tint_params(unsigned mul_r, unsigned mul_g, unsigned mul_b, unsigned add_r, unsigned add_g,
                                   unsigned add_b) noexcept {
    const std::uint16_t mul[4] = {static_cast<std::uint16_t>(mul_b > 256u ? 256u : mul_b),
                                  static_cast<std::uint16_t>(mul_g > 256u ? 256u : mul_g),
                                  static_cast<std::uint16_t>(mul_r > 256u ? 256u : mul_r), 256u};
    const std::uint16_t add[4] = {static_cast<std::uint16_t>(add_b > 255u ? 255u : add_b),
                                  static_cast<std::uint16_t>(add_g > 255u ? 255u : add_g),
                                  static_cast<std::uint16_t>(add_r > 255u ? 255u : add_r), 0u};
    TintParams p{};
    for (unsigned i = 0; i < 16; ++i) {
        p.mul16[i] = mul[i & 3u];
        p.add16[i] = add[i & 3u];
    }
    return p;
}

}  // namespace ge::opt

#if defined(GE_HAVE_ASM)
// Raw NASM entry points (asm/x86_64/common/pixels.asm; argument registers are mapped per ABI).
namespace ge::opt {
struct FogParams {
    std::uint32_t light;      // 0..256
    std::uint32_t fog_color;  // 0xAARRGGBB
};
}  // namespace ge::opt

extern "C" {
void ge_floor_span_sse2(std::uint32_t* dst, std::size_t count, const ge::opt::FloorSpanParams* params);
void ge_floor_span_avx2(std::uint32_t* dst, std::size_t count, const ge::opt::FloorSpanParams* params);
std::uint32_t ge_sprite_span_sse2(std::uint32_t* dst, std::size_t count, const ge::opt::SpriteSpanParams* params);
std::uint32_t ge_sprite_span_avx2(std::uint32_t* dst, std::size_t count, const ge::opt::SpriteSpanParams* params);
void ge_tint_sse2(std::uint32_t* px, std::size_t count, const ge::opt::TintParams* params);
void ge_tint_avx2(std::uint32_t* px, std::size_t count, const ge::opt::TintParams* params);
}

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

// ---- span kernels (same run-time kernel choice as fog_blend: set_fog_kernel / --fog-kernel force one) ----
// All of them give bit-identical results on every kernel; see ScalarFallback.hpp for the exact definitions.

// One screen row of floor or ceiling; see scalar::floor_span. |position| must stay below 2^30.
void floor_span(std::uint32_t* dst, std::size_t count, const FloorSpanParams& params) noexcept;

// One row of a sprite; see scalar::sprite_span. `params.row` is the image row, `params.ix` the texel column
// of each of the `count` pixels. Returns true if any pixel was written.
bool sprite_span(std::uint32_t* dst, std::size_t count, const SpriteSpanParams& params) noexcept;

// In-place per-channel scale-and-add over `count` pixels; see scalar::tint_u32.
void tint(std::uint32_t* px, std::size_t count, const TintParams& params) noexcept;

}  // namespace ge::opt
