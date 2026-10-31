// Run-time dispatch for the span kernels (floor row, sprite row, tint). The kernel is the one chosen
// for fog_blend (see FogBlend.cpp), so `--fog-kernel scalar|sse2|avx2` forces all of them together.
#include "engine/optimized/OptimizedRoutines.hpp"

namespace ge::opt {

void floor_span(std::uint32_t* dst, std::size_t count, const FloorSpanParams& p) noexcept {
#if defined(GE_HAVE_ASM)
    switch (active_fog_kernel()) {
        case FogKernel::Avx2: ge_floor_span_avx2(dst, count, &p); return;
        case FogKernel::Sse2: ge_floor_span_sse2(dst, count, &p); return;
        case FogKernel::Scalar: break;
    }
#endif
    scalar::floor_span(dst, count, p.texels, p.fx[0], p.fy[0], p.sx[0], p.sy[0], p.parity[0]);
}

bool sprite_span(std::uint32_t* dst, std::size_t count, const SpriteSpanParams& p) noexcept {
#if defined(GE_HAVE_ASM)
    switch (active_fog_kernel()) {
        case FogKernel::Avx2: return ge_sprite_span_avx2(dst, count, &p) != 0;
        case FogKernel::Sse2: return ge_sprite_span_sse2(dst, count, &p) != 0;
        case FogKernel::Scalar: break;
    }
#endif
    return scalar::sprite_span(dst, count, p.row, p.ix, p.light16[0], p.flash16[0]);
}

void tint(std::uint32_t* px, std::size_t count, const TintParams& p) noexcept {
#if defined(GE_HAVE_ASM)
    switch (active_fog_kernel()) {
        case FogKernel::Avx2: ge_tint_avx2(px, count, &p); return;
        case FogKernel::Sse2: ge_tint_sse2(px, count, &p); return;
        case FogKernel::Scalar: break;
    }
#endif
    scalar::tint_u32(px, count, p.mul16, p.add16);  // the first four words are b, g, r, a
}

}  // namespace ge::opt
