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
| E | open the door in front of you (a locked door uses up a key) |
| F3 | debug overlay (FPS, entities, NASM or C++ backend) |
| Tab | release / recapture the mouse |
| Esc or P | pause (Esc again or Enter resumes, Q quits) |
| Enter / click | confirm in menus |

Destroy every target to clear a wave (+250 bonus); each wave adds targets and speed.
Rendering is textured by default (procedural brick, stone, panel and plank walls, tiled floor, lit ceiling panels, distance fog); `--flat` switches to the original flat-shaded walls.
Fog shading runs on a NASM AVX2 or SSE2 kernel when the CPU supports it (F3 shows which; `--fog-kernel scalar|sse2|avx2` forces one). All kernels give identical pixels; see `docs/PERFORMANCE.md` for the measured, modest speed-up.
Headless options: `--headless --ppm out.ppm` saves the final frame; `--window --frames N` runs N frames.

## Gameplay

Red spheres are enemies. They stand still until they see you within about 9 cells, hear you fire, or get shot, then
chase you around walls (they find routes through mazes and wait behind closed doors) and bite for 10 damage about once a
second when close. Two shots kill one. You have 100 health; at 0 you get a "YOU DIED" screen and Enter retries
the level from scratch. Diamonds are items: green health (+25, left alone if you are at full health), yellow ammo (refills
the magazine), blue key (opens one locked steel door). Every third enemy you kill drops a health pack. A wave is cleared when
all targets **and** all enemies are gone; clear the level's number of waves to win.

`./start-all.sh` opens the `bunker` level by default (six rooms, doors, a locked vault, the key in the south-east room).
Level markers: `E` enemy, `H` health, `A` ammo, `K` key, `D` door, `L` locked door (see `assets/levels/`).

## Levels, settings and key bindings

- **Levels** are plain text files in `assets/levels/` (`hall`, `arena`, `maze` ship with the repo).
  `./start-all.sh -- --level arena` plays one, `--list-levels` lists them, and `--level path/to/file.txt` loads any file.
  Format: optional `name:`, `angle:` (degrees), `waves:` lines, then `map:` followed by rows. `1`-`9` are wall
  textures, `.` is floor, `P` is the single player start, `T` a preferred target spawn, `E` an enemy, `H`/`A`/`K` pickups, `D`/`L` doors. The border must be solid.
  Mistakes are reported with the file and line (`row 12 has a different length`).
- **Settings:** copy `assets/settings.ini` next to where you run the demo as `settings.ini` (or pass `--settings FILE`)
  to change resolution, FOV, mouse sensitivity, textures and every key binding (SDL key names, several per action
  separated by commas). Typos are errors with the line number. Precedence: defaults < settings file < command line.
- **Assets** are found automatically near the executable; set `GE_ASSETS=/path` or pass `--assets DIR` to override.

## Sound

Sound effects (shot, hit, kill, reload, wave start/clear, win, menu blips) are synthesised in code at startup, so
there are no audio files. They are mixed by a small software mixer (16 voices, stereo panning from where the target is
relative to you) and played through SDL3 in window mode. `--volume 0..1`, `--no-audio`, or `audio`/`volume` in `settings.ini`
control it. If no audio device can be opened the game continues silently and says so.

Headless runs can render the audio to a file instead of a speaker: `./start-all.sh --no-build -- --headless --wav demo.wav`
writes the demo's sound track (16-bit stereo, 44.1 kHz), so you can hear what the scripted run triggers.

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
