# Project Status

Two products from one repository: **FunkShot** (the game: `game/`, `assets/`) and **SORTED** (the engine: `engine/`, `include/engine/`,
`asm/`, plus the SORTED Viewer program in `viewer/`).

Last verified: 2026-10-25, Linux x86-64 (Ubuntu 24.04 container, 1 CPU), GCC 13.3.0, CMake 4.4.4, NASM 2.16.01, SDL3 release-3.2.8 (fetched, static).

Statuses: **Verified** = run and observed to work on the stated system; **Implemented** = code exists but has not been run on every platform; **Planned** = not started.

## Components

| Component | Status | Notes |
| --- | --- | --- |
| Engine core, raycaster, textured renderer, world, collision, audio mixer | Verified (Linux) | 18 CTest entries pass in Release (also Debug with address+UB sanitizers and the no-NASM build, both without SDL: 17 entries) |
| In-Viewer level editing (`F2`) | Verified (Linux, virtual X display) | Real keys and mouse clicks: insert (right click), select (left click, highlight), wheel and `]` choose, change (`C`), remove (`X`), repeated remove (refused, no crash), move (`M`), rotate the player start (`.`), save (`F5`, file diffed), reload (`F9`), `E` refused while unsaved, relaunch shows the saved level and start angle, quit exit code 0. `unit_editor` (19) covers the same rules without a display. Windows: **never run** |
| SDL3 window, input | Verified (Linux, virtual X display) | Real key presses and mouse clicks (xdotool) drove the Viewer; audio output is untested (no sound card) |
| SORTED Viewer (`viewer/`) | Verified (Linux) | Walk, turn, shoot orbs, next level, textures on/off, statistics, pause, quit from pause (exit code 0); checked by screenshots. Windows: **never run** |
| FunkShot: enemies, weapon, pickups, doors, waves, four levels | Verified (tests); interactive play not done here | |
| NASM routines + C++ fallbacks | Verified (Linux) | `assembly_equivalence` passes; the Viewer's statistics showed the NASM AVX2 fog kernel in use. Windows variants are built by the Windows CI job only |
| FunkShot Linux package `funkshot_<version>_amd64.deb` | Verified | |
| SORTED Linux package `sorted_<version>_amd64.deb` | Verified | Installs the Viewer, sample levels, a menu entry, headers, `libsorted.a`, bundled SDL3 and the CMake package |
| FunkShot `FunkShot-Setup.exe`, SORTED `SORTED-Setup.exe` (Inno Setup) | Implemented | Scripts, `build.sh` steps and CI install/run/uninstall tests written; **never run**: no Windows or Inno Setup where they were written |
| Windows MSVC build | Implemented | Built by CI only; MSVC warnings are not errors yet |
| GitHub Actions workflow | Implemented | Passes `actionlint`. Whether its runs (including releases `v0.1.0`-`v0.3.0`) succeeded on GitHub has not been observed by the author of this file |
| `tools/changelog.py`, `release.sh` | Verified (Linux) | See below |
| Clang build | Implemented | Not run anywhere |
| JSON levels/prefabs, standalone level editor | Planned | `tools/asset-validator`, `tools/level-editor` are empty placeholders (editing happens inside SORTED Viewer instead) |

## Package generation

`./build.sh --release --sdl fetch --werror --package` builds, runs all tests and writes two products to `build/release/`:

- FunkShot: `funkshot_<version>_amd64.deb` (Linux, CPack) or `FunkShot-Setup.exe` (Windows, Inno Setup `packaging/windows/FunkShot.iss`).
- SORTED: `sorted_<version>_amd64.deb` (Linux, CPack) or `SORTED-Setup.exe` (Windows, `packaging/windows/SORTED.iss`).
  Both contain the SORTED Viewer and the developer files (`cmake/Sorted.cmake`).

Release assets are exactly those four files and `SHA256SUMS.txt`.

## Verification results (0.4.0 work)

- Release, `--werror`, `--sdl fetch`: build OK, 18/18 CTest entries pass (clean build about 5 minutes on one core). With the level-editing change: Debug + `address;undefined` sanitizers and `--no-asm` builds (both `--sdl off`, `--werror`) pass 17/17; with editing off, the Viewer's frames are bit-identical to the pre-change build for all four levels once the start-up help panel (whose text changed) is gone.
- SORTED Viewer driven in a window under a virtual X display with real input: movement, turning, orbs (9 counted after firing), level switch (Bunker to Training Hall), texture toggle, statistics overlay, pause panel, `Q` exits with code 0. `--level` by name, by file path, and an unknown name (clear error) behave.
- `sorted_<version>_amd64.deb`: `Depends: libc6, libgcc-s1, libstdc++6` (computed), recommended window/audio libraries, contents `/usr/bin/sorted-viewer`, levels, `settings.ini`, `/usr/share/applications/sorted-viewer.desktop`, 52 headers, `/usr/lib/libsorted.a`, `/usr/lib/sorted/libSDL3.a`, `/usr/lib/cmake/SORTED/`, guide, example. After `apt install`, from `/` with an empty environment: `sorted-viewer --list-levels`, `--headless`, a window launch by bare name, and `find_package(SORTED)` with no hints all work; `apt remove` leaves nothing behind.
- Asset lookup is now generic (`share/<executable name>/assets`), so FunkShot and SORTED Viewer each find their own data; covered by `asset_search_uses_the_executable_name_for_the_installed_layout`.
- Earlier (still valid): launching by bare name resolves the real executable location (`/proc/self/exe`, `_get_pgmptr` on Windows); `tools/test_changelog.py` 11/11; `release.sh` rehearsed against a throwaway origin (dry run, cancel, rejections, three consecutive releases, failed-push recovery).
- Not verified: anything on Windows (including `std::filesystem::rename` replacing the level file on save), any GitHub Actions run, tag-triggered publishing, rebuilding the `.deb`/Setup.exe after the editing change (one more installed header, `LevelEditor.hpp`), Debug/ASan/no-asm builds *with* SDL, real audio hardware, play on a real display.

## Known limitations

- Level editing in SORTED Viewer changes cells and spawn points only: no scaling (the level format stores no sizes), only the player start can be rotated, no undo (`F9` reverts to the last save), no level-wide settings (name, waves) and no new enemy types or textures (those still need code). Saving writes wall id 1 as `1` instead of `#`, so saving a level that used `#` changes those characters in the file. Saving overwrites the level's file in place (the repository copy when run from the source tree); a copy under `/usr/share` needs write permission or `--level FILE` on your own copy.
- FunkShot is not changed: editing exists in SORTED Viewer only.
- On Windows both programs show a console window next to their window (console-subsystem executables).
- Linux packages require glibc 2.38+ and libstdc++ from GCC 13+ (built on Ubuntu 24.04); other distributions are untested.
- `LICENSE` is still a placeholder; the package maintainer field uses a GitHub noreply address.
- CI runs one Release configuration per platform; Debug, sanitizer and no-asm builds are run locally with `build.sh`.
- Warnings are errors in CI only for GCC.
