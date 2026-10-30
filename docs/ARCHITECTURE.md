# Architecture

Status: describes the code as of v0.1.0. Items marked *(planned)* do not exist yet.

## Frame flow

`Engine::run(Application&)` loops: measure raw delta (virtual fixed delta when `deterministic`) -> `FrameTimer::advance` (clamps stalls, negatives, NaN) -> `IPlatform::poll_input` -> `FixedStepper::consume` -> `Application::fixed_update` per step -> `Application::render` into the engine-owned `Framebuffer` -> `IPlatform::present`. After the last frame `IPlatform::finalize` runs, then `Application::shutdown`.

Catch-up is bounded (`max_catchup_steps`, default 5): excess time is dropped, so a stall cannot cause a spiral.

## Modules

| Module | Contents |
| --- | --- |
| core | `EngineConfig`, `Logger`, `FrameTimer`, `FixedStepper`, `Engine`, `Application`, `Status`, `Math` |
| renderer | `Framebuffer`, `Camera`, `cast_ray`, `Renderer` |
| world | `GridMap`, `Level` (enemy kinds, weapon pickups, themes), `parse_level`, `serialize_level_text`/`save_level_file`, `LevelEditor` (edit, select, save), `test_level()` |
| physics | `collides_aabb`, `move_with_collision` |
| platform | `IPlatform`, `InputState`, `HeadlessPlatform`, `ScriptedInput`, PPM/ASCII dump |
| optimized | `scalar::` reference code, `opt::` dispatch to NASM |
| game | `Player`, `update_player`, `GameApplication`, `demo_tour`, demo `main` |

Coordinates are y-down map space: angle 0 faces +x, positive turn is clockwise on the map, the camera's right vector is `(-dir.y, dir.x)`.

## Boundaries

- The engine owns the platform and framebuffer; the game never touches OS APIs.
- The only OS-facing abstraction is `IPlatform`. A real window backend (SDL3, *planned*) implements it without engine changes; OS-specific files would live in `engine/platform/linux` and `engine/platform/windows`.
- Prefabs and JSON levels are *planned*; their directories are kept empty on purpose. The renderer also provides `SpriteImage` (pixel art built from text, drawn as billboards with transparency, height and brightness) and `ParticleSystem` (a fixed pool of sparks, smoke and fire); the game layer owns the weapon and enemy definition tables (`game/Definitions.hpp`), projectiles and the sprite art.
