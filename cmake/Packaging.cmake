# Minimal install rules. Installers and release archives are planned, not implemented.
include(GNUInstallDirs)
install(TARGETS ge_demo RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES README.md LICENSE DESTINATION ${CMAKE_INSTALL_DOCDIR})
