# Platform and architecture detection. Sets GE_PLATFORM_NAME, GE_IS_X86_64.
if(WIN32)
  set(GE_PLATFORM_NAME "windows")
elseif(UNIX AND NOT APPLE AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(GE_PLATFORM_NAME "linux")
else()
  set(GE_PLATFORM_NAME "${CMAKE_SYSTEM_NAME}")
  message(WARNING "Unsupported platform '${CMAKE_SYSTEM_NAME}': only Linux and Windows are tested. "
                  "The portable C++ code should still build.")
endif()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64|x64)$")
  set(GE_IS_X86_64 TRUE)
else()
  set(GE_IS_X86_64 FALSE)
endif()
