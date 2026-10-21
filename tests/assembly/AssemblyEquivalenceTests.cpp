#include <cstdio>
#include <cstdint>
#include <cstddef>
#include <limits>
#include <vector>

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
