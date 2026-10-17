# Testing

Run: `ctest --test-dir build --output-on-failure`. A dependency-free harness lives in `tests/support` (`TEST`, `CHECK`, `CHECK_EQ`, `CHECK_NEAR`); each executable runs all its tests and exits non-zero on any failure. No display is required anywhere.

| CTest entry | Tests | Covers |
| --- | --- | --- |
| unit_core | 10 | config validation/CLI parsing, frame timer clamping, fixed stepper, logger, monotonic clock |
| unit_renderer | 14 | framebuffer, ray distances in all directions, camera, wall rendering, orientation (not mirrored), determinism |
| unit_world | 8 | grid map, level parser errors, built-in level, AABB collision, sliding, no tunnelling |
| unit_player | 8 | forward/strafe/turn, diagonal normalisation, wall blocking, randomised never-inside-wall check |
| platform_headless | 5 | scripted input, headless platform, PPM writer, ASCII writer |
| assembly_equivalence | 11 | `opt::` vs `scalar::` for add/multiply and the pixel routines (all counts 0-70, unaligned starts, guard-word overrun checks, hash chaining, caller-state preservation); reports whether NASM is enabled |
| integration_engine | 9 | init/shutdown, invalid config, app init failure, exact deterministic loop counts, quit, realtime loop, deterministic demo run |
| benchmark_smoke | - | runs `ge-bench` briefly; asserts nothing about timing |

Total: 65 test cases (plus the benchmark smoke run).

The headless path: `HeadlessPlatform` + `deterministic = true` runs the whole engine with scripted input and a virtual clock. Tests compare checksums between runs of the same build; they intentionally do not hard-code golden hashes, because floating-point results may differ between compilers.

Also verified manually (not in CTest): sanitizer build (`-DGE_SANITIZE="address;undefined"`), `GE_ASM=OFF`, `GE_ASM=ON` without NASM failing with a clear message.

CI: `.github/workflows/build-linux.yml` and `build-windows.yml` configure, build, run CTest and run the demo. Not yet executed on GitHub.
