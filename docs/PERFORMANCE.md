# Performance

Principles: no heap allocation in the frame path (buffers are sized once), no per-frame resource creation, debug logging compiled out under `NDEBUG`, no threads, no SIMD abstractions, no assembly without a measured benefit.

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

## Candidate optimisations (only if profiling justifies them)

Column-major or tiled framebuffer writes, cheaper shading, precomputed per-column ray directions.
