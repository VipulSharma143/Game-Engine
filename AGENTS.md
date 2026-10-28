# AGENTS.md

Instructions for AI coding agents and developers. This is the single root-level agent file for the repository.

## Purpose and architecture

A small C++20 / NASM x86-64 engine foundation for a retro first-person game. Layers (dependencies point downward only):

```text
game/            player, application, demo tour, demo executable      -> engine
engine/          core, renderer, world, physics, platform, optimized
asm/x86_64/      NASM sources                                          (only via engine/optimized)
```

- `core`: config, logger, timer, fixed-step loop, `Engine`, `Application` interface.
- `renderer`: `Framebuffer`, `Camera`, `cast_ray` (DDA), `Renderer` (walls + depth buffer).
- `world` / `physics`: `GridMap`, `Level`, AABB collision, sliding movement; `LevelEditor` (select/insert/remove/move/change/rotate a `Level`, save/reload) and level serialization. Editing keys are fixed in the SDL platform (not in `settings.ini`); the Viewer maps them to `LevelEditor` calls.
- `platform`: `IPlatform` interface; `HeadlessPlatform` and the SDL3 platform implement it. No OS headers outside this directory.
- `optimized`: `scalar::` reference implementations and `opt::` dispatch (NASM when `GE_HAVE_ASM`).

Headers live in `include/engine/<module>/`, sources in `engine/<module>/`, game code in `game/include/game/` and `game/src/`. Namespaces: `ge` (engine), `game`.

## Build and test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DGE_WARNINGS_AS_ERRORS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/funkshot --ascii
cmake -S . -B build-noasm -DGE_ASM=OFF      # the fallback build must keep working
```

Ninja is optional, never required. Do not add dependencies that download at configure/build time.

## Conventions

- C++20, no extensions, no exceptions in hot paths, no RTTI tricks. 4-space indent (`.editorconfig`), `snake_case` functions/variables, `PascalCase` types, `kName` constants, headers use `#pragma once`.
- Keep files focused (aim for < 500 lines). Comment intent, not the obvious.
- New source files must be added to the explicit lists in the top-level `CMakeLists.txt` (no globbing).
- The code compiles clean with `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion`. Keep it that way.

## Cross-platform rules

- Linux and Windows are supported targets. Shared code must not include OS headers or assume `long` is 64-bit; use fixed-width types.
- OS-specific code goes behind `IPlatform` (planned layout: `engine/platform/linux/`, `engine/platform/windows/`).
- Flags that only make sense for the C++ compiler must use `$<$<COMPILE_LANGUAGE:CXX>:...>` so they are not passed to NASM.

## Assembly rules

- Linux uses System V AMD64 (args rdi, rsi, rdx, rcx, r8, r9); Windows uses Windows x64 (rcx, rdx, r8, r9 + 32-byte shadow space). `asm/x86_64/common/math.asm` is **System V only** despite its name; Windows has its own symbols (`ge_windows_*`).
- Every asm routine needs a C++ implementation in `ScalarFallback` with identical results, a dispatch function in `OptimizedRoutines`, and an equivalence test in `tests/assembly`.
- Preserve callee-saved registers, keep the stack 16-byte aligned at calls, end ELF files with the `.note.GNU-stack` section.
- Use only baseline x86-64 instructions, or add an explicit CPU-feature check before dispatching to anything newer.
- Routines used in hot paths must be benchmarked (`ge-bench`) against the C++ fallback and must never be slower end to end. Current routines are at parity (see docs/PERFORMANCE.md); say so, do not claim speedups you have not measured.
- Routines assembled on both OSes go in `common/*.asm` using `abi.inc` (`GE_FUNC`, `A1..A4`); see docs/ASSEMBLY_ABI.md. Keep `ge::opt::` wrappers header-inline for the hot path.
- Keep tests strict: bit-exact equivalence, guard-word overrun checks, odd counts and unaligned pointers.
- The build must work without NASM (`GE_ASM=OFF` or NASM absent).

## Resources and performance

- RAII for everything owned; no raw `new/delete`; no mutable globals (the logger is the single documented exception).
- No heap allocation, resource creation or logging-by-default inside the per-frame path. Buffers are sized once (`Framebuffer::resize`, `Renderer` depth buffer).
- No threads, custom allocators or SIMD abstractions until a measurement justifies them. Keep timing out of correctness tests.

## Testing expectations

Every behaviour change needs a test in `tests/` (harness: `tests/support/TestFramework.hpp`, register new executables in `tests/CMakeLists.txt`). Tests must be deterministic, display-free, and must not assert wall-clock timing. Run the full suite in Debug and Release, and with `-DGE_ASM=OFF`, before claiming a change works.

## Do not change without checking dependents

- `IPlatform`, `Application`, `InputState`, `EngineConfig`: used by the engine loop, game, tests and (later) platform backends.
- `Framebuffer` pixel format (0xAARRGGBB) and `cast_ray` distance semantics (ray parameter = perpendicular distance when `dir = camera.direction + camera.plane * k`).
- Asm symbol names (`ge_add_i64`, `ge_linux_multiply_i64`, `ge_windows_*`, `ge_fill_u32`, `ge_fill_column_u32`, `ge_fnv1a64_u32`) and their declarations in `OptimizedRoutines.cpp`.
- Map semantics: 0 = empty, non-zero = wall id, out of bounds = solid.

## Implemented vs planned

Implemented and tested: core, raycaster, grid world, player + collision, headless platform, NASM routines (add, multiply, fill, column fill, checksum) with fallbacks, tests, benchmark tool. Also implemented: SDL3 window and input, textured rendering, enemies, weapons, pickups, doors, audio, text levels, in-Viewer level editing (`F2`: select, insert, remove, move, change, rotate the player start, save, reload), `.deb` and Setup.exe packaging. Not verified locally: the MSVC build, the Windows installer (CI only), Clang. **Planned (do not describe as working):** JSON levels/prefabs, a standalone editor tool, scripting, multithreading.

Do not claim a feature exists because a file or directory exists: empty directories under `assets/`, `tools/` and `engine/{audio,resources}` are reserved placeholders.

## CI, versioning, changelog and releases

- One workflow, `.github/workflows/ci.yml` (Linux and Windows in parallel, then a tag-only release job). Do not add
  workflows or jobs without a measured need. Every build step must be a `./build.sh` call so CI equals a local run.
  Permissions stay `contents: read`; only the release job has `contents: write`. CI never creates or pushes tags.
- The one authoritative version is `project(SORTED VERSION x.y.z ...)` in `CMakeLists.txt`. Never write a version
  anywhere else by hand (installer, package names and release titles derive from it). Tags are `vMAJOR.MINOR.PATCH`.
- `CHANGELOG.md` holds the two newest releases (plus at most one `## [X.Y.Z] - Unreleased` entry at the top);
  `ARCHIVE_CHANGELOG.md` holds everything older. Categories: Features, Improvements, Bug Fixes, Breaking Changes. Write
  short, factual notes for user-visible changes only, never commit lists, and never claim unimplemented features.
  Never edit archived entries; `python3 tools/changelog.py sync|check` moves and validates them (run `python3 tools/test_changelog.py` after changing the script).
- Release: commit to `main`, add the Unreleased entry, run `./release.sh X.Y.Z` (`--dry-run` first). Never run `git tag`
  or `git push <tag>` by hand, never delete or move a pushed tag, never overwrite a release. Recovery: docs/CI_CD.md.
- Packaging is `./build.sh --package` (CPack `.deb` on Linux, Inno Setup `packaging/windows/FunkShot.iss` on Windows) and also writes the SORTED installer
  (`sorted_<version>_amd64.deb` / `SORTED-Setup.exe` from `packaging/windows/SORTED.iss`; `cmake/Sorted.cmake`): the SORTED Viewer
  program (`viewer/`, engine API only) plus headers, library and CMake package. Game install layout on both systems: `bin/`, `share/funkshot/assets/`; the game finds assets relative to the real executable.
- Names: **SORTED** is the engine (`engine/`, `include/engine/`, `asm/`, `viewer/`; library `libsorted.a`/`sorted.lib`; target `sorted::engine`),
  **FunkShot** is the game (`game/`, `assets/`; executable `funkshot`). Engine code must not depend on `game/`. C++ namespaces
  (`ge`, `game`) and `GE_*` macros keep their short names; do not rename them. Changing a public engine header changes the SDK.

## Documentation

When behaviour, commands or status change, update in the same change: `README.md`, `PROJECT_STATUS.md`, this file, and the relevant file in `docs/`. Report only results you actually observed.
