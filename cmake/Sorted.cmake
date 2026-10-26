# SORTED (install component "sorted"): the engine, ready to use.
#   - SORTED Viewer (bin/sorted-viewer): a program that opens a window with a level you can walk around, shoot in
#     and inspect, with sample levels (share/sorted-viewer/assets) and a Linux menu entry
#   - everything a developer needs to build a game on the engine without compiling it: headers, the compiled
#     engine library (libsorted.a / sorted.lib), the static SDL3 it was built with, and a CMake package so that
#     find_package(SORTED CONFIG REQUIRED)
#     target_link_libraries(mygame PRIVATE sorted::engine)
# works. build.sh --package turns it into sorted_<version>_amd64.deb (Linux, installs under /usr, so find_package
# needs no hints) or SORTED-Setup.exe (Windows).
#
# Only available with GE_SDL=FETCH: the SDK bundles that static SDL3 so users do not have to install one.
if(NOT (GE_SDL STREQUAL "FETCH" AND GE_SDL_ENABLED))
  message(STATUS "SORTED package: not available (configure with -DGE_SDL=FETCH to bundle SDL3)")
  return()
endif()

include(CMakePackageConfigHelpers)
set(GE_SDK_DEFINITIONS "GE_HAVE_SDL=1")
if(GE_ASM_ENABLED)
  list(APPEND GE_SDK_DEFINITIONS "GE_HAVE_ASM=1")   # header-inline ge::opt wrappers must match how the library was built
endif()
configure_file(cmake/SORTEDConfig.cmake.in "${CMAKE_BINARY_DIR}/sdk/SORTEDConfig.in" @ONLY)
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/sdk/SORTEDConfig.cmake" INPUT "${CMAKE_BINARY_DIR}/sdk/SORTEDConfig.in")
write_basic_package_version_file("${CMAKE_BINARY_DIR}/sdk/SORTEDConfigVersion.cmake"
                                 VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion ARCH_INDEPENDENT)

install(TARGETS ge_engine ARCHIVE DESTINATION lib COMPONENT sorted)
install(FILES "$<TARGET_FILE:SDL3-static>" DESTINATION lib/sorted COMPONENT sorted)   # own folder: never clashes with a system libSDL3
install(DIRECTORY include/engine/ DESTINATION include/sorted/engine COMPONENT sorted FILES_MATCHING PATTERN "*.hpp")
install(FILES "${CMAKE_BINARY_DIR}/sdk/SORTEDConfig.cmake" "${CMAKE_BINARY_DIR}/sdk/SORTEDConfigVersion.cmake"
        DESTINATION lib/cmake/SORTED COMPONENT sorted)
install(FILES docs/USING_SORTED.md LICENSE DESTINATION share/doc/sorted COMPONENT sorted)
install(DIRECTORY examples/hello-sorted DESTINATION share/sorted/examples COMPONENT sorted)

# The program and what it needs to run: sample levels and the Linux menu entry.
install(TARGETS sorted-viewer RUNTIME DESTINATION bin COMPONENT sorted)
install(DIRECTORY assets/ DESTINATION share/sorted-viewer/assets COMPONENT sorted PATTERN ".gitkeep" EXCLUDE)
if(UNIX)
  install(FILES packaging/linux/sorted-viewer.desktop DESTINATION share/applications COMPONENT sorted)
endif()
