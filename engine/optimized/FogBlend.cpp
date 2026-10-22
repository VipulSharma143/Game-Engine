#include <atomic>

#include "engine/optimized/CpuFeatures.hpp"
#include "engine/optimized/OptimizedRoutines.hpp"
#include "engine/optimized/ScalarFallback.hpp"

namespace ge::opt {

namespace {
std::atomic<int> g_active{-1};  // -1 = not chosen yet
}

const char* fog_kernel_name(FogKernel k) noexcept {
    switch (k) {
        case FogKernel::Scalar: return "C++ scalar";
        case FogKernel::Sse2: return "NASM SSE2";
        case FogKernel::Avx2: return "NASM AVX2";
    }
    return "?";
}

bool fog_kernel_available(FogKernel k) noexcept {
    switch (k) {
        case FogKernel::Scalar: return true;
#if defined(GE_HAVE_ASM)
        case FogKernel::Sse2: return cpu_features().sse2;
        case FogKernel::Avx2: return cpu_features().avx2;
#else
        case FogKernel::Sse2:
        case FogKernel::Avx2: return false;
#endif
    }
    return false;
}

FogKernel best_fog_kernel() noexcept {
    if (fog_kernel_available(FogKernel::Avx2)) return FogKernel::Avx2;
    if (fog_kernel_available(FogKernel::Sse2)) return FogKernel::Sse2;
    return FogKernel::Scalar;
}

FogKernel active_fog_kernel() noexcept {
    int v = g_active.load(std::memory_order_relaxed);
    if (v < 0) {
        v = static_cast<int>(best_fog_kernel());
        g_active.store(v, std::memory_order_relaxed);
    }
    return static_cast<FogKernel>(v);
}

bool set_fog_kernel(FogKernel k) noexcept {
    if (!fog_kernel_available(k)) return false;
    g_active.store(static_cast<int>(k), std::memory_order_relaxed);
    return true;
}

void fog_blend(std::uint32_t* dst, const std::uint32_t* src, std::size_t count, std::uint32_t light,
               std::uint32_t fog_color) noexcept {
#if defined(GE_HAVE_ASM)
    const FogParams params{light, fog_color};
    switch (active_fog_kernel()) {
        case FogKernel::Avx2: ge_fog_blend_avx2(dst, src, count, &params); return;
        case FogKernel::Sse2: ge_fog_blend_sse2(dst, src, count, &params); return;
        case FogKernel::Scalar: break;
    }
#endif
    scalar::fog_blend(dst, src, count, light, fog_color);
}

}  // namespace ge::opt
