# Changelog

All notable changes to FunkShot (the game) and SORTED (the engine it is built on) are recorded here, newest first. Only the
two newest releases are kept in this file; older releases are in [ARCHIVE_CHANGELOG.md](ARCHIVE_CHANGELOG.md).
`tools/changelog.py` keeps both files in sync, and `./release.sh` fills in the release date. See [AGENTS.md](AGENTS.md) for the rules.

## [0.4.0] - 2026-10-27

### Features

- SORTED Viewer, a program you can open and use: it shows a textured level that you walk around with WASD and the mouse, shoot orbs at (targets flash when hit), switch levels with `E`, switch textures off and on with `R`, and inspect with `F3` (frame rate, position, which fog routine runs). It uses only the engine, no FunkShot code. `SORTED-Setup.exe` adds it to the Start Menu; the Linux package adds it to the applications menu and as `sorted-viewer`.
- FunkShot now appears in the Linux applications menu.

### Improvements

- The engine installers are renamed to match what they contain: `SORTED-Setup.exe` and `sorted_<version>_amd64.deb` (the Viewer program plus the developer files), replacing `SORTED-SDK-Setup.exe` and `sorted-sdk_<version>_amd64.deb`.
- Programs find their data in `share/<program name>/assets`, so FunkShot and SORTED Viewer each use their own levels when installed.

### Breaking Changes

- The Linux engine package is now called `sorted` instead of `sorted-sdk`: run `sudo apt remove sorted-sdk` before installing it. The Windows installer keeps its application identity and upgrades in place.

## [0.3.0] - 2026-10-25

### Improvements

- The SORTED engine SDK is now a proper installer on both systems: `SORTED-SDK-Setup.exe` (Windows, with an uninstaller and an option to set `SORTED_ROOT` for CMake) and `sorted-sdk_<version>_amd64.deb` (Linux, installs under `/usr` so `find_package(SORTED)` needs no extra paths).

### Breaking Changes

- The SDK archives `SORTED-SDK-<version>-windows-x64.zip` and `SORTED-SDK-<version>-linux-x86_64.tar.gz` are no longer published; use the installers. The bundled SDL3 library now lives in `lib/sorted/` inside the SDK.
