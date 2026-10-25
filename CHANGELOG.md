# Changelog

All notable changes to FunkShot (the game) and SORTED (the engine it is built on) are recorded here, newest first. Only the
two newest releases are kept in this file; older releases are in [ARCHIVE_CHANGELOG.md](ARCHIVE_CHANGELOG.md).
`tools/changelog.py` keeps both files in sync, and `./release.sh` fills in the release date. See [AGENTS.md](AGENTS.md) for the rules.

## [0.2.0] - 2026-10-25

### Features

- SORTED engine SDK for Windows x64 (MSVC) and Linux x86-64 (`SORTED-SDK-<version>-windows-x64.zip`, `SORTED-SDK-<version>-linux-x86_64.tar.gz`): headers, the compiled engine library, bundled SDL3 and a CMake package. Use it with `find_package(SORTED)` and the target `sorted::engine`; a minimal example program and a guide (`docs/USING_SORTED.md`) are included.

### Improvements

- The game is now called FunkShot and the engine SORTED, so the two are no longer confused: the title screen, installer, Start Menu entry and Linux package use the game name, and the SDK uses the engine name.
- Releases publish exactly four packages plus `SHA256SUMS.txt`: `FunkShot-Setup.exe`, `funkshot_<version>_amd64.deb` and the two SDK archives.

### Breaking Changes

- Renamed from 0.1.0: the executable `ge-demo` is now `funkshot`, `GameEngine-Setup.exe` is now `FunkShot-Setup.exe`, and the Linux package `game-engine-demo` is now `funkshot` (assets in `/usr/share/funkshot/assets`). Remove the old `game-engine-demo` package before installing `funkshot`.

## [0.1.0] - 2026-10-24

### Features

- First-person CPU raycasting demo with textured walls, floor, ceiling and distance fog; `--flat` selects the plain shaded walls.
- SDL3 window with keyboard and mouse input, menus and pause, plus a headless mode with scripted input that can write PPM frames, an ASCII preview or a WAV file.
- Gameplay: enemies that hunt the player, a weapon with reload, health, ammo and key pickups, doors and locked doors, and waves with a score.
- Four levels (`hall`, `arena`, `maze`, `bunker`) stored as plain text files, and a `settings.ini` file for resolution, mouse sensitivity and key bindings.
- Sound effects synthesised at startup and mixed in software, played through SDL3.
- NASM x86-64 routines for framebuffer fills, column fills, frame checksums, fog shading (SSE2 and AVX2 kernels) and entity motion, each with a portable C++ fallback that produces identical results.
- Windows x64 installer (`GameEngine-Setup.exe`) and Debian package (`game-engine-demo_0.1.0_amd64.deb`) published with SHA-256 checksums.
