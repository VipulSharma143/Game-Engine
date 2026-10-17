cmake_minimum_required(VERSION 3.21)
project(GameEngine VERSION 0.1.0 DESCRIPTION "Small C++20/NASM raycasting game engine foundation" LANGUAGES CXX)

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
  engine/platform/ScriptedInput.cpp
  engine/renderer/Camera.cpp
  engine/renderer/Framebuffer.cpp
  engine/renderer/Raycaster.cpp
  engine/renderer/Renderer.cpp
  engine/world/World.cpp)
add_library(ge::engine ALIAS ge_engine)
target_include_directories(ge_engine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(ge_engine PUBLIC cxx_std_20)
ge_enable_warnings(ge_engine)
ge_enable_sanitizers(ge_engine)
ge_link_assembly(ge_engine)

# ---- game (library + demo executable) ----------------------------------------
add_library(ge_game STATIC
  game/src/DemoTour.cpp
  game/src/GameApplication.cpp
  game/src/PlayerController.cpp)
add_library(ge::game ALIAS ge_game)
target_include_directories(ge_game PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/game/include")
target_link_libraries(ge_game PUBLIC ge_engine)
ge_enable_warnings(ge_game)
ge_enable_sanitizers(ge_game)

add_executable(ge_demo game/src/main.cpp)
set_target_properties(ge_demo PROPERTIES OUTPUT_NAME ge-demo)
target_link_libraries(ge_demo PRIVATE ge_game)
ge_enable_warnings(ge_demo)
ge_enable_sanitizers(ge_demo)

# ---- tests, tools, install ----------------------------------------------------
if(GE_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
if(GE_BUILD_TOOLS)
  add_subdirectory(tools/benchmark-runner)
endif()
include(Packaging)

message(STATUS "GameEngine ${PROJECT_VERSION}: platform=${GE_PLATFORM_NAME} build=${CMAKE_BUILD_TYPE}${CMAKE_CONFIGURATION_TYPES} assembly=${GE_ASM_STATUS}")
