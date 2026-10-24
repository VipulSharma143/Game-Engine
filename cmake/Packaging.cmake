# Minimal install rules. Installers and release archives are planned, not implemented.
include(GNUInstallDirs)
install(TARGETS ge_demo RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES README.md LICENSE DESTINATION ${CMAKE_INSTALL_DOCDIR})

# Demo archive via CPack (./build.sh --package): TGZ on Linux, ZIP on Windows.
install(DIRECTORY assets/ DESTINATION ${CMAKE_INSTALL_DATADIR}/game-engine/assets)
set(CPACK_PACKAGE_NAME "game-engine-demo")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "game-engine-demo-${PROJECT_VERSION}-${GE_PLATFORM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
if(WIN32)
  set(CPACK_GENERATOR ZIP)
else()
  set(CPACK_GENERATOR TGZ)
endif()
set(CPACK_PACKAGE_CHECKSUM SHA256)
include(CPack)

# A shared SDL3 (system package or -DCMAKE_PREFIX_PATH) must travel with the package, or the unpacked
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
