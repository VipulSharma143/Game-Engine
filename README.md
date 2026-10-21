# Game Engine

A small C++20 / NASM x86-64 engine foundation with a CPU raycasting renderer, built as the base for a retro first-person game.

**Status: early foundation (v0.1.0).** The engine core, a software raycaster, a grid world with player movement and collision, and a headless demo all build, run and pass automated tests on Linux. There is **no window yet**: frames are rendered in memory and can be dumped as an ASCII preview or a PPM image. See [PROJECT_STATUS.md](PROJECT_STATUS.md) for the exact state of every component.

## What works today

- CMake build (C++20, GCC verified on Linux), Debug/Release, Unix Makefiles and Ninja.
- Engine core: config + CLI parsing, severity logger, monotonic frame timer with spike clamping, fixed-timestep loop with a bounded catch-up, orderly init/shutdown.
- CPU raycasting renderer (DDA) with a reusable framebuffer and per-column depth buffer; no heap allocation per frame.
- Grid world, player movement (walk, strafe, turn), axis-separated wall collision with sliding.
- Headless platform with scripted input, PPM and ASCII frame output.
- NASM routines (`add`, `multiply`, framebuffer fill, column fill, frame checksum) for System V and Windows x64, behind a C++ interface with portable fallbacks, verified bit-identical by tests. The framebuffer uses them; they are at performance parity with the C++ code, not faster.
- 65 automated tests across 8 CTest entries, plus a separate benchmark tool.

## Not implemented yet

Window, keyboard/mouse input and real-time presentation (SDL3 is planned), textures, sprites, enemies, weapons, audio, JSON levels/prefabs, level editor, entity-component system, multithreading. Windows and Clang builds are configured in CI but have not been verified by the maintainers yet.

## Root scripts

| Script | Purpose |
| --- | --- |
| `./build.sh` | Detects OS, compiler, CMake, NASM and SDL3, then configures, builds and tests. Options: `--debug/--release`, `--asm auto|on|off`, `--no-tests`, `--package`, `--diagnose`, `--werror`, `--sanitize`, `-v`. Exit codes: 0 ok, 1 usage/missing tool, 2 configure, 3 build, 4 tests, 5 package. On a build failure it prints the failing target, source file and stage. Output goes to `build/<config>/`. |
| `./start-all.sh` | Builds (or with `--no-build` verifies existing binaries), checks prerequisites, refuses a second instance (`--allow-multiple` overrides), launches `ge-demo` and shuts it down cleanly on Ctrl+C. Demo arguments follow `--`. |
| `./clean.sh` | Removes only known generated paths (`build/`, `out/`, `*.ppm`). `--deep` also removes stray CMake files and root-level packages; `--dry-run` previews. Safe to repeat. |

The scripts run on Linux and in Git Bash/MSYS2 on Windows. The Windows path through the scripts has **not been run**; only Linux was verified.

## Playing the demo

`./start-all.sh --sdl fetch` builds SDL3 from source on the first run and opens a window.
If SDL3 is installed (`libsdl3-dev` or a prefix on `CMAKE_PREFIX_PATH`), plain `./start-all.sh` finds it.
Without a display or without SDL3 it falls back to the scripted headless demo.

On Debian/Ubuntu/Mint, building SDL3 from source needs the window-system headers first:
`sudo apt install libx11-dev libxext-dev libxcursor-dev libxi-dev libxrandr-dev libxfixes-dev libxss-dev libxkbcommon-dev libwayland-dev libasound2-dev libgl1-mesa-dev`

| Key | Action |
| --- | --- |
| W A S D / arrows up-down | move / strafe |
| mouse, or arrows left-right | look |
| left click or Space | fire |
| R | reload |
| F3 | debug overlay (FPS, entities, NASM or C++ backend) |
| Tab | release / recapture the mouse |
| Esc | quit |

Destroy every target to clear a wave (+250 bonus); each wave adds targets and speed.
Headless options: `--headless --ppm out.ppm` saves the final frame; `--window --frames N` runs N frames.

## Prerequisites

- A C++20 compiler (verified: GCC 13.3 on Ubuntu 24.04)
- CMake 3.21 or newer (verified: 3.28.3)
- Optional: NASM 2.16+ for the assembly routines (verified: 2.16.01), Ninja

No third-party libraries are required and nothing is downloaded during configure or build.

## Build

```bash
# Linux (Unix Makefiles; Ninja works too: -G Ninja)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Presets are provided: `cmake --preset release && cmake --build --preset release` (also `debug`, `ninja-debug`, `ninja-release`, `no-asm`, `asan`). Preset builds go to `build/<preset>/`.

Windows (MSVC, Ninja, NASM on PATH) is configured in `.github/workflows/build-windows.yml` using the same CMake options; it has not been run by the maintainers yet. See [docs/BUILDING.md](docs/BUILDING.md).

### Options

| Option | Default | Meaning |
| --- | --- | --- |
| `GE_ASM` | `AUTO` | `AUTO` uses NASM if found, `ON` requires it, `OFF` uses the C++ fallback only |
| `GE_BUILD_TESTS` | `ON` | Build the test executables and register them with CTest |
| `GE_BUILD_TOOLS` | `ON` | Build `ge-bench` |
| `GE_WARNINGS_AS_ERRORS` | `OFF` | Treat compiler warnings as errors (used for GCC in CI) |
| `GE_SANITIZE` | empty | e.g. `-DGE_SANITIZE="address;undefined"` (GCC/Clang) |

## Run the demo

```bash
./build/ge-demo --ascii                          # 300 frames, prints the last frame as ASCII
./build/ge-demo --width 640 --height 400 --ppm frame.ppm   # writes an image of the last frame
./build/ge-demo --help
```

The demo walks a scripted tour through the built-in 16x16 level with a virtual clock, so results are repeatable. It prints a summary line such as
`frames=300 steps=300 simulated=5.000s player=(6.572, 5.787) angle=1.375 checksum=...` (the checksum depends on resolution).

## Test

```bash
ctest --test-dir build --output-on-failure
```

Tests need no display. Benchmarks are separate from correctness: `./build/tools/benchmark-runner/ge-bench`. See [docs/TESTING.md](docs/TESTING.md) and [docs/PERFORMANCE.md](docs/PERFORMANCE.md).

## Layout

```text
engine/ + include/engine/   core, renderer, world, physics, platform, optimized (C++ fallback + NASM dispatch)
game/                       game library (player, application, demo tour) and the demo executable
asm/x86_64/                 NASM sources (common + linux: System V, windows: Windows x64)
tests/                      unit, platform, assembly, integration (+ tiny test harness in tests/support)
tools/benchmark-runner/     ge-bench
cmake/  docs/  assets/      build modules, documentation, asset directories (empty, reserved)
```

## Limitations

Headless only; single-threaded; map is a small built-in grid; walls are flat colours; no sprites or textures; the NASM routines are correct and cost nothing, but are not faster than the C++ fallbacks (see [docs/PERFORMANCE.md](docs/PERFORMANCE.md)).

## Milestones

1. **Done:** build system, core, raycaster, world/player, headless demo, tests, CI config.
2. SDL3 window, keyboard/mouse input, real-time presentation.
3. Textured walls, sprites, simple enemies/weapons.
4. JSON levels, prefabs, audio, then editor tooling.

## Contributing and license

See [docs/CONTRIBUTING.md](docs/CONTRIBUTING.md) and [AGENTS.md](AGENTS.md). The `LICENSE` file is currently a placeholder: the repository owner still needs to choose and add a license.
