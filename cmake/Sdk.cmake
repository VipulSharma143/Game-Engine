# SORTED SDK (install component "sdk"): everything a developer needs to build a game on the engine without
# compiling it: headers, the compiled engine library (libsorted.a / sorted.lib), the static SDL3 it was built
# with, and a CMake package so that
#     find_package(SORTED CONFIG REQUIRED)
#     target_link_libraries(mygame PRIVATE sorted::engine)
# works. build.sh --package turns it into sorted-sdk_<version>_amd64.deb (Linux, installs under /usr, so find_package
# needs no hints) or SORTED-SDK-Setup.exe (Windows).
#
# Only available with GE_SDL=FETCH: the SDK bundles that static SDL3 so users do not have to install one.
if(NOT (GE_SDL STREQUAL "FETCH" AND GE_SDL_ENABLED))
  message(STATUS "SORTED SDK: not available (configure with -DGE_SDL=FETCH to bundle SDL3)")
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

install(TARGETS ge_engine ARCHIVE DESTINATION lib COMPONENT sdk)
install(FILES "$<TARGET_FILE:SDL3-static>" DESTINATION lib/sorted COMPONENT sdk)   # own folder: never clashes with a system libSDL3
install(DIRECTORY include/engine/ DESTINATION include/sorted/engine COMPONENT sdk FILES_MATCHING PATTERN "*.hpp")
install(FILES "${CMAKE_BINARY_DIR}/sdk/SORTEDConfig.cmake" "${CMAKE_BINARY_DIR}/sdk/SORTEDConfigVersion.cmake"
        DESTINATION lib/cmake/SORTED COMPONENT sdk)
install(FILES docs/USING_SORTED.md LICENSE DESTINATION share/doc/sorted-sdk COMPONENT sdk)
install(DIRECTORY examples/hello-sorted DESTINATION share/sorted/examples COMPONENT sdk)
