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
