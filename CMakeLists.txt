cmake_minimum_required(VERSION 3.21)
project(SORTED VERSION 0.4.0 DESCRIPTION "SORTED: a small C++20/NASM raycasting game engine" LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
  message(STATUS "CMAKE_BUILD_TYPE not set, defaulting to Release")
endif()

set(GE_ASM "AUTO" CACHE STRING "NASM assembly routines: AUTO (use if available), ON (require), OFF")
set_property(CACHE GE_ASM PROPERTY STRINGS AUTO ON OFF)
option(GE_BUILD_TESTS "Build the automated tests" ON)
option(GE_BUILD_TOOLS "Build developer tools (benchmark runner)" ON)
option(GE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
set(GE_SANITIZE "" CACHE STRING "Sanitizers for GCC/Clang, e.g. address;undefined")

# MSVC: link the C/C++ runtime statically so the .exe runs on a PC without the Visual C++
# redistributable (SDL3, built by FetchContent, follows the same setting).
option(GE_STATIC_CRT "MSVC: link the C++ runtime statically" ON)
if(MSVC AND GE_STATIC_CRT)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include(PlatformConfig)
include(CompilerWarnings)
include(AssemblyConfig)
include(Sanitizers)
include(Dependencies)

# ---- engine -----------------------------------------------------------------
add_library(ge_engine STATIC
  engine/core/Config.cpp
  engine/core/Engine.cpp
  engine/core/GameLoop.cpp
  engine/core/Logger.cpp
  engine/core/Time.cpp
  engine/optimized/OptimizedRoutines.cpp
  engine/optimized/ScalarFallback.cpp
  engine/physics/Collision.cpp
  engine/physics/Movement.cpp
  engine/platform/FrameDump.cpp
  engine/platform/HeadlessPlatform.cpp
  engine/platform/PlatformFactory.cpp
  engine/resources/AssetPath.cpp
  engine/resources/ResourceManager.cpp
  engine/core/Settings.cpp
  engine/optimized/CpuFeatures.cpp
  engine/optimized/FogBlend.cpp
  engine/audio/AudioMixer.cpp
  engine/audio/Synth.cpp
  engine/audio/Wav.cpp
  engine/platform/ScriptedInput.cpp
  engine/renderer/Camera.cpp
  engine/renderer/Framebuffer.cpp
  engine/renderer/Raycaster.cpp
  engine/renderer/Renderer.cpp
  engine/renderer/SpriteRenderer.cpp
  engine/renderer/Texture.cpp
  engine/renderer/TextRenderer.cpp
  engine/world/MotionBatch.cpp
  engine/world/LevelEditor.cpp
  engine/world/LevelLoader.cpp
  engine/world/World.cpp)
add_library(ge::engine ALIAS ge_engine)
add_library(sorted::engine ALIAS ge_engine)
# The SORTED engine library is libsorted.a / sorted.lib (the SDK ships it under that name).
set_target_properties(ge_engine PROPERTIES OUTPUT_NAME sorted)
target_include_directories(ge_engine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(ge_engine PUBLIC cxx_std_20)
ge_enable_warnings(ge_engine)
ge_enable_sanitizers(ge_engine)
ge_link_assembly(ge_engine)
ge_link_sdl(ge_engine)

# ---- FunkShot: the game (library + executable) built on the engine ----------------------------------------
add_library(ge_game STATIC
  game/src/DemoTour.cpp
  game/src/GameApplication.cpp
  game/src/GameApplicationWorld.cpp
  game/src/Enemy.cpp
  game/src/Hud.cpp
  game/src/GameAudio.cpp
  game/src/PlayerController.cpp
  game/src/WeaponSystem.cpp)
add_library(ge::game ALIAS ge_game)
target_include_directories(ge_game PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/game/include")
target_link_libraries(ge_game PUBLIC ge_engine)
ge_enable_warnings(ge_game)
ge_enable_sanitizers(ge_game)

add_executable(funkshot game/src/main.cpp)
target_link_libraries(funkshot PRIVATE ge_game)
ge_enable_warnings(funkshot)
ge_enable_sanitizers(funkshot)
if(WIN32 AND GE_SDL_ENABLED)
  # Put SDL3.dll next to funkshot.exe when SDL3 is a shared library. With a static SDL3 (GE_SDL=FETCH)
  # the DLL list is empty, and "copy" with no files would fail, so the command becomes "true" then.
  add_custom_command(TARGET funkshot POST_BUILD
                     COMMAND ${CMAKE_COMMAND} -E $<IF:$<BOOL:$<TARGET_RUNTIME_DLLS:funkshot>>,copy,true>
                             $<TARGET_RUNTIME_DLLS:funkshot> $<TARGET_FILE_DIR:funkshot>
                     COMMAND_EXPAND_LISTS)
endif()

# ---- SORTED Viewer: the engine's own program (installed by SORTED-Setup.exe / the sorted .deb) -------------
# Uses only the public engine API; no code from game/.
add_executable(sorted-viewer viewer/main.cpp)
target_link_libraries(sorted-viewer PRIVATE ge_engine)
ge_enable_warnings(sorted-viewer)
ge_enable_sanitizers(sorted-viewer)
if(WIN32 AND GE_SDL_ENABLED)
  add_custom_command(TARGET sorted-viewer POST_BUILD
                     COMMAND ${CMAKE_COMMAND} -E $<IF:$<BOOL:$<TARGET_RUNTIME_DLLS:sorted-viewer>>,copy,true>
                             $<TARGET_RUNTIME_DLLS:sorted-viewer> $<TARGET_FILE_DIR:sorted-viewer>
                     COMMAND_EXPAND_LISTS)
endif()

# ---- tests, tools, install ----------------------------------------------------
if(GE_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
if(GE_BUILD_TOOLS)
  add_subdirectory(tools/benchmark-runner)
endif()
include(Packaging)
include(Sorted)

message(STATUS "SORTED ${PROJECT_VERSION}: platform=${GE_PLATFORM_NAME} build=${CMAKE_BUILD_TYPE}${CMAKE_CONFIGURATION_TYPES} assembly=${GE_ASM_STATUS} sdl=${GE_SDL_STATUS}")
