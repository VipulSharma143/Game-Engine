#include "engine/optimized/OptimizedRoutines.hpp"

#include "engine/optimized/ScalarFallback.hpp"

#if defined(GE_HAVE_ASM)
// Raw NASM entry points. Linux/System V uses rdi/rsi; Windows x64 uses rcx/rdx,
// so each platform has its own symbol names (see docs/ASSEMBLY_ABI.md).
extern "C" {
#if defined(_WIN32)
std::int64_t ge_windows_add_i64(std::int64_t a, std::int64_t b);
std::int64_t ge_windows_multiply_i64(std::int64_t a, std::int64_t b);
#else
std::int64_t ge_add_i64(std::int64_t a, std::int64_t b);
std::int64_t ge_linux_multiply_i64(std::int64_t a, std::int64_t b);
#endif
}
#endif

namespace ge::opt {

bool asm_enabled() noexcept {
#if defined(GE_HAVE_ASM)
    return true;
#else
    return false;
#endif
}

std::int64_t add_i64(std::int64_t a, std::int64_t b) noexcept {
#if defined(GE_HAVE_ASM) && defined(_WIN32)
    return ge_windows_add_i64(a, b);
#elif defined(GE_HAVE_ASM)
    return ge_add_i64(a, b);
#else
    return scalar::add_i64(a, b);
#endif
}

std::int64_t multiply_i64(std::int64_t a, std::int64_t b) noexcept {
#if defined(GE_HAVE_ASM) && defined(_WIN32)
    return ge_windows_multiply_i64(a, b);
#elif defined(GE_HAVE_ASM)
    return ge_linux_multiply_i64(a, b);
#else
    return scalar::multiply_i64(a, b);
#endif
}

}  // namespace ge::opt
