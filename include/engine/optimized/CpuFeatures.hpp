#pragma once
// Runtime CPU feature detection for choosing between assembly kernels.

namespace ge {

struct CpuFeatures {
    bool x86_64 = false;  // false on other architectures: no NASM kernel can run
    bool sse2 = false;    // baseline on every x86-64 CPU
    bool avx2 = false;    // CPU supports it AND the OS saves the YMM registers
};

// Detected once and cached; thread-safe.
const CpuFeatures& cpu_features() noexcept;

}  // namespace ge
