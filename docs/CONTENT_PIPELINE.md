# Content Pipeline

Status: **planned**. There is no asset loading, manifest, prefab or JSON level code yet.

What exists today:

- Levels are text grids parsed by `ge::parse_level` (`#`/`1`-`9` walls, `.` empty, `P` spawn). `ge::test_level()` is the built-in map.
- Directories under `assets/` (audio, fonts, levels, manifests, prefabs, sprites, textures) are empty reserved placeholders.

Planned, in order: file-based level loading, textures, sprites, JSON levels and prefabs, an asset validator (`tools/asset-validator`), a level editor (`tools/level-editor`). Resource handling will stay explicit (RAII, no global manager) until needed.
