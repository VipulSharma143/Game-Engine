// Floor-row, sprite-row and tint kernels: every kernel this CPU supports must match the scalar
// reference bit for bit, for all lengths, unaligned destinations, and without touching memory
// outside the destination range.
#include <cmath>
#include <cstdint>
#include <vector>

#include "TestFramework.hpp"
#include "engine/optimized/CpuFeatures.hpp"
#include "engine/optimized/OptimizedRoutines.hpp"
#include "engine/optimized/ScalarFallback.hpp"

namespace {

using ge::opt::FogKernel;

struct KernelGuard {
    FogKernel saved = ge::opt::active_fog_kernel();
    ~KernelGuard() { ge::opt::set_fog_kernel(saved); }
};

struct Rng {
    std::uint64_t s;
    explicit Rng(std::uint64_t seed) : s(seed * 0x9E3779B97F4A7C15ull + 1) {}
    std::uint32_t next() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return static_cast<std::uint32_t>(s >> 11); }
    double unit() { return static_cast<double>(next() % 1000001u) / 1000000.0; }
    double range(double lo, double hi) { return lo + (hi - lo) * unit(); }
};

std::vector<std::uint32_t> random_texels(std::size_t n, std::uint64_t seed) {
    Rng r(seed);
    std::vector<std::uint32_t> v(n);
    for (auto& t : v) t = r.next() ^ (r.next() << 8);
    return v;
}

int available_kernel_count() {
    int expected = 1;
#if defined(GE_HAVE_ASM)
    expected += (ge::cpu_features().sse2 ? 1 : 0) + (ge::cpu_features().avx2 ? 1 : 0);
#endif
    return expected;
}

const FogKernel kAllKernels[] = {FogKernel::Scalar, FogKernel::Sse2, FogKernel::Avx2};

}  // namespace

// ---- floor_span ------------------------------------------------------------------------------

TEST(scalar_floor_span_agrees_with_a_plain_std_floor_implementation) {
    const auto texels = random_texels(8192, 1);
    Rng r(5);
    for (int trial = 0; trial < 200; ++trial) {
        const double fx = r.range(-40.0, 40.0), fy = r.range(-40.0, 40.0);
        const double sx = r.range(-1.5, 1.5), sy = r.range(-1.5, 1.5);
        const std::uint32_t parity = r.next() & 1u;
        std::vector<std::uint32_t> got(37);
        ge::scalar::floor_span(got.data(), got.size(), texels.data(), fx, fy, sx, sy, parity);
        for (std::size_t i = 0; i < got.size(); ++i) {
            const double x = fx + static_cast<double>(i) * sx, y = fy + static_cast<double>(i) * sy;
            const double cx = std::floor(x), cy = std::floor(y);
            const int tx = static_cast<int>((x - cx) * 64.0) & 63, ty = static_cast<int>((y - cy) * 64.0) & 63;
            const int variant = (static_cast<int>(cx) + static_cast<int>(cy)) & static_cast<int>(parity);
            CHECK_EQ(got[i], texels[static_cast<std::size_t>(variant * 4096 + ty * 64 + tx)]);
        }
    }
}

TEST(every_available_kernel_floor_span_matches_the_scalar_reference) {
    KernelGuard guard;
    int tested = 0;
    const auto texels = random_texels(8192, 2);
    for (FogKernel k : kAllKernels) {
        if (!ge::opt::set_fog_kernel(k)) continue;
        ++tested;
        Rng r(11);
        for (std::size_t n = 0; n <= 70; ++n) {
            for (std::size_t offset = 0; offset < 4; ++offset) {
                for (int variant = 0; variant < 6; ++variant) {
                    double fx = r.range(-30.0, 30.0), fy = r.range(-30.0, 30.0);
                    double sx = r.range(-1.2, 1.2), sy = r.range(-1.2, 1.2);
                    if (variant == 1) { fx = 3.0; fy = -2.0; sx = 0.25; sy = -0.5; }        // lands exactly on texel and cell borders
                    if (variant == 2) { fx = -1e-20; fy = -1e-20; sx = 0.0; sy = 0.0; }      // tiny negatives: fraction rounds to 1.0
                    if (variant == 3) { fx = 1.0e6 + 0.3; fy = -1.0e6 - 0.7; sx = 0.01; sy = -0.01; }
                    if (variant == 4) { fx = 0.0; fy = 0.0; sx = 1.0 / 64.0; sy = 1.0 / 32.0; }  // many exact fractions
                    const std::uint32_t parity = (variant == 5) ? 0u : (r.next() & 1u);
                    const auto params = ge::opt::make_floor_span_params(texels.data(), fx, fy, sx, sy, parity);
                    std::vector<std::uint32_t> got(n + 8, 0xDEADBEEFu), want(n + 8, 0xDEADBEEFu);
                    ge::scalar::floor_span(want.data() + offset, n, texels.data(), fx, fy, sx, sy, parity);
                    ge::opt::floor_span(got.data() + offset, n, params);
                    CHECK(got == want);  // includes the untouched guard words on both sides
                }
            }
        }
    }
    CHECK_EQ(tested, available_kernel_count());
}

// ---- sprite_span -----------------------------------------------------------------------------

TEST(shade_u32_known_values) {
    CHECK_EQ(ge::scalar::shade_u32(0x80102030u, 256, 0), 0xFF102030u);           // unchanged, made opaque
    CHECK_EQ(ge::scalar::shade_u32(0xFFFFFFFFu, 0, 0), 0xFF000000u);             // no light: black
    CHECK_EQ(ge::scalar::shade_u32(0xFF102030u, 256, 256), 0xFFFFFFFFu);         // full flash: white
    CHECK_EQ(ge::scalar::shade_u32(0xFF804020u, 128, 0), 0xFF402010u);           // half light
    CHECK_EQ(ge::scalar::shade_u32(0xFF808080u, 1023, 0), 0xFFFFFFFFu);          // brighter than white saturates
    CHECK_EQ(ge::scalar::shade_u32(0xFF000000u, 256, 128), 0xFF7F7F7Fu);         // half way to white
}

TEST(every_available_kernel_sprite_span_matches_the_scalar_reference) {
    KernelGuard guard;
    int tested = 0;
    constexpr std::size_t kRowWidth = 37;
    for (FogKernel k : kAllKernels) {
        if (!ge::opt::set_fog_kernel(k)) continue;
        ++tested;
        Rng r(21);
        for (std::size_t n = 0; n <= 70; ++n) {
            for (std::size_t offset = 0; offset < 4; ++offset) {
                for (int variant = 0; variant < 5; ++variant) {
                    std::vector<std::uint32_t> row = random_texels(kRowWidth, n * 13 + static_cast<std::size_t>(variant));
                    if (variant == 3) for (auto& t : row) t |= 0xFF000000u;            // all opaque
                    if (variant == 4) for (auto& t : row) t &= 0x7FFFFFFFu;            // all transparent
                    std::vector<std::int32_t> ix(n);                                    // exactly n entries: no over-read allowed
                    for (auto& v : ix) v = (r.next() % 4u == 0u) ? -1 - static_cast<std::int32_t>(r.next() % 5u)
                                                                  : static_cast<std::int32_t>(r.next() % kRowWidth);
                    const std::uint32_t light = variant == 0 ? 256u : r.next() % 1024u;
                    const std::uint32_t flash = variant == 1 ? 0u : r.next() % 257u;
                    const auto before = random_texels(n + 8, n * 7 + 1);
                    std::vector<std::uint32_t> got = before, want = before;
                    bool want_any = ge::scalar::sprite_span(want.data() + offset, n, row.data(), ix.data(), light, flash);
                    auto params = ge::opt::make_sprite_span_params(light, flash, ix.data());
                    params.row = row.data();
                    const bool got_any = ge::opt::sprite_span(got.data() + offset, n, params);
                    CHECK(got == want);
                    CHECK_EQ(got_any, want_any);
                }
            }
        }
    }
    CHECK_EQ(tested, available_kernel_count());
}

TEST(sprite_span_leaves_skipped_pixels_and_reports_nothing_drawn) {
    KernelGuard guard;
    for (FogKernel k : kAllKernels) {
        if (!ge::opt::set_fog_kernel(k)) continue;
        std::vector<std::uint32_t> row(4, 0x10FFFFFFu);  // alpha 16: transparent
        std::vector<std::int32_t> ix = {0, 1, -1, 2, 3, -1, 0, 1, 2, 3};
        std::vector<std::uint32_t> dst(ix.size(), 0x12345678u);
        auto params = ge::opt::make_sprite_span_params(256, 0, ix.data());
        params.row = row.data();
        CHECK(!ge::opt::sprite_span(dst.data(), dst.size(), params));
        for (std::uint32_t p : dst) CHECK_EQ(p, 0x12345678u);
        row[2] = 0x80336699u;  // alpha exactly 128 counts as opaque
        CHECK(ge::opt::sprite_span(dst.data(), dst.size(), params));
        CHECK_EQ(dst[3], 0xFF336699u);
        CHECK_EQ(dst[8], 0xFF336699u);
        CHECK_EQ(dst[2], 0x12345678u);  // ix < 0
        CHECK_EQ(dst[0], 0x12345678u);  // transparent texel
    }
}

// ---- tint ------------------------------------------------------------------------------------

TEST(tint_known_values) {
    const std::uint16_t mul[4] = {256, 128, 256, 256};  // b, g, r, a
    const std::uint16_t add[4] = {0, 0, 100, 0};
    std::uint32_t px[3] = {0xFF2040C0u, 0x80FFFFFFu, 0x00000000u};
    ge::scalar::tint_u32(px, 3, mul, add);
    CHECK_EQ(px[0], 0xFF8420C0u);  // r 0x20 + 100 = 0x84, g 0x40 halved, b kept
    CHECK_EQ(px[1], 0x80FF7FFFu);  // r saturates, alpha kept
    CHECK_EQ(px[2], 0x00640000u);
}

TEST(every_available_kernel_tint_matches_the_scalar_reference) {
    KernelGuard guard;
    int tested = 0;
    for (FogKernel k : kAllKernels) {
        if (!ge::opt::set_fog_kernel(k)) continue;
        ++tested;
        Rng r(31);
        for (std::size_t n = 0; n <= 70; ++n) {
            for (std::size_t offset = 0; offset < 4; ++offset) {
                for (int variant = 0; variant < 4; ++variant) {
                    unsigned m[3] = {r.next() % 257u, r.next() % 257u, r.next() % 257u};
                    unsigned a[3] = {r.next() % 256u, r.next() % 256u, r.next() % 256u};
                    if (variant == 1) { m[0] = m[1] = m[2] = 256; a[0] = a[1] = a[2] = 0; }   // identity
                    if (variant == 2) { m[0] = m[1] = m[2] = 0; a[0] = a[1] = a[2] = 255; }   // solid white
                    if (variant == 3) { m[0] = 256; m[1] = m[2] = 0; a[0] = 255; a[1] = a[2] = 0; }
                    const auto params = ge::opt::make_tint_params(m[0], m[1], m[2], a[0], a[1], a[2]);
                    const auto src = random_texels(n + 8, n * 3 + static_cast<std::size_t>(variant));
                    std::vector<std::uint32_t> got = src, want = src;
                    ge::scalar::tint_u32(want.data() + offset, n, params.mul16, params.add16);
                    ge::opt::tint(got.data() + offset, n, params);
                    CHECK(got == want);
                    if (variant == 1) CHECK(got == src);  // identity really is identity
                }
            }
        }
    }
    CHECK_EQ(tested, available_kernel_count());
}

TEST(span_kernels_preserve_callee_saved_state) {
    // Live local state across many calls: a clobbered callee-saved register would corrupt it.
    KernelGuard guard;
    const auto texels = random_texels(8192, 3);
    std::vector<std::uint32_t> row(64, 0xFF405060u), dst(64);
    std::vector<std::int32_t> ix(64, 5);
    for (FogKernel k : kAllKernels) {
        if (!ge::opt::set_fog_kernel(k)) continue;
        std::uint64_t a = 1, b = 7, c = 11, d = 13, e = 17, f = 19;
        for (int i = 0; i < 2000; ++i) {
            const auto fp = ge::opt::make_floor_span_params(texels.data(), 0.1 * i, 0.2 * i, 0.01, 0.02, 1);
            ge::opt::floor_span(dst.data(), dst.size(), fp);
            auto sp = ge::opt::make_sprite_span_params(300, 40, ix.data());
            sp.row = row.data();
            ge::opt::sprite_span(dst.data(), 63, sp);
            ge::opt::tint(dst.data(), 61, ge::opt::make_tint_params(200, 256, 100, 3, 0, 9));
            a = a * 3 + static_cast<std::uint64_t>(i); b = b * 5 + a; c = c ^ (b << 1); d = d + c; e = e * 7 + d; f = f ^ e;
        }
        std::uint64_t ra = 1, rb = 7, rc = 11, rd = 13, re = 17, rf = 19;
        for (int i = 0; i < 2000; ++i) {
            ra = ra * 3 + static_cast<std::uint64_t>(i); rb = rb * 5 + ra; rc = rc ^ (rb << 1); rd = rd + rc; re = re * 7 + rd; rf = rf ^ re;
        }
        CHECK(a == ra && b == rb && c == rc && d == rd && e == re && f == rf);
    }
}
