#include "engine/optimized/CpuFeatures.hpp"

#include <cstdint>

#if defined(__x86_64__) || defined(_M_X64)
#define GE_X86_64 1
#if defined(_MSC_VER)
#include <immintrin.h>  // _xgetbv
#include <intrin.h>   // __cpuidex
#else
#include <cpuid.h>
#endif
#endif

namespace ge {
namespace {

#if defined(GE_X86_64)
void cpuid(std::uint32_t leaf, std::uint32_t sub, std::uint32_t out[4]) noexcept {
#if defined(_MSC_VER)
    int r[4];
    __cpuidex(r, static_cast<int>(leaf), static_cast<int>(sub));
    for (int i = 0; i < 4; ++i) out[i] = static_cast<std::uint32_t>(r[i]);
#else
    __cpuid_count(leaf, sub, out[0], out[1], out[2], out[3]);
#endif
}

std::uint64_t xgetbv0() noexcept {
#if defined(_MSC_VER)
    return _xgetbv(0);
#else
    std::uint32_t eax, edx;
    __asm__ volatile("xgetbv" : "=a"(eax), "=d"(edx) : "c"(0));
    return (static_cast<std::uint64_t>(edx) << 32) | eax;
#endif
}
#endif

CpuFeatures detect() noexcept {
    CpuFeatures f;
#if defined(GE_X86_64)
    f.x86_64 = true;
    f.sse2 = true;  // part of the x86-64 baseline
    std::uint32_t r[4] = {0, 0, 0, 0};
    cpuid(0, 0, r);
    const std::uint32_t max_leaf = r[0];
    if (max_leaf >= 7) {
        cpuid(1, 0, r);
        const bool osxsave = (r[2] >> 27) & 1u;
        const bool avx = (r[2] >> 28) & 1u;
        if (osxsave && avx && (xgetbv0() & 0x6u) == 0x6u) {  // OS saves XMM and YMM state
            cpuid(7, 0, r);
            f.avx2 = (r[1] >> 5) & 1u;
        }
    }
#endif
    return f;
}

}  // namespace

const CpuFeatures& cpu_features() noexcept {
    static const CpuFeatures features = detect();
    return features;
}

}  // namespace ge
