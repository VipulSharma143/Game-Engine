# Install rules for the two products of this repository.
#
#   FunkShot (the game, component "funkshot")
#     Linux:   ./build.sh --package  ->  CPack writes funkshot_<version>_amd64.deb
#     Windows: ./build.sh --package  ->  stages "cmake --install", then Inno Setup builds FunkShot-Setup.exe
#              (packaging/windows/FunkShot.iss; CPack is not used on Windows)
#   SORTED SDK (the engine, component "sdk", cmake/Sdk.cmake)
#     Linux:   the same cpack run also writes sorted-sdk_<version>_amd64.deb (installs under /usr)
#     Windows: stage "cmake --install --component sdk", then Inno Setup builds SORTED-SDK-Setup.exe
#              (packaging/windows/SORTED-SDK.iss)
#
# Game layout (identical on both systems, and what find_assets_dir() in AssetPath.cpp already searches):
#   bin/funkshot[.exe]   share/funkshot/assets/   share/doc/funkshot/{README.md,LICENSE}
set(CMAKE_INSTALL_DOCDIR share/doc/funkshot)
include(GNUInstallDirs)
install(TARGETS funkshot RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT funkshot)
install(DIRECTORY assets/ DESTINATION ${CMAKE_INSTALL_DATADIR}/funkshot/assets COMPONENT funkshot PATTERN ".gitkeep" EXCLUDE)
install(FILES README.md LICENSE DESTINATION ${CMAKE_INSTALL_DOCDIR} COMPONENT funkshot)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(CPACK_GENERATOR DEB)
  set(CPACK_PACKAGE_NAME "funkshot")
  set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
  set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "FunkShot - first-person shooter built on the SORTED engine")
  set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/VipulSharma143/Game-Engine")
  set(CPACK_PACKAGE_CONTACT "VipulSharma143 <VipulSharma143@users.noreply.github.com>")
  set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
  set(CPACK_STRIP_FILES ON)
  set(CPACK_DEB_COMPONENT_INSTALL ON)                   # one .deb per component
  set(CPACK_COMPONENTS_ALL funkshot)
  if(GE_SDL STREQUAL "FETCH" AND GE_SDL_ENABLED)    # the SDK exists only when SDL3 can be bundled (cmake/Sdk.cmake)
    list(APPEND CPACK_COMPONENTS_ALL sdk)
  endif()
  set(CPACK_DEBIAN_FUNKSHOT_PACKAGE_NAME "funkshot")
  set(CPACK_DEBIAN_FUNKSHOT_FILE_NAME DEB-DEFAULT)      # funkshot_<version>_amd64.deb
  set(CPACK_DEBIAN_FUNKSHOT_PACKAGE_SECTION "games")
  set(CPACK_DEBIAN_FUNKSHOT_PACKAGE_SHLIBDEPS ON)       # libc6, libstdc++6, ... from the binary itself
  # SDL3 is linked statically (GE_SDL=FETCH) but loads the window-system and audio libraries at run time,
  # so they cannot be found by shlibdeps. A desktop with X11 or Wayland already has them.
  set(CPACK_DEBIAN_FUNKSHOT_PACKAGE_RECOMMENDS "libx11-6 | libwayland-client0, libasound2t64 | libasound2, libgl1")
  set(CPACK_DEBIAN_SDK_PACKAGE_NAME "sorted-sdk")
  set(CPACK_DEBIAN_SDK_FILE_NAME DEB-DEFAULT)           # sorted-sdk_<version>_amd64.deb
  set(CPACK_DEBIAN_SDK_PACKAGE_SECTION "devel")
  set(CPACK_DEBIAN_SDK_PACKAGE_SHLIBDEPS OFF)           # static libraries and headers only, no binaries
  set(CPACK_DEBIAN_SDK_PACKAGE_DESCRIPTION "SORTED engine SDK: headers, static library and CMake package (find_package(SORTED)) for building games on the SORTED engine")
  set(CPACK_DEBIAN_SDK_PACKAGE_RECOMMENDS "cmake, g++, ninja-build")
  include(CPack)
endif()

# A shared SDL3 (system package or -DCMAKE_PREFIX_PATH) must travel with the game, or the installed game
# would not start on a machine without it. A static SDL3 (GE_SDL=FETCH) needs nothing.
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
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT funkshot)
    if(NOT WIN32)
      set_target_properties(funkshot PROPERTIES INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
    endif()
  endif()
endif()
