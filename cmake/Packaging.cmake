# Install rules and packaging.
#
#   Linux:   ./build.sh --package  ->  CPack writes game-engine-demo_<version>_amd64.deb
#   Windows: ./build.sh --package  ->  stages "cmake --install", then Inno Setup builds GameEngine-Setup.exe
#            (packaging/windows/GameEngine.iss; CPack is not used on Windows)
#
# Layout (identical on both systems, and what find_assets_dir() in AssetPath.cpp already searches):
#   bin/ge-demo[.exe]   share/game-engine/assets/   share/doc/game-engine-demo/{README.md,LICENSE}
set(CMAKE_INSTALL_DOCDIR share/doc/game-engine-demo)
include(GNUInstallDirs)
install(TARGETS ge_demo RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY assets/ DESTINATION ${CMAKE_INSTALL_DATADIR}/game-engine/assets PATTERN ".gitkeep" EXCLUDE)
install(FILES README.md LICENSE DESTINATION ${CMAKE_INSTALL_DOCDIR})

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(CPACK_GENERATOR DEB)
  set(CPACK_PACKAGE_NAME "game-engine-demo")
  set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
  set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
  set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/VipulSharma143/Game-Engine")
  set(CPACK_PACKAGE_CONTACT "VipulSharma143 <VipulSharma143@users.noreply.github.com>")
  set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
  set(CPACK_STRIP_FILES ON)
  set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)               # game-engine-demo_<version>_amd64.deb
  set(CPACK_DEBIAN_PACKAGE_SECTION "games")
  set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)                # libc6, libstdc++6, ... from the binary itself
  # SDL3 is linked statically (GE_SDL=FETCH) but loads the window-system and audio libraries at run time,
  # so they cannot be found by shlibdeps. A desktop with X11 or Wayland already has them.
  set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "libx11-6 | libwayland-client0, libasound2t64 | libasound2, libgl1")
  include(CPack)
endif()

# A shared SDL3 (system package or -DCMAKE_PREFIX_PATH) must travel with the package, or the installed
# demo would not start on a machine without it. A static SDL3 (GE_SDL=FETCH) needs nothing.
if(GE_SDL_ENABLED)
  set(_ge_sdl_target SDL3::SDL3)
  get_target_property(_ge_sdl_alias SDL3::SDL3 ALIASED_TARGET)  # SDL3::SDL3 is an alias of the shared or static target
  if(_ge_sdl_alias)
    set(_ge_sdl_target ${_ge_sdl_alias})
  endif()
  get_target_property(_ge_sdl_imported ${_ge_sdl_target} IMPORTED)
  get_target_property(_ge_sdl_type ${_ge_sdl_target} TYPE)
  if(_ge_sdl_imported AND _ge_sdl_type STREQUAL "SHARED_LIBRARY")
    install(IMPORTED_RUNTIME_ARTIFACTS ${_ge_sdl_target} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
    if(NOT WIN32)
      set_target_properties(ge_demo PROPERTIES INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
    endif()
  endif()
endif()
