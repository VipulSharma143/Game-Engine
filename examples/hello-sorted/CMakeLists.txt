# The smallest program built on the SORTED SDK.
#   cmake -S . -B build        (after installing the SORTED SDK; add -DCMAKE_PREFIX_PATH=<SDK folder> if it is not found)
#   cmake --build build
cmake_minimum_required(VERSION 3.21)
project(hello_sorted LANGUAGES CXX)

# The SORTED SDK for Windows is built with the static MSVC runtime; use the same one.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

find_package(SORTED CONFIG REQUIRED)

add_executable(hello-sorted main.cpp)
target_link_libraries(hello-sorted PRIVATE sorted::engine)
