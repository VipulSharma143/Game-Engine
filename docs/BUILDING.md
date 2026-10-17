# Building

## Requirements

C++20 compiler, CMake >= 3.21. Optional: NASM >= 2.16, Ninja. Verified environment: Ubuntu 24.04, GCC 13.3.0, CMake 3.28.3, NASM 2.16.01, Ninja (via pip), Unix Makefiles.

Ubuntu/Linux Mint: `sudo apt install build-essential cmake nasm` (add `ninja-build` if wanted).

## Configure, build, test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug     # or Release (the default when unset)
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Presets: `debug`, `release`, `ninja-debug`, `ninja-release`, `no-asm`, `asan` (`cmake --preset <name>`, `cmake --build --preset <name>`, `ctest --preset <name>`). Presets need CMake 3.21+.

## Options

`GE_ASM=AUTO|ON|OFF`, `GE_BUILD_TESTS`, `GE_BUILD_TOOLS`, `GE_WARNINGS_AS_ERRORS`, `GE_SANITIZE="address;undefined"`.
`GE_ASM=ON` fails configuration with an explanation if NASM or an x86-64 Linux/Windows target is unavailable; `AUTO` falls back to C++ and prints the reason.

## Dependencies

None. Nothing is fetched during configure or build. SDL3 (*planned*) will be added as an explicit, optional `find_package` dependency in `cmake/Dependencies.cmake`.

## Windows

Intended: Visual Studio Build Tools (MSVC), Ninja and NASM on `PATH`, built from a "x64 Native Tools" prompt with the same commands. This path is configured in `.github/workflows/build-windows.yml` but has not been executed or verified yet.
