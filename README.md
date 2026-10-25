# FunkShot and SORTED

This repository contains two products that ship together:

| Name | What it is | You get |
| --- | --- | --- |
| **FunkShot** | The game: a retro first-person shooter | `FunkShot-Setup.exe` (Windows), `funkshot_<version>_amd64.deb` (Linux) |
| **SORTED** | The engine FunkShot is built on: a small C++20/NASM raycasting engine | `SORTED-SDK-Setup.exe` (Windows), `sorted-sdk_<version>_amd64.deb` (Linux) |

Play FunkShot if you want the game. Use SORTED if you want to make your own game: see [docs/USING_SORTED.md](docs/USING_SORTED.md).
(Code layout: `engine/`, `include/engine/` and `asm/` are SORTED; `game/` and `assets/` are FunkShot. The repository is still
named Game-Engine on GitHub.)

**Status: a playable first-person game.** See [CHANGELOG.md](CHANGELOG.md) for what the release contains and
[PROJECT_STATUS.md](PROJECT_STATUS.md) for the verified state of every component.

## Install FunkShot

Download from the [Releases page](https://github.com/VipulSharma143/Game-Engine/releases). No compiler, CMake, NASM or Git
is needed to play.

| System | File | Install | Run |
| --- | --- | --- | --- |
| Windows 10/11 x64 | `FunkShot-Setup.exe` | Double-click it (installs for the current user; the dialog offers all users). The C++ runtime and SDL3 are built in. | Start Menu -> FunkShot. Uninstall from Settings -> Apps. |
| Linux x86-64 | `funkshot_<version>_amd64.deb` | `sudo apt install ./funkshot_<version>_amd64.deb` | `funkshot` |

Linux requirements: a distribution with glibc 2.38 or newer and libstdc++ from GCC 13 or newer (Ubuntu 24.04, Linux Mint 22
and later), and an X11 or Wayland desktop. `apt` also installs the recommended window-system and ALSA libraries. The
package was verified on Ubuntu 24.04 only. The game finds its levels in `/usr/share/funkshot/assets`, wherever you run it from.

Check a download: `sha256sum -c SHA256SUMS.txt --ignore-missing` (Linux) or `certutil -hashfile FunkShot-Setup.exe SHA256` (Windows).

## Use SORTED to make your own game

Install the SDK like any other program (from the Releases page):

| System | Install |
| --- | --- |
| Windows 10/11 x64 (MSVC) | Run `SORTED-SDK-Setup.exe`. Per user by default; it sets `SORTED_ROOT` so CMake finds it. Choose "all users" to install into Program Files instead. |
| Linux x86-64 | `sudo apt install ./sorted-sdk_<version>_amd64.deb` (installs under `/usr`) |

Then, in your CMake project, with no extra paths:

```cmake
find_package(SORTED CONFIG REQUIRED)
target_link_libraries(mygame PRIVATE sorted::engine)
```

Headers, the compiled engine and SDL3 are all part of the SDK. A complete first program is in `examples/hello-sorted/`
(also installed with the SDK). Full guide: [docs/USING_SORTED.md](docs/USING_SORTED.md).

## What works

- SDL3 window with keyboard/mouse input, menus and pause; textured raycaster with distance fog; enemies, weapon, pickups, doors, waves; four text-file levels; synthesised sound; headless mode with scripted input (PPM, ASCII, WAV output).
- NASM x86-64 routines for System V and Windows x64 with bit-identical C++ fallbacks (fog kernels SSE2/AVX2). They are at performance parity or a modest speed-up; see [docs/PERFORMANCE.md](docs/PERFORMANCE.md).
- 17 CTest entries (unit, platform, assembly equivalence, integration), plus a separate benchmark tool.

## Not implemented

JSON levels and prefabs, a level editor, image/audio asset files (textures and sounds are generated in code), multithreading.
The Windows/MSVC build and the installer are exercised by CI only; Clang has not been verified.

## Root scripts

| Script | Purpose |
| --- | --- |
| `./build.sh` | Detects OS, compiler, CMake, NASM and SDL3, then configures, builds and tests. Options: `--debug/--release`, `--asm auto|on|off`, `--no-tests`, `--package` (.deb on Linux, Setup.exe on Windows), `--diagnose`, `--werror`, `--sanitize`, `-v`. Exit codes: 0 ok, 1 usage/missing tool, 2 configure, 3 build, 4 tests, 5 package. On a build failure it prints the failing target, source file and stage. Output goes to `build/<config>/`. |
| `./release.sh X.Y.Z` | Validates, updates version and changelogs, confirms, commits, tags and pushes a release (see Releases and CI). |
| `./start-all.sh` | Builds (or with `--no-build` verifies existing binaries), checks prerequisites, refuses a second instance (`--allow-multiple` overrides), launches `funkshot` and shuts it down cleanly on Ctrl+C. Demo arguments follow `--`. |
| `./clean.sh` | Removes only known generated paths (`build/`, `out/`, `*.ppm`). `--deep` also removes stray CMake files and root-level packages; `--dry-run` previews. Safe to repeat. |

The scripts run on Linux and in Git Bash/MSYS2 on Windows. `build.sh` and the installer step were run on Linux here; the Windows path is exercised by CI.

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

## Releases and CI

One workflow, `.github/workflows/ci.yml`, builds and tests Linux and Windows in parallel on every push and pull request to
`main`. Pushing a tag `vX.Y.Z` publishes a GitHub Release containing only `FunkShot-Setup.exe`,
`funkshot_X.Y.Z_amd64.deb`, `SORTED-SDK-Setup.exe`, `sorted-sdk_X.Y.Z_amd64.deb` and `SHA256SUMS.txt`, and only if both builds passed.

To make a release, commit your work to `main`, add a `## [X.Y.Z] - Unreleased` section to `CHANGELOG.md`, then run:

```bash
./release.sh X.Y.Z --dry-run   # optional: run every check, change nothing
./release.sh X.Y.Z             # validates, updates version + changelogs, asks you to confirm, commits, tags, pushes
```

The version lives in one place, `project(SORTED VERSION ...)` in `CMakeLists.txt`; `release.sh` updates it. Details,
changelog rules and recovery from a failed release: [docs/CI_CD.md](docs/CI_CD.md).

## Building the packages locally

`./build.sh --release --sdl fetch --package` builds, tests and then writes both products to `build/release/`:

- FunkShot: `funkshot_<version>_amd64.deb` on Linux (needs `dpkg-dev`) or `FunkShot-Setup.exe` on Windows (needs
  [Inno Setup 6](https://jrsoftware.org/isinfo.php), `choco install innosetup`).
- SORTED SDK: `sorted-sdk_<version>_amd64.deb` on Linux or `SORTED-SDK-Setup.exe` on Windows.
  After installing it, `bash tools/sdk-smoke-test.sh` builds the example that ships inside the SDK against it.

With `--sdl fetch` SDL3 is linked in statically. The packages depend on the C library of the machine they were built on, so
build on the oldest distribution you want to support.

## Prerequisites (building from source)

- A C++20 compiler (verified: GCC 13.3 on Ubuntu 24.04; MSVC x64 on Windows is built by CI)
- CMake 3.21 or newer (verified: 3.28.3 and 4.4.4)
- Optional: NASM 2.16+ for the assembly routines (verified: 2.16.01), Ninja
- Optional: SDL3 for the window. `--sdl fetch` downloads and builds it during configure (needs network and, on Linux, the
  X11/Wayland/ALSA headers listed above); without SDL3 only the headless demo is built.

## Build

```bash
./build.sh --release --sdl fetch      # configure, build and test in build/release/
# or by hand:
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DGE_SDL=FETCH
cmake --build build --parallel
```

Presets are provided: `cmake --preset release && cmake --build --preset release` (also `debug`, `ninja-debug`, `ninja-release`, `windows-release`, `no-asm`, `asan`). Preset builds go to `build/<preset>/`.

See [docs/BUILDING.md](docs/BUILDING.md).

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
./build/funkshot --ascii                          # 300 frames, prints the last frame as ASCII
./build/funkshot --width 640 --height 400 --ppm frame.ppm   # writes an image of the last frame
./build/funkshot --help
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
cmake/  docs/  assets/      build modules, documentation, levels and settings
packaging/windows/          Inno Setup script for FunkShot-Setup.exe
tools/                      benchmark runner, changelog.py (changelog sync + validation) and its tests
release.sh                  one-command release
```

## Limitations

Single-threaded; levels are small text grids; no sprite or texture files (everything is generated in code); on Windows the
demo opens a console window next to the game window (it logs there). The NASM routines are correct but only modestly
faster than the C++ fallbacks (see [docs/PERFORMANCE.md](docs/PERFORMANCE.md)).

## Contributing and license

See [docs/CONTRIBUTING.md](docs/CONTRIBUTING.md) and [AGENTS.md](AGENTS.md). The `LICENSE` file is currently a placeholder: the repository owner still needs to choose and add a license.
