# Using SORTED

**SORTED** is the engine: a small C++20 raycasting engine with NASM x86-64 routines (each with a portable C++ fallback),
an SDL3 window/input/audio platform and a headless platform for tests. **FunkShot** is the game built on it (`game/`).
You can use SORTED to make your own game without touching FunkShot.

## Install SORTED

Download the installer for your system from the [Releases page](https://github.com/VipulSharma143/Game-Engine/releases):

| System | File | Install |
| --- | --- | --- |
| Windows x64 (MSVC) | `SORTED-Setup.exe` | Run it. It installs for the current user (no admin prompt) and, for developers, sets the user variable `SORTED_ROOT`, which CMake reads; or pick "all users" to install into `C:\Program Files\SORTED`, where CMake finds it by itself. Uninstall from Settings -> Apps. |
| Linux x86-64 (GCC/Clang, glibc 2.38+) | `sorted_<version>_amd64.deb` | `sudo apt install ./sorted_<version>_amd64.deb` (installs under `/usr`; remove with `sudo apt remove sorted`) |

## Try it: SORTED Viewer

The installer includes **SORTED Viewer**, a program built only on the engine: open it from the Start Menu (Windows) or your
applications menu / `sorted-viewer` (Linux). Walk around with WASD and the mouse, shoot orbs with click or Space, `E` for the
next level, `R` to turn textures off and on, `F3` for statistics (frame rate, which assembly routines run), `Esc` to pause.
Press `F2` for **edit mode**: select, insert, remove, move, change and save the objects of the level you are standing in
(controls are in the README). Its source is `viewer/main.cpp` in the repository, a good second example after the one below.

To edit levels from your own program, use the same engine pieces the Viewer uses: `ge::LevelEditor` (`engine/world/LevelEditor.hpp`:
`insert`, `remove_selected`, `move_selected`, `change_selected`, `rotate_selected`, `save`, `reload`; every call validates and
returns a `Status` message, so a bad request changes nothing), `ge::crosshair_target` (what the camera points at and where an
insert would land), `ge::Renderer::set_highlight_cell`, and `ge::save_level_file` / `ge::serialize_level_text` in
`LevelLoader.hpp`, the exact inverse of `parse_level_text`. The editor edits cells and spawn points: walls and doors have no
direction or size in the level format, so only the player start can be rotated and nothing can be scaled.

## Build your own game on SORTED

The installer also places `include/sorted/` (headers), the compiled engine (`libsorted.a` or `sorted.lib`) with the static
SDL3 it was built with, a CMake package, this guide and a ready-to-build example (`share/sorted/examples/hello-sorted/`).
You need CMake 3.21+, Ninja (or another generator) and a C++20 compiler. You do not need NASM, Git or SDL3. On Windows, open a
new terminal after installing so that `SORTED_ROOT` is visible.

## Your first program

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.21)
project(mygame LANGUAGES CXX)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")      # matters on Windows only: the SDK uses the static MSVC runtime
find_package(SORTED CONFIG REQUIRED)
add_executable(mygame main.cpp)
target_link_libraries(mygame PRIVATE sorted::engine)
```

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

If CMake cannot find SORTED (for example it is installed somewhere unusual), add `-DCMAKE_PREFIX_PATH=<SDK folder>`.

`main.cpp` is the example installed with the SDK (`hello-sorted/main.cpp`): a class that implements
`ge::Application`, handed to a `ge::Engine`. Run it with `--window` for a window.

```cpp
class MyGame final : public ge::Application {
    ge::Status initialize(const ge::EngineConfig&) override;   // load your assets, build your world
    void fixed_update(const ge::InputState&, double dt) override;  // game logic, fixed time step
    void render(ge::Framebuffer& target) override;                 // draw into the pixel buffer
    void shutdown() noexcept override;
    bool wants_quit() const noexcept override;
};
ge::Engine engine(config, ge::make_window_platform());   // or std::make_unique<ge::HeadlessPlatform>()
engine.initialize();
MyGame game;
engine.run(game);
```

## What is in the engine

| Directory (`#include "engine/<dir>/..."`) | Purpose |
| --- | --- |
| `core/` | `Engine` main loop, `Application` interface, `EngineConfig`, settings file, event bus, logging, math, time |
| `platform/` | `IPlatform`; SDL3 window/input/audio; headless platform with scripted input (PPM/ASCII dumps) for tests |
| `renderer/` | `Framebuffer`, `Raycaster` (textured walls, floor, ceiling, fog), sprites, text, textures, camera |
| `world/` | entities and components, level loader (text levels), prefab registry, world serializer |
| `physics/` | movement, collision, spatial grid |
| `audio/` | software mixer, synthesiser, WAV, audio system |
| `resources/` | asset search paths, resource cache, asset validator |
| `optimized/` | routines with NASM implementations and C++ fallbacks, chosen at run time by CPU features |

For a complete game built this way, read `game/src/` (FunkShot): `GameApplication.cpp` shows how the pieces fit.

## Using the engine from source instead

SORTED builds with plain CMake, so you can also add this repository to your project (`add_subdirectory` or `FetchContent`)
and link the target `sorted::engine`. Build it with `-DGE_SDL=FETCH` to get a window, `-DGE_ASM=ON` to require the assembly
routines (needs NASM). See [BUILDING.md](BUILDING.md).

## Notes and limits

- The SDK is a Release build. Debugging your own code works as usual; stepping into SORTED has no debug symbols.
- Windows: use MSVC x64 with the static runtime (`/MT`), as in the example, or the linker reports a runtime mismatch.
- The SDK links SDL3 statically and does not expose SDL types, so you do not need SDL3 installed or its headers.
- SORTED is version 0.x: the C++ interface can change between minor versions. `find_package(SORTED 0.1)` accepts any 0.x.
- Linux SDK was built on Ubuntu 24.04; the library needs a glibc and libstdc++ at least as new as that.
