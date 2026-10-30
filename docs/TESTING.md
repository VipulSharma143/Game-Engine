# Testing

Run: `ctest --test-dir build --output-on-failure`. A dependency-free harness lives in `tests/support` (`TEST`, `CHECK`, `CHECK_EQ`, `CHECK_NEAR`); each executable runs all its tests and exits non-zero on any failure. No display is required anywhere.

| CTest entry | Tests | Covers |
| --- | --- | --- |
| unit_core | 10 | config validation/CLI parsing, frame timer clamping, fixed stepper, logger, monotonic clock |
| unit_renderer | 14 | framebuffer, ray distances in all directions, camera, wall rendering, orientation (not mirrored), determinism |
| unit_arsenal | 24 | weapon and enemy tables, reserve and reload, shotgun spread and falloff, chaingun rate, rockets and blast falloff, bolts (no tunnelling), gunner behaviour (shoots, kites, chases, needs line of sight), pickups, ammo, switching, kill rewards and drops, new run reset, enemies navigating the real catacomb maze, weapon views, determinism, new sounds |
| unit_content | 15 | level format (enemy kinds, weapon pickups, extra walls, themes), every shipped level loads and is fully reachable, textures and themes, pixel-art sprites (transparency, walls, light, lift), particles (fall, bounce, die on walls, pool cap, deterministic), new sounds |
| unit_editor | 19 | level serialization (every shipped level round-trips; unchanged files reproduce byte for byte), atomic save and refused/blocked saves, editor insert/remove/move/change/rotate, refusals that change nothing, repeated removal, never walling in the player, edited level equals the reloaded file, failed reload keeps edits, crosshair picking, selection highlight |
| unit_world | 8 | grid map, level parser errors, built-in level, AABB collision, sliding, no tunnelling |
| unit_player | 8 | forward/strafe/turn, diagonal normalisation, wall blocking, randomised never-inside-wall check |
| platform_headless | 6 | scripted input, headless platform, PPM writer, ASCII writer, press/delta carry-over helpers |
| assembly_equivalence | 11 | `opt::` vs `scalar::` for add/multiply and the pixel routines (all counts 0-70, unaligned starts, guard-word overrun checks, hash chaining, caller-state preservation); reports whether NASM is enabled |
| integration_engine | 10 | init/shutdown, invalid config, app init failure, exact deterministic loop counts, quit, realtime loop, deterministic demo run, a press arriving on a frame without a fixed step is still delivered once |
| benchmark_smoke | - | runs `ge-bench` briefly; asserts nothing about timing |

Total: 227 test cases (counted from the 19 suites; the table above lists the main ones) (plus the benchmark smoke run).

The headless path: `HeadlessPlatform` + `deterministic = true` runs the whole engine with scripted input and a virtual clock. Tests compare checksums between runs of the same build; they intentionally do not hard-code golden hashes, because floating-point results may differ between compilers.

Also verified manually (not in CTest): sanitizer build (`-DGE_SANITIZE="address;undefined"`), `GE_ASM=OFF`, `GE_ASM=ON` without NASM failing with a clear message.

CI: `.github/workflows/ci.yml` builds, runs CTest and installs the packaged demo on Linux and Windows. Not yet executed on GitHub (see docs/CI_CD.md).
