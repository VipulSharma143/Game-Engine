# SDL3 (window, input, presentation).
#   GE_SDL=AUTO   use an installed SDL3 if find_package finds one, else build headless-only
#   GE_SDL=ON     require an installed SDL3 (fail configuration if missing)
#   GE_SDL=FETCH  download and build SDL3 from GitHub during configure (needs network)
#   GE_SDL=OFF    never use SDL3
# Point CMake at a non-system install with -DCMAKE_PREFIX_PATH=<prefix> or -DSDL3_DIR=<dir>.
set(GE_SDL "AUTO" CACHE STRING "SDL3 window platform: AUTO, ON, FETCH, OFF")
set_property(CACHE GE_SDL PROPERTY STRINGS AUTO ON FETCH OFF)

set(GE_SDL_ENABLED FALSE)
set(GE_SDL_STATUS "disabled (GE_SDL=OFF)")

if(NOT GE_SDL STREQUAL "OFF")
  if(GE_SDL STREQUAL "FETCH")
    include(FetchContent)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL3 GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
                         GIT_TAG release-3.2.8 GIT_SHALLOW TRUE)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
      # A source-built SDL only gets a real window backend if the dev headers exist at configure time.
      find_path(_ge_x11_header X11/Xlib.h)
      find_path(_ge_wayland_header wayland-client.h)
      if(NOT _ge_x11_header AND NOT _ge_wayland_header)
        message(WARNING "GE_SDL=FETCH: neither X11 nor Wayland development headers were found, so SDL3 "
                        "will be built WITHOUT a window backend (only the 'dummy' driver). On Debian/Ubuntu/Mint run:\n"
                        "  sudo apt install libx11-dev libxext-dev libxcursor-dev libxi-dev libxrandr-dev "
                        "libxfixes-dev libxss-dev libxkbcommon-dev libwayland-dev libasound2-dev libgl1-mesa-dev\n"
                        "then delete the build directory and rebuild.")
      endif()
    endif()
    FetchContent_MakeAvailable(SDL3)
    if(TARGET SDL3::SDL3)
      set(GE_SDL_ENABLED TRUE)
      set(GE_SDL_STATUS "fetched release-3.2.8 (static)")
    else()
      message(FATAL_ERROR "GE_SDL=FETCH did not produce the SDL3::SDL3 target")
    endif()
  else()
    find_package(SDL3 CONFIG QUIET)
    if(TARGET SDL3::SDL3)
      set(GE_SDL_ENABLED TRUE)
      set(GE_SDL_STATUS "found ${SDL3_VERSION}")
    elseif(GE_SDL STREQUAL "ON")
      message(FATAL_ERROR "GE_SDL=ON but SDL3 was not found. Install SDL3 development files, set "
                          "-DCMAKE_PREFIX_PATH=<prefix>, or use -DGE_SDL=FETCH.")
    else()
      set(GE_SDL_STATUS "not found, window mode unavailable (install SDL3 or use -DGE_SDL=FETCH)")
      message(STATUS "SDL3 not found: ${GE_SDL_STATUS}")
    endif()
  endif()
endif()

function(ge_link_sdl target)
  if(NOT GE_SDL_ENABLED)
    return()
  endif()
  target_sources(${target} PRIVATE engine/platform/SDLPlatform.cpp)
  target_link_libraries(${target} PUBLIC SDL3::SDL3)
  target_compile_definitions(${target} PUBLIC GE_HAVE_SDL=1)
endfunction()
