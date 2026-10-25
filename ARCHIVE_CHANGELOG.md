# Archived Changelog

Release notes older than the two newest releases, newest first. `tools/changelog.py` moves them here from
[CHANGELOG.md](CHANGELOG.md) unchanged; nothing in this file is ever deleted.

## [0.1.0] - 2026-10-24

### Features

- First-person CPU raycasting demo with textured walls, floor, ceiling and distance fog; `--flat` selects the plain shaded walls.
- SDL3 window with keyboard and mouse input, menus and pause, plus a headless mode with scripted input that can write PPM frames, an ASCII preview or a WAV file.
- Gameplay: enemies that hunt the player, a weapon with reload, health, ammo and key pickups, doors and locked doors, and waves with a score.
- Four levels (`hall`, `arena`, `maze`, `bunker`) stored as plain text files, and a `settings.ini` file for resolution, mouse sensitivity and key bindings.
- Sound effects synthesised at startup and mixed in software, played through SDL3.
- NASM x86-64 routines for framebuffer fills, column fills, frame checksums, fog shading (SSE2 and AVX2 kernels) and entity motion, each with a portable C++ fallback that produces identical results.
- Windows x64 installer (`GameEngine-Setup.exe`) and Debian package (`game-engine-demo_0.1.0_amd64.deb`) published with SHA-256 checksums.
