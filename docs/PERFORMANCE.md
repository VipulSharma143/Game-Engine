# Performance

Principles: no heap allocation in the frame path (buffers are sized once), no per-frame resource creation, debug logging compiled out under `NDEBUG`, no threads, no SIMD abstraction layer, no assembly without a measured benefit (the fog kernel is the only routine so far that shows one).

## Benchmark

`ge-bench` (`tools/benchmark-runner`) renders the built-in 16x16 level from a fixed rotating-camera path and prints workload, build, compiler, method and timings. It is not a pass/fail test; CTest only smoke-runs it (`benchmark_smoke`, label `benchmark`).

```bash
./build/tools/benchmark-runner/ge-bench --width 640 --height 400 --frames 200 --warmup 20
```

Method: wall time per frame with `std::chrono::steady_clock` around `Renderer::render`, single thread, warm-up frames discarded, median/min/max reported.

## Recorded measurements (not the target machine)

Taken on 2026-10-17 in a 1-vCPU Intel Xeon @ 2.10 GHz container, GCC 13.3.0. Treat as a rough sanity check only; re-measure on the Linux Mint development machine before drawing conclusions. No FPS target is set.

| Workload | Build | Result |
| --- | --- | --- |
| 640x400, 200 frames | Release | median 0.592 ms, min 0.568, max 0.989 |
| 320x200, 500 frames | Release | median 0.074 ms, min 0.071, max 0.125 |
| 640x400, 50 frames | Debug (no optimisation flags) | median 0.719 ms |

Rendering only; excludes presentation, which is not implemented.

## Assembly versus C++

Measured 2026-10-17 in the same container (1 vCPU Xeon @ 2.10 GHz, GCC 13.3.0 Release, 640x400 buffer, median of 15 reps; three runs shown as ratio scalar/asm, >1 means asm faster):

| Routine | Ratio across 3 runs | Verdict |
| --- | --- | --- |
| `fill_u32` (256000 pixels) | 1.06, 1.01, 0.99 | parity (GCC `-O3` already vectorises the C++ loop) |
| `fill_column_u32` (400 px, stride 640) | 1.00, 1.04, 0.93 | parity (store bound) |
| `fnv1a64_u32` (256000 pixels) | 0.94, 0.90, 1.04 | parity/noise (limited by imul latency) |
| `add_i64` / `multiply_i64` | scalar faster (2.4 vs 1.5 ns/call) | tiny out-of-line calls; no benefit |

End to end (`Renderer::render`, 640x400, 300 frames, five alternating runs): median of medians 0.561 ms with NASM, 0.565 ms with `GE_ASM=OFF`, i.e. no measurable difference once the dispatchers were made inline. Before inlining the dispatchers, the NASM build was roughly 5-7% slower from the extra call per column.

So the assembly is correct and costs nothing, but it is **not faster** on this machine; its value is the verified two-ABI infrastructure for future routines (wider SIMD with a CPU feature check is the first place a gain is plausible). Re-measure on the Linux Mint machine.

## Fog shading: SSE2 and AVX2 versus C++ (textured renderer)

Every floor, ceiling and wall pixel of the textured renderer is blended towards the fog colour. The renderer gathers the raw texels of a row or column and shades them with one call to `opt::fog_blend`, which uses the C++ scalar kernel, the NASM SSE2 kernel or the NASM AVX2 kernel (chosen at run time by CPUID + XGETBV; `--fog-kernel scalar|sse2|avx2|auto` forces one). All three produce bit-identical pixels (checked by `assembly_equivalence`, `unit_texture`, and the benchmark itself, which exits non-zero if they differ).

Measured 2026-10-22 in the same kind of container (1 vCPU Xeon @ 2.10 GHz with AVX2, GCC 13.3.0 Release, `ge-bench --textured-only`, ratio scalar/asm, >1 = asm faster). The C++ baseline is the fast version (two channels per 32-bit multiply), not a naive loop:

| What | 320x200 (3 runs) | 640x400 (3 runs) |
| --- | --- | --- |
| Fog routine alone, one row, SSE2 | 2.3x - 2.4x | 1.3x - 2.7x |
| Fog routine alone, one row, AVX2 | 4.5x - 4.8x | 4.7x - 6.3x |
| Fog routine alone, one wall column, AVX2 | 3.4x - 3.5x | 4.4x - 5.9x |
| Whole textured frame, SSE2 | 0.94x - 1.13x (noise) | 0.79x - 1.07x (noise) |
| Whole textured frame, AVX2 | 1.12x - 1.14x (0.465 -> 0.41 ms) | 0.84x - 1.11x (noise) |

Reading: the AVX2 kernel is clearly faster than the C++ one in isolation, but fog is a small slice of a frame (about 65 us of 465 us at 320x200). Most of the remaining time is per-pixel texture-coordinate maths and texel fetches, which were **not** moved to assembly: a gather is not a win without AVX2 gather support, and a fixed-point C++ rewrite would change the pixels. On this machine the whole-frame gain is about 10% at 320x200 and not distinguishable from noise at 640x400. SSE2 is not worth its place on a CPU that has AVX2, but is kept as the fallback for older x86-64 CPUs. Most of the speed-up from the earlier 0.61 ms came from restructuring (shade a whole row or column per call) and the faster C++ fallback, not from assembly. Re-measure on the Linux Mint machine.

The earlier routines (`fill_u32`, `fill_column_u32`, `fnv1a64_u32`) measure at 1.00x, 0.75x and 1.03x on the same run: no gain, kept as tested infrastructure.

## Floor rows, sprite rows and tint: SSE2 and AVX2 versus C++ (0.5.0)

Profiling the textured frame showed that the floor and ceiling pass, not walls or fog, was about 80% of it (0.37 of 0.45 ms at 320x200): two `floor` calls and a texel fetch per pixel. That pass, the sprite pixel loop and the full-screen damage/blast tints are now kernels (`span.asm`, `tint.asm`), all bit-identical to their C++ references (`assembly_span_kernels`, and `unit_texture` renders whole frames with every kernel and compares checksums).

Measured 2026-10-31, same kind of container (1 vCPU Xeon with AVX2, GCC 13.3 Release), `ge-bench --textured-only` (it prints the same table), ratio C++/asm, >1 = asm faster:

| What | SSE2 | AVX2 |
| --- | --- | --- |
| floor row, 320 px (1540-1620 ns in C++) | 1.4x - 1.5x | 4.0x - 4.3x |
| sprite row, 320 px (950-970 ns in C++) | 2.4x | 5.4x - 5.6x |
| full-screen tint, 320x200 (the C++ reference is a generic per-channel loop, not the old hand-written one) | 9x | 16x - 17x |
| whole textured frame, 320x200 | 1.1x - 1.4x | 2.1x - 2.3x (0.45 ms before this change, 0.22 ms after) |
| whole textured frame, 640x400 | about 1.3x | about 2.2x (1.7-2.2 ms before, 0.8-0.9 ms after; runs on this machine vary a lot) |

Sprites, measured with a throwaway harness against the previous library (8 picture sprites 32x40, 60 sphere particles): picture sprites 108 us -> 17 us at 320x200 and 430 us -> 60 us at 640x400, mostly from hoisting the per-pixel `double` divisions into per-column and per-row tables, then the kernel. Sphere particles (still C++) did not change (43 us -> 43 us). The first picture-sprite figures are for the AVX2 build; the SSE2 sprite kernel is 2.4x over C++ in isolation.

Reading: the AVX2 floor kernel's advantage comes from `vpgatherdd` plus doing four positions per vector; the SSE2 floor kernel fetches texels with scalar loads and gains little over the C++ loop, so on a CPU without AVX2 expect roughly the old speed. Gather speed varies by CPU (slower on some AMD parts); re-measure on the Linux Mint machine. The no-NASM build is unchanged in output and runs the C++ kernels at its previous speed (the C++ floor loop is at parity with the old one).

Floor positions are now `start + i * step` instead of an accumulated sum. For the benchmark path (400 frames, many poses) every pixel came out identical to the previous renderer (same checksum); in general a sample that lies within about 1e-13 of a texel border could land in the neighbouring texel.

## Candidate optimisations (only if profiling justifies them)

Column-major or tiled framebuffer writes, cheaper shading, precomputed per-column ray directions.
