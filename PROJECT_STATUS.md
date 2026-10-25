# Project Status

Two products from one repository: **FunkShot** (the game, `game/`) and **SORTED** (the engine, `engine/`, `include/engine/`, `asm/`).

Last verified: 2026-10-24, Linux x86-64 (Ubuntu 24.04 container, 1 CPU), GCC 13.3.0, CMake 4.4.4, NASM 2.16.01, SDL3 release-3.2.8 (fetched, static).

Statuses: **Verified** = run and observed to work on the stated system; **Implemented** = code exists but has not been run on every platform; **Planned** = not started.

## Components

| Component | Status | Notes |
| --- | --- | --- |
| Engine core, raycaster, textured renderer, world, collision, audio mixer | Verified (Linux) | 17 CTest entries pass in Release |
| SDL3 window, input, audio output | Verified (Linux, virtual X display) | Window opened with the X11 driver and rendered 120 frames; no sound card, so audio output reports "disabled" and the game continues |
| Game: enemies, weapon, pickups, doors, waves, four levels | Verified (tests; interactive play not done here) | |
| NASM routines + C++ fallbacks | Verified (Linux) | `assembly_equivalence` passes. Windows variants are built by the Windows CI job only |
| FunkShot Linux package `funkshot_0.1.0_amd64.deb` | Verified | See below |
| SORTED SDK, Linux `SORTED-SDK-0.1.0-linux-x86_64.tar.gz` | Verified | An example built outside the repository against the unpacked SDK linked and ran, headless and in a window (virtual X display) |
| SORTED SDK, Windows `SORTED-SDK-0.1.0-windows-x64.zip` | Implemented | Same packaging code; its Windows system-library list is unverified; **never run**. CI builds the example against it |
| FunkShot Windows installer `FunkShot-Setup.exe` (Inno Setup) | Implemented | Script, `build.sh` step and CI install/run/uninstall test written; **never run**: no Windows or Inno Setup available where it was written |
| Windows MSVC build | Implemented | Built by CI only; MSVC warnings are not errors yet |
| Consolidated GitHub Actions workflow | Implemented | Passes `actionlint`; **never executed on GitHub** |
| `tools/changelog.py`, `release.sh` | Verified (Linux) | See below |
| Clang build | Implemented | Not run anywhere |
| JSON levels/prefabs, level editor | Planned | `tools/asset-validator`, `tools/level-editor` are empty placeholders |

## Package generation

`./build.sh --release --sdl fetch --werror --package` builds, runs all tests and writes two products to `build/release/`:

- FunkShot: `funkshot_<version>_amd64.deb` on Linux (CPack, `cmake/Packaging.cmake`), `FunkShot-Setup.exe` on Windows (stage
  `cmake --install --component funkshot`, then Inno Setup compiles `packaging/windows/FunkShot.iss`).
- SORTED SDK: `SORTED-SDK-<version>-linux-x86_64.tar.gz` / `SORTED-SDK-<version>-windows-x64.zip` (`cmake/Sdk.cmake`: headers, `libsorted`,
  the static SDL3 it was built with, a CMake package, the example and `docs/USING_SORTED.md`).

Release assets are exactly those four files and `SHA256SUMS.txt`.

## Verification results (this session)

- Release, `--werror`, `--sdl fetch`: build OK, 17/17 CTest entries pass (clean build 194 s on one core).
- The FunkShot `.deb` (package `funkshot`): control data has `Depends: libc6 (>= 2.38), libgcc-s1, libstdc++6 (>= 13.1)` and recommended window/audio libraries; contents are `/usr/bin/funkshot`, `/usr/share/funkshot/assets/` (levels, settings.ini; no `.gitkeep` files) and docs. `dpkg -i`, then `funkshot --list-levels` and `funkshot --headless --level bunker` from `/` with an empty environment (launched by bare name) find the levels and run; window mode ran 120 frames under Xvfb; `dpkg -r` removes everything.
- Bug found and fixed by that test: when launched by bare name through `PATH`, the demo searched for assets relative to the current directory. It now resolves its real location (`/proc/self/exe`, `_get_pgmptr` on Windows); covered by the new test `asset_search_resolves_a_bare_executable_name` (Linux only).
- SORTED SDK (Linux): `tools/sdk-smoke-test.sh` unpacks the archive in a temporary directory, configures the example that ships inside it with `find_package(SORTED)`, builds and runs it ("SORTED hello: 120 frames"). The archive holds 52 headers, `libsorted.a`, `libSDL3.a`, the CMake package, the example and the guide; the `.deb` holds only the game.
- Changelog: `tools/test_changelog.py` 11/11 pass; `sync` run repeatedly leaves both files byte-identical; the oldest release moves into the archive unchanged; duplicate versions, wrong order, bad headings, CMake/changelog/tag mismatches are rejected.
- `release.sh`, against a throwaway local origin: dry run changes nothing; wrong confirmation cancels and restores files; lower version, missing entry, dirty unrelated file, wrong branch and existing remote tag are rejected; real runs create one commit (only version/changelog files), an annotated tag, and one atomic push; three consecutive releases rolled the oldest into the archive; a rejected push is recovered by rerunning the same command (also when only the commit, not the tag, exists).
- Caching: a restored build tree (88 MiB) cut a rebuild after a fresh checkout from 194 s to 110 s on one core (SDL3 is ~60% of clean build CPU time). Real GitHub timings are unmeasured.
- Not verified: anything on Windows, any GitHub Actions run, tag-triggered publishing, Debug/ASan/no-asm builds this session, real display and audio hardware.

## Known limitations

- On Windows the demo shows a console window next to the game window (console-subsystem executable); CI parses its output.
- Linux package requires glibc 2.38+ and libstdc++ from GCC 13+ (built on Ubuntu 24.04); other distributions are untested.
- `LICENSE` is still a placeholder; the package maintainer field uses a GitHub noreply address.
- CI runs one Release configuration per platform; Debug, sanitizer and no-asm builds are run locally with `build.sh`.
- Warnings are errors in CI only for GCC.
