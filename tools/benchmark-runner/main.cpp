// Raycaster benchmark. Renders the built-in test level from a fixed camera path
// and reports wall-clock time per frame. Timing is informational only.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "engine/core/Time.hpp"
#include "engine/optimized/OptimizedRoutines.hpp"
#include "engine/optimized/ScalarFallback.hpp"
#include "engine/optimized/OptimizedRoutines.hpp"
#include "engine/renderer/Renderer.hpp"
#include "engine/world/World.hpp"

namespace {

#if defined(NDEBUG)
constexpr const char* kBuild = "Release (NDEBUG)";
#else
constexpr const char* kBuild = "Debug (no NDEBUG)";
#endif

#if defined(__clang__)
constexpr const char* kCompiler = "clang " __clang_version__;
#elif defined(__GNUC__)
constexpr const char* kCompiler = "gcc " __VERSION__;
#elif defined(_MSC_VER)
constexpr const char* kCompiler = "msvc";
#else
constexpr const char* kCompiler = "unknown";
#endif

volatile std::int64_t g_sink = 0;

double median(std::vector<double> v) {
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

template <typename Fn>
double time_calls(Fn fn, long iterations) {
    const double start = ge::monotonic_seconds();
    for (long i = 0; i < iterations; ++i) g_sink = fn(i);
    return (ge::monotonic_seconds() - start) / static_cast<double>(iterations) * 1e9;
}

// Median ns per call of fn() over `reps` repetitions of `inner` calls.
template <typename Fn>
double bench_ns(Fn fn, int reps, int inner) {
    std::vector<double> samples;
    samples.reserve(static_cast<std::size_t>(reps));
    for (int r = 0; r < reps; ++r) {
        const double t0 = ge::monotonic_seconds();
        for (int i = 0; i < inner; ++i) fn(i);
        samples.push_back((ge::monotonic_seconds() - t0) / inner * 1e9);
    }
    return median(samples);
}

void report_pair(const char* name, double scalar_ns, double opt_ns) {
    std::printf("%-22s: scalar %10.1f ns, dispatched %10.1f ns, ratio %.2fx (%s)\n", name, scalar_ns, opt_ns,
                scalar_ns / opt_ns, ge::opt::asm_enabled() ? "NASM" : "C++ fallback");
}

void bench_pixel_routines(int width, int height) {
    const std::size_t n = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    std::vector<std::uint32_t> buf(n, 1u);
    const std::size_t stride = static_cast<std::size_t>(width);
    const std::size_t column = static_cast<std::size_t>(height);
    constexpr int kReps = 15;
    std::printf("pixel routines on a %dx%d buffer (%zu pixels), median of %d reps:\n", width, height, n, kReps);
    report_pair("fill_u32 (full buffer)",
                bench_ns([&](int i) { ge::scalar::fill_u32(buf.data(), static_cast<std::uint32_t>(i), n); g_sink = buf[0]; }, kReps, 200),
                bench_ns([&](int i) { ge::opt::fill_u32(buf.data(), static_cast<std::uint32_t>(i), n); g_sink = buf[0]; }, kReps, 200));
    report_pair("fill_column_u32",
                bench_ns([&](int i) { ge::scalar::fill_column_u32(buf.data() + (i & 63), stride, static_cast<std::uint32_t>(i), column); g_sink = buf[0]; }, kReps, 20000),
                bench_ns([&](int i) { ge::opt::fill_column_u32(buf.data() + (i & 63), stride, static_cast<std::uint32_t>(i), column); g_sink = buf[0]; }, kReps, 20000));
    std::uint64_t sink = 0;
    report_pair("fnv1a64_u32 (buffer)",
                bench_ns([&](int) { sink ^= ge::scalar::fnv1a64_u32(buf.data(), n, ge::scalar::kFnvOffsetBasis); }, kReps, 50),
                bench_ns([&](int) { sink ^= ge::opt::fnv1a64_u32(buf.data(), n, ge::scalar::kFnvOffsetBasis); }, kReps, 50));
    g_sink = static_cast<std::int64_t>(sink);
}

}  // namespace

int main(int argc, char** argv) {
    int width = 640, height = 400, frames = 200, warmup = 20;
    bool textured_only = false;
    for (int i = 1; i < argc; ++i) {
        const char* flag = argv[i];
        if (std::strcmp(flag, "--textured-only") == 0) { textured_only = true; continue; }
        if (i + 1 >= argc) { std::fprintf(stderr, "usage: ge-bench [--width N] [--height N] [--frames N] [--warmup N] [--textured-only]\n"); return 64; }
        const int v = std::atoi(argv[++i]);
        if (std::strcmp(flag, "--width") == 0) width = v;
        else if (std::strcmp(flag, "--height") == 0) height = v;
        else if (std::strcmp(flag, "--frames") == 0) frames = v;
        else if (std::strcmp(flag, "--warmup") == 0) warmup = v;
        else {
            std::fprintf(stderr, "usage: ge-bench [--width N] [--height N] [--frames N] [--warmup N] [--textured-only]\n");
            return 64;
        }
    }
    if (width < 16 || height < 16 || frames < 1 || warmup < 0) {
        std::fprintf(stderr, "invalid arguments\n");
        return 64;
    }

    const ge::Level& level = ge::test_level();
    ge::Framebuffer fb(width, height);
    ge::Renderer renderer;
    auto camera_at = [&](int frame) {
        // Fixed, repeatable sweep: full turn over 360 frames at the spawn point.
        return ge::Camera::from_angle(level.spawn, frame * 0.0174532925, 66.0);
    };

    struct RenderTiming { double median_ms, min_ms, max_ms; std::uint64_t checksum; };
    auto time_render = [&](ge::Renderer& r) {
        for (int i = 0; i < warmup; ++i) r.render(level.map, camera_at(i), fb);
        std::vector<double> ms;
        ms.reserve(static_cast<std::size_t>(frames));
        std::uint64_t sum = 0;
        for (int i = 0; i < frames; ++i) {
            const ge::Camera cam = camera_at(i);
            const double t0 = ge::monotonic_seconds();
            r.render(level.map, cam, fb);
            ms.push_back((ge::monotonic_seconds() - t0) * 1000.0);
            sum = sum * 1099511628211ull + fb.checksum();  // fold every frame so any pixel difference shows
        }
        return RenderTiming{median(ms), *std::min_element(ms.begin(), ms.end()), *std::max_element(ms.begin(), ms.end()), sum};
    };

    std::printf("workload : raycast test level (16x16), %dx%d, %d frames (+%d warmup), 66 deg fov, rotating camera\n",
                width, height, frames, warmup);
    std::printf("build    : %s, compiler %s\n", kBuild, kCompiler);
    std::printf("method   : per-frame wall time with steady_clock around Renderer::render, single thread\n");
    if (!textured_only) {
        const RenderTiming t = time_render(renderer);
        std::printf("render flat     : median %.3f ms, min %.3f ms, max %.3f ms (all-frames checksum %016llx)\n", t.median_ms,
                    t.min_ms, t.max_ms, static_cast<unsigned long long>(t.checksum));
    }
    const ge::TextureSet textures = ge::TextureSet::make_procedural();
    ge::Renderer textured_renderer;
    textured_renderer.set_textures(&textures);
    {
        const RenderTiming t = time_render(textured_renderer);
        std::printf("render textured : median %.3f ms, min %.3f ms, max %.3f ms (all-frames checksum %016llx)\n", t.median_ms,
                    t.min_ms, t.max_ms, static_cast<unsigned long long>(t.checksum));
    }
    // ---- every fog kernel, same frames: time and (must be) identical pixels ----------------------
    {
        const ge::opt::FogKernel original = ge::opt::active_fog_kernel();
        std::printf("\nfog kernels (CPU: avx2=%d, sse2=%d), textured render and the fog routine alone:\n",
                    ge::opt::fog_kernel_available(ge::opt::FogKernel::Avx2), ge::opt::fog_kernel_available(ge::opt::FogKernel::Sse2));
        double scalar_frame = 0.0, scalar_row = 0.0, scalar_col = 0.0;
        std::uint64_t reference = 0;
        std::vector<std::uint32_t> src_row(static_cast<std::size_t>(width)), dst_row(static_cast<std::size_t>(width));
        std::vector<std::uint32_t> src_col(static_cast<std::size_t>(height) / 2), dst_col(static_cast<std::size_t>(height) / 2);
        for (std::size_t i = 0; i < src_row.size(); ++i) src_row[i] = 0xFF000000u | static_cast<std::uint32_t>(i * 2654435761u);
        for (std::size_t i = 0; i < src_col.size(); ++i) src_col[i] = 0xFF000000u | static_cast<std::uint32_t>(i * 40503u);
        for (ge::opt::FogKernel k : {ge::opt::FogKernel::Scalar, ge::opt::FogKernel::Sse2, ge::opt::FogKernel::Avx2}) {
            if (!ge::opt::set_fog_kernel(k)) { std::printf("  %-11s: not available here\n", ge::opt::fog_kernel_name(k)); continue; }
            const RenderTiming t = time_render(textured_renderer);
            const double row_ns = bench_ns([&](int i) { ge::opt::fog_blend(dst_row.data(), src_row.data(), src_row.size(), 100 + static_cast<unsigned>(i & 63), 0xFF080A12u); g_sink = dst_row[1]; }, 15, 20000);
            const double col_ns = bench_ns([&](int i) { ge::opt::fog_blend(dst_col.data(), src_col.data(), src_col.size(), 100 + static_cast<unsigned>(i & 63), 0xFF080A12u); g_sink = dst_col[1]; }, 15, 20000);
            if (k == ge::opt::FogKernel::Scalar) { scalar_frame = t.median_ms; scalar_row = row_ns; scalar_col = col_ns; reference = t.checksum; }
            std::printf("  %-11s: frame %.3f ms (%.2fx), %zu-px row %7.1f ns (%.2fx), %zu-px column %6.1f ns (%.2fx), pixels %s\n",
                        ge::opt::fog_kernel_name(k), t.median_ms, scalar_frame / t.median_ms, src_row.size(), row_ns, scalar_row / row_ns,
                        src_col.size(), col_ns, scalar_col / col_ns, t.checksum == reference ? "identical" : "DIFFERENT");
            if (t.checksum != reference) { ge::opt::set_fog_kernel(original); return 1; }
        }
        ge::opt::set_fog_kernel(original);
    }
    if (textured_only) return 0;

    const long n = frames >= 100 ? 20000000L : 100000L;
    const double scalar_ns = time_calls([](long i) { return ge::scalar::add_i64(i, 3); }, n);
    const double dispatch_ns = time_calls([](long i) { return ge::opt::add_i64(i, 3); }, n);
    std::printf("add_i64  : scalar %.2f ns/call, dispatched (%s) %.2f ns/call over %ld calls\n", scalar_ns,
                ge::opt::asm_enabled() ? "NASM" : "C++ fallback", dispatch_ns, n);
    std::printf("\n");
    bench_pixel_routines(width, height);
    return 0;
}
