# Project Status

Last verified: 2026-10-17, Linux x86-64 (Ubuntu 24.04 container), GCC 13.3.0, CMake 3.28.3, NASM 2.16.01.

Statuses: **Tested** = implemented and covered by passing automated tests; **Implemented** = code exists and builds but is not covered by tests or was not run on every platform; **Partial** = usable but incomplete; **Planned** = not started.

## Components

| Component | Status | Notes |
| --- | --- | --- |
| CMake build, presets, options | Tested | Debug, Release, no-asm, Ninja and ASan+UBSan configurations built and tested on Linux/GCC |
| Core: config + CLI parsing, logger, frame timer, fixed-step loop, engine lifecycle | Tested | unit_core, integration_engine |
| Framebuffer, camera, DDA raycaster, wall renderer | Tested | unit_renderer (known distances, orientation, determinism) |
| Grid world, level parser, built-in test level | Tested | unit_world |
| Collision + movement with sliding | Tested | unit_world, unit_player (incl. 5000-step random-input fuzz) |
| Player controller, game application, demo tour | Tested | unit_player, integration_engine |
| Headless platform, scripted input, PPM/ASCII dump | Tested | platform_headless |
| NASM routines (add, multiply, fill, column fill, checksum) + C++ fallbacks + dispatch, used by `Framebuffer` | Tested (Linux) | assembly_equivalence (11 tests). Windows variants assemble and were inspected statically with `nasm -f win64`; **never run or linked on Windows** |
| Benchmark runner | Implemented | Smoke-tested only; timing is informational |
| Linux CI (GCC, Clang) | Implemented | Workflow written, not yet executed on GitHub; Clang job has not been run anywhere |
| Windows CI (MSVC) | Implemented | Workflow written, never executed; MSVC build unverified |
| Interactive window, keyboard/mouse input (SDL3) | Planned | Headless output only |
| Textures, sprites, enemies, weapons, audio | Planned | |
| JSON levels, prefabs, resource manager, editor tools | Planned | `assets/`, `tools/asset-validator`, `tools/level-editor` are empty placeholders |
| Release packaging / installers | Planned | Only a minimal `install()` rule for the demo exists |

## Build and test results

- Debug (GCC, `-Werror`): build OK, 8/8 CTest entries pass (65 test cases).
- Release (GCC, `-Werror`): build OK, 8/8 pass.
- Release with `GE_ASM=OFF`: build OK, 8/8 pass; no assembly symbols linked.
- Debug with Ninja: build OK, 8/8 pass.
- Debug with ASan + UBSan: build OK, 8/8 pass, no sanitizer reports.
- `GE_ASM=ON` without NASM: configure fails with a clear message. `GE_ASM=AUTO` without NASM: falls back to C++.
- The demo produces the same final frame checksum in Debug, Release and no-asm builds.
- Not verified: Windows/MSVC, Clang, GitHub Actions runs, any real display.

## Known limitations

- No window: nothing can be played interactively yet.
- The NASM routines are at performance parity with the C++ versions (add/multiply are slightly slower); they are correct but not an optimisation on this machine. `Framebuffer` uses them.
- The raycaster writes columns into a row-major buffer (strided writes); fine at current sizes, a candidate for later optimization only if profiling shows it matters.
- `LICENSE` is still a placeholder.
- Warnings are treated as errors in CI only for GCC.

## Next milestone

SDL3 presentation and input behind the existing `IPlatform` interface (Linux first, then Windows), a real-time demo, and a CI job that builds it with the SDL3 development package.
