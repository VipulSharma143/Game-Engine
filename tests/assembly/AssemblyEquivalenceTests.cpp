#include <string>
#include <cstdio>
#include <cstdint>
#include <cstddef>
#include <limits>
#include <vector>
#include "engine/optimized/CpuFeatures.hpp"

#include "TestFramework.hpp"
#include "engine/optimized/OptimizedRoutines.hpp"
#include "engine/optimized/ScalarFallback.hpp"

namespace {
std::vector<std::int64_t> sample_values() {
    constexpr std::int64_t mn = std::numeric_limits<std::int64_t>::min();
    constexpr std::int64_t mx = std::numeric_limits<std::int64_t>::max();
    std::vector<std::int64_t> v = {0, 1, -1, 2, -2, 3, 255, 256, 65535, 1LL << 31, -(1LL << 31),
                                   1LL << 32, 1LL << 62, mx, mx - 1, mn, mn + 1, 1000000007LL, -999999937LL};
    std::uint64_t state = 0x9E3779B97F4A7C15ull;  // deterministic xorshift
    for (int i = 0; i < 200; ++i) {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        v.push_back(static_cast<std::int64_t>(state));
    }
    return v;
}
}  // namespace

TEST(report_assembly_mode) {
    std::printf("      assembly enabled: %s\n", ge::opt::asm_enabled() ? "yes (NASM)" : "no (C++ fallback)");
}

TEST(dispatched_add_matches_scalar_reference) {
    const auto values = sample_values();
    for (std::int64_t a : values)
        for (std::int64_t b : values) CHECK_EQ(ge::opt::add_i64(a, b), ge::scalar::add_i64(a, b));
}

TEST(dispatched_multiply_matches_scalar_reference) {
    const auto values = sample_values();
    for (std::int64_t a : values)
        for (std::int64_t b : values) CHECK_EQ(ge::opt::multiply_i64(a, b), ge::scalar::multiply_i64(a, b));
}

TEST(scalar_reference_known_values) {
    CHECK_EQ(ge::scalar::add_i64(2, 3), 5);
    CHECK_EQ(ge::scalar::add_i64(std::numeric_limits<std::int64_t>::max(), 1), std::numeric_limits<std::int64_t>::min());
    CHECK_EQ(ge::scalar::multiply_i64(-4, 6), -24);
    CHECK_EQ(ge::scalar::multiply_i64(1LL << 62, 4), 0);
    CHECK_EQ(ge::opt::add_i64(40, 2), 42);
    CHECK_EQ(ge::opt::multiply_i64(7, -6), -42);
}

TEST(routines_preserve_callee_state_across_many_calls) {
    // Interleave calls with live local state to catch clobbered callee-saved registers.
    std::int64_t acc = 1, other = 7, third = 11;
    for (int i = 0; i < 10000; ++i) {
        acc = ge::opt::add_i64(acc, i);
        other = ge::opt::multiply_i64(other, 3);
        third = ge::opt::add_i64(third, other);
    }
    std::int64_t racc = 1, rother = 7, rthird = 11;
    for (int i = 0; i < 10000; ++i) {
        racc = ge::scalar::add_i64(racc, i);
        rother = ge::scalar::multiply_i64(rother, 3);
        rthird = ge::scalar::add_i64(rthird, rother);
    }
    CHECK_EQ(acc, racc);
    CHECK_EQ(other, rother);
    CHECK_EQ(third, rthird);
}

namespace {
constexpr std::uint32_t kGuard = 0xDEADBEEFu;

std::vector<std::uint32_t> random_pixels(std::size_t n, std::uint64_t seed) {
    std::vector<std::uint32_t> v(n);
    for (auto& p : v) {
        seed ^= seed << 13;
        seed ^= seed >> 7;
        seed ^= seed << 17;
        p = static_cast<std::uint32_t>(seed);
    }
    return v;
}
}  // namespace

TEST(fill_u32_matches_scalar_and_stays_in_bounds) {
    // Every count 0..70 and every start offset 0..4 (unaligned), with guard words on both sides.
    for (std::size_t offset = 0; offset < 5; ++offset) {
        for (std::size_t count = 0; count <= 70; ++count) {
            std::vector<std::uint32_t> a(count + 16, kGuard), b(count + 16, kGuard);
            ge::opt::fill_u32(a.data() + 8 + offset % 4, 0x12345678u, count);
            ge::scalar::fill_u32(b.data() + 8 + offset % 4, 0x12345678u, count);
            CHECK(a == b);
        }
    }
    std::uint32_t one = 0;
    ge::opt::fill_u32(&one, 7u, 0);  // zero count writes nothing
    CHECK_EQ(one, 0u);
}

TEST(fill_u32_large_buffer) {
    std::vector<std::uint32_t> a(100003, 1), b(100003, 1);
    ge::opt::fill_u32(a.data() + 1, 0xFF00FF00u, 100001);
    ge::scalar::fill_u32(b.data() + 1, 0xFF00FF00u, 100001);
    CHECK(a == b);
    CHECK_EQ(a.front(), 1u);
    CHECK_EQ(a.back(), 1u);
}

TEST(fill_column_matches_scalar_and_touches_only_its_pixels) {
    for (std::size_t stride : {1u, 2u, 3u, 7u, 64u, 640u}) {
        for (std::size_t count = 0; count <= 40; ++count) {
            const std::size_t size = stride * (count + 2) + 4;
            std::vector<std::uint32_t> a(size, kGuard), b(size, kGuard);
            ge::opt::fill_column_u32(a.data() + 2, stride, 0xCAFEF00Du, count);
            ge::scalar::fill_column_u32(b.data() + 2, stride, 0xCAFEF00Du, count);
            CHECK(a == b);
        }
    }
}

TEST(fnv1a64_matches_scalar_for_many_lengths_and_seeds) {
    for (std::size_t n = 0; n <= 64; ++n) {
        const auto px = random_pixels(n, 0x1234567ull + n);
        for (std::uint64_t seed : std::vector<std::uint64_t>{ge::scalar::kFnvOffsetBasis, 0ull, ~0ull, 42ull})
            CHECK_EQ(ge::opt::fnv1a64_u32(px.data(), n, seed), ge::scalar::fnv1a64_u32(px.data(), n, seed));
    }
    const auto big = random_pixels(64 * 48, 99);
    CHECK_EQ(ge::opt::fnv1a64_u32(big.data(), big.size(), ge::scalar::kFnvOffsetBasis),
             ge::scalar::fnv1a64_u32(big.data(), big.size(), ge::scalar::kFnvOffsetBasis));
}

TEST(fnv1a64_known_vector_and_chaining) {
    // FNV-1a 64 of the bytes 00 00 00 00 (one zero pixel), computed independently.
    std::uint64_t h = ge::scalar::kFnvOffsetBasis;
    for (int i = 0; i < 4; ++i) h = (h ^ 0u) * 1099511628211ull;
    const std::uint32_t zero = 0;
    CHECK_EQ(ge::opt::fnv1a64_u32(&zero, 1, ge::scalar::kFnvOffsetBasis), h);
    // Hashing in two parts equals hashing in one.
    const auto px = random_pixels(37, 5);
    const auto whole = ge::opt::fnv1a64_u32(px.data(), 37, ge::scalar::kFnvOffsetBasis);
    const auto part = ge::opt::fnv1a64_u32(px.data() + 10, 27, ge::opt::fnv1a64_u32(px.data(), 10, ge::scalar::kFnvOffsetBasis));
    CHECK_EQ(whole, part);
}

TEST(pixel_routines_preserve_caller_state) {
    // Live locals across many calls expose clobbered callee-saved registers.
    std::vector<std::uint32_t> buf(257, 0);
    std::uint64_t h = ge::scalar::kFnvOffsetBasis, rh = h;
    std::size_t a = 3, b = 5;
    for (int i = 0; i < 2000; ++i) {
        ge::opt::fill_u32(buf.data(), static_cast<std::uint32_t>(i), buf.size());
        ge::opt::fill_column_u32(buf.data(), 3, static_cast<std::uint32_t>(i * 3), 80);
        h = ge::opt::fnv1a64_u32(buf.data(), buf.size(), h);
        rh = ge::scalar::fnv1a64_u32(buf.data(), buf.size(), rh);
        a += 1;
        b += a;
    }
    CHECK_EQ(h, rh);
    CHECK_EQ(a, 2003u);
    CHECK_EQ(b, 5u + 2000u * 3u + (2000u * 2001u) / 2u);  // 5 + sum of (3 + i) for i = 1..2000
}

#include <cstring>

namespace {
bool bitwise_equal(const std::vector<double>& a, const std::vector<double>& b) {
    return a.size() == b.size() && std::memcmp(a.data(), b.data(), a.size() * sizeof(double)) == 0;
}
}  // namespace

TEST(vec2_madd_matches_scalar_for_all_lengths_and_alignments) {
    std::uint64_t state = 0x1234567ull;
    auto next = [&state] {
        state ^= state << 13; state ^= state >> 7; state ^= state << 17;
        return static_cast<double>(static_cast<std::int64_t>(state % 2000001) - 1000000) / 977.0;
    };
    for (std::size_t count = 0; count <= 37; ++count) {
        for (std::size_t offset = 0; offset < 2; ++offset) {  // offset 1 => 8-byte aligned only
            std::vector<double> d(count + 2), s(count + 2), expect;
            for (auto& v : d) v = next();
            for (auto& v : s) v = next();
            expect = d;
            const double k = next();
            ge::scalar::vec2_madd(expect.data() + offset, s.data() + offset, k, count);
            ge::opt::vec2_madd(d.data() + offset, s.data() + offset, k, count);
            CHECK(bitwise_equal(d, expect));
        }
    }
}

TEST(vec2_madd_known_values_and_untouched_tail) {
    std::vector<double> dst = {1, 2, 3, 99};
    const std::vector<double> src = {10, 20, 30, 40};
    ge::opt::vec2_madd(dst.data(), src.data(), 0.5, 3);
    CHECK_NEAR(dst[0], 6.0, 0.0);
    CHECK_NEAR(dst[1], 12.0, 0.0);
    CHECK_NEAR(dst[2], 18.0, 0.0);
    CHECK_NEAR(dst[3], 99.0, 0.0);  // element past `count` must not be written
}

// ---- fog blend ------------------------------------------------------------------------------------
namespace {

std::vector<std::uint32_t> random_pixels(std::size_t n, std::uint64_t seed, bool opaque) {
    std::vector<std::uint32_t> v(n);
    std::uint64_t s = seed * 0x9E3779B97F4A7C15ull + 1;
    for (auto& p : v) {
        s ^= s << 13; s ^= s >> 7; s ^= s << 17;
        p = static_cast<std::uint32_t>(s >> 11);
        if (opaque) p |= 0xFF000000u;
    }
    return v;
}

// Restores the active kernel when a test ends, however it ends.
struct KernelGuard {
    ge::opt::FogKernel saved = ge::opt::active_fog_kernel();
    ~KernelGuard() { ge::opt::set_fog_kernel(saved); }
};

const std::uint32_t kLights[] = {0, 1, 2, 37, 100, 128, 200, 255, 256};
const std::uint32_t kFogs[] = {0xFF080A12u, 0x00000000u, 0xFFFFFFFFu, 0x80FF0033u};

}  // namespace

TEST(fast_scalar_fog_blend_matches_the_per_channel_reference) {
    for (std::size_t n : {0u, 1u, 3u, 8u, 33u, 64u}) {
        const auto src = random_pixels(n, n + 7, false);
        for (std::uint32_t light : kLights)
            for (std::uint32_t fog : kFogs) {
                std::vector<std::uint32_t> a(n), b(n);
                ge::scalar::fog_blend(a.data(), src.data(), n, light, fog);
                ge::scalar::fog_blend_reference(b.data(), src.data(), n, light, fog);
                CHECK(a == b);
            }
    }
}

TEST(fog_blend_extremes_and_alpha) {
    const auto src = random_pixels(40, 3, true);
    std::vector<std::uint32_t> out(40);
    ge::scalar::fog_blend(out.data(), src.data(), 40, 256, 0xFF080A12u);
    CHECK(out == src);                                  // full light: unchanged
    ge::scalar::fog_blend(out.data(), src.data(), 40, 0, 0xFF080A12u);
    for (std::uint32_t p : out) CHECK_EQ(p, 0xFF080A12u);  // no light: pure fog colour
    ge::scalar::fog_blend(out.data(), src.data(), 40, 77, 0xFF080A12u);
    for (std::uint32_t p : out) CHECK_EQ(p >> 24, 0xFFu);  // opaque stays opaque
}

TEST(every_available_fog_kernel_matches_the_reference_exactly) {
    KernelGuard guard;
    int tested = 0;
    for (ge::opt::FogKernel k : {ge::opt::FogKernel::Scalar, ge::opt::FogKernel::Sse2, ge::opt::FogKernel::Avx2}) {
        if (!ge::opt::set_fog_kernel(k)) continue;
        ++tested;
        for (std::size_t n = 0; n <= 70; ++n) {
            for (std::size_t offset = 0; offset < 4; ++offset) {      // unaligned starts (4-byte steps)
                const auto src_all = random_pixels(n + 8, n * 31 + offset, false);
                for (std::uint32_t light : kLights) {
                    const std::uint32_t fog = kFogs[(n + light) % 4];
                    // out of place, with guard words on both sides that must not change
                    std::vector<std::uint32_t> dst(n + 8, 0xDEADBEEFu), expect(n + 8, 0xDEADBEEFu);
                    ge::scalar::fog_blend_reference(expect.data() + offset, src_all.data() + offset, n, light, fog);
                    ge::opt::fog_blend(dst.data() + offset, src_all.data() + offset, n, light, fog);
                    CHECK(dst == expect);
                    // in place
                    std::vector<std::uint32_t> inplace = src_all;
                    ge::opt::fog_blend(inplace.data() + offset, inplace.data() + offset, n, light, fog);
                    std::vector<std::uint32_t> expect_inplace = src_all;
                    ge::scalar::fog_blend_reference(expect_inplace.data() + offset, expect_inplace.data() + offset, n, light, fog);
                    CHECK(inplace == expect_inplace);
                }
            }
        }
    }
    // Every kernel this CPU supports must really have been exercised, not silently skipped.
    int expected = 1;
#if defined(GE_HAVE_ASM)
    expected += (ge::cpu_features().sse2 ? 1 : 0) + (ge::cpu_features().avx2 ? 1 : 0);
#endif
    CHECK_EQ(tested, expected);
}

TEST(cpu_feature_detection_is_consistent_with_kernel_availability) {
    const ge::CpuFeatures& f = ge::cpu_features();
    if (f.avx2) CHECK(f.sse2);
    if (f.sse2) CHECK(f.x86_64);
    CHECK(ge::opt::fog_kernel_available(ge::opt::FogKernel::Scalar));
#if defined(GE_HAVE_ASM)
    CHECK_EQ(ge::opt::fog_kernel_available(ge::opt::FogKernel::Avx2), f.avx2);
    CHECK_EQ(ge::opt::fog_kernel_available(ge::opt::FogKernel::Sse2), f.sse2);
#else
    CHECK(!ge::opt::fog_kernel_available(ge::opt::FogKernel::Sse2));
    CHECK(!ge::opt::fog_kernel_available(ge::opt::FogKernel::Avx2));
#endif
    KernelGuard guard;
    const ge::opt::FogKernel best = ge::opt::best_fog_kernel();
    CHECK(ge::opt::fog_kernel_available(best));
    CHECK(ge::opt::set_fog_kernel(best));
    CHECK(ge::opt::active_fog_kernel() == best);
    for (ge::opt::FogKernel k : {ge::opt::FogKernel::Scalar, ge::opt::FogKernel::Sse2, ge::opt::FogKernel::Avx2}) {
        CHECK(std::string(ge::opt::fog_kernel_name(k)).size() > 3u);
        if (!ge::opt::fog_kernel_available(k)) {
            const ge::opt::FogKernel before = ge::opt::active_fog_kernel();
            CHECK(!ge::opt::set_fog_kernel(k));                  // refused...
            CHECK(ge::opt::active_fog_kernel() == before);       // ...and nothing changed
        }
    }
}
