#!/usr/bin/env bash
# build.sh - configure, build, test and optionally package the Game Engine.
# Works on Linux and on Windows through Git Bash / MSYS2 / Cygwin.
#
# Exit codes (stable, for CI):
#   0 success   1 usage or missing tool   2 configure failed
#   3 build failed   4 tests failed   5 packaging failed
set -u -o pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT" || exit 1

CONFIG="Release"
ASM="AUTO"
SDL="AUTO"
GENERATOR=""
JOBS=""
RUN_TESTS=1
PACKAGE=0
DIAG=0
VERBOSE=0
WERROR="OFF"
SANITIZE=""
BUILD_DIR=""
CC_OVERRIDE=""
CXX_OVERRIDE=""
EXTRA_CMAKE_ARGS=()

usage() {
  cat <<'EOF'
Usage: ./build.sh [options]

  -d, --debug            Debug build
  -r, --release          Release build (default)
      --asm auto|on|off  NASM routines (default: auto; "on" fails if unusable)
      --no-asm           same as --asm off
      --sdl auto|on|fetch|off  SDL3 window support (default: auto; "fetch" downloads and builds SDL3)
      --no-tests         skip CTest
      --package          after a successful build, write both products to the build directory:
                           FunkShot (game):  funkshot_<version>_amd64.deb (Linux) / FunkShot-Setup.exe (Windows, needs Inno Setup 6)
                           SORTED (engine):  SORTED-SDK-<version>-linux-x86_64.tar.gz / SORTED-SDK-<version>-windows-x64.zip
                         (needs --sdl fetch: the SDK bundles that static SDL3)
  -G, --generator NAME   CMake generator (default: Ninja if found, else Unix Makefiles / Ninja on Windows)
  -j, --jobs N           parallel jobs (default: all cores)
      --cxx PATH         C++ compiler to use
      --cmake-arg ARG    pass ARG to the CMake configure step (repeatable), e.g. -DGE_STATIC_CRT=OFF
      --werror           treat compiler warnings as errors
      --sanitize LIST    e.g. "address;undefined" (GCC/Clang)
      --build-dir DIR    output directory (default: build/<config>)
      --diagnose         only report detected tools/libraries and exit
  -v, --verbose          show full compiler output
  -h, --help             this help

Exit codes: 0 ok, 1 usage/missing tool, 2 configure, 3 build, 4 tests, 5 package.
EOF
}

while [ $# -gt 0 ]; do
  case "$1" in
    -d|--debug) CONFIG="Debug" ;;
    -r|--release) CONFIG="Release" ;;
    --asm) shift; [ $# -gt 0 ] || { echo "error: --asm needs a value" >&2; exit 1; }
           ASM="$(printf '%s' "$1" | tr '[:lower:]' '[:upper:]')" ;;
    --no-asm) ASM="OFF" ;;
    --sdl) shift; [ $# -gt 0 ] || { echo "error: --sdl needs a value" >&2; exit 1; }
           SDL="$(printf '%s' "$1" | tr '[:lower:]' '[:upper:]')" ;;
    --no-tests) RUN_TESTS=0 ;;
    --package) PACKAGE=1 ;;
    -G|--generator) shift; [ $# -gt 0 ] || { echo "error: --generator needs a value" >&2; exit 1; }; GENERATOR="$1" ;;
    -j|--jobs) shift; [ $# -gt 0 ] || { echo "error: --jobs needs a value" >&2; exit 1; }; JOBS="$1" ;;
    --cxx) shift; [ $# -gt 0 ] || { echo "error: --cxx needs a value" >&2; exit 1; }; CXX_OVERRIDE="$1" ;;
    --cmake-arg) shift; [ $# -gt 0 ] || { echo "error: --cmake-arg needs a value" >&2; exit 1; }; EXTRA_CMAKE_ARGS+=("$1") ;;
    --werror) WERROR="ON" ;;
    --sanitize) shift; [ $# -gt 0 ] || { echo "error: --sanitize needs a value" >&2; exit 1; }; SANITIZE="$1" ;;
    --build-dir) shift; [ $# -gt 0 ] || { echo "error: --build-dir needs a value" >&2; exit 1; }; BUILD_DIR="$1" ;;
    --diagnose|--diag) DIAG=1 ;;
    -v|--verbose) VERBOSE=1 ;;
    -h|--help) usage; exit 0 ;;
    *) echo "error: unknown option '$1'" >&2; usage >&2; exit 1 ;;
  esac
  shift
done

case "$ASM" in AUTO|ON|OFF) ;; *) echo "error: --asm must be auto, on or off" >&2; exit 1 ;; esac
case "$SDL" in AUTO|ON|OFF|FETCH) ;; *) echo "error: --sdl must be auto, on, fetch or off" >&2; exit 1 ;; esac

# ---- colours / messages -------------------------------------------------------
if [ -t 1 ]; then R=$'\033[31m'; G=$'\033[32m'; Y=$'\033[33m'; B=$'\033[1m'; N=$'\033[0m'; else R=""; G=""; Y=""; B=""; N=""; fi
step() { printf '%s==> %s%s\n' "$B" "$*" "$N"; }
ok()   { printf '%s  ok:%s %s\n' "$G" "$N" "$*"; }
warn() { printf '%s  warning:%s %s\n' "$Y" "$N" "$*" >&2; }
fail() { printf '%s  FAILED:%s %s\n' "$R" "$N" "$*" >&2; }

# ---- platform detection ---------------------------------------------------------
UNAME="$(uname -s 2>/dev/null || echo unknown)"
case "$UNAME" in
  Linux*) OS="linux" ;;
  MINGW*|MSYS*|CYGWIN*) OS="windows" ;;
  Darwin*) OS="macos" ;;
  *) OS="unknown" ;;
esac
EXE=""; [ "$OS" = "windows" ] && EXE=".exe"

have() { command -v "$1" >/dev/null 2>&1; }
ver()  { "$@" 2>&1 | head -n1; }

find_cxx() {
  if [ -n "$CXX_OVERRIDE" ]; then echo "$CXX_OVERRIDE"; return; fi
  if [ -n "${CXX:-}" ] && have "$CXX"; then echo "$CXX"; return; fi
  if [ "$OS" = "windows" ] && have cl; then echo "cl"; return; fi
  for c in g++ clang++ c++; do have "$c" && { echo "$c"; return; }; done
}

CXX_BIN="$(find_cxx || true)"
HAVE_CMAKE=0; have cmake && HAVE_CMAKE=1
HAVE_NINJA=0; have ninja && HAVE_NINJA=1
HAVE_MAKE=0;  have make && HAVE_MAKE=1
HAVE_NASM=0;  have nasm && HAVE_NASM=1

SDL_STATUS="not found (window mode unavailable; install SDL3 or use --sdl fetch)"
if have pkg-config && pkg-config --exists sdl3 2>/dev/null; then
  SDL_STATUS="found $(pkg-config --modversion sdl3)"
fi

if [ -z "$GENERATOR" ]; then
  if [ "$HAVE_NINJA" = 1 ]; then GENERATOR="Ninja"
  elif [ "$OS" != "windows" ] && [ "$HAVE_MAKE" = 1 ]; then GENERATOR="Unix Makefiles"
  fi
fi

diagnose() {
  step "Environment"
  echo "  OS:         $OS ($UNAME, $(uname -m 2>/dev/null))"
  echo "  C++:        ${CXX_BIN:-MISSING}  $( [ -n "$CXX_BIN" ] && ver "$CXX_BIN" --version )"
  echo "  CMake:      $( [ $HAVE_CMAKE = 1 ] && ver cmake --version || echo 'MISSING (need >= 3.21)')"
  echo "  Generator:  ${GENERATOR:-MISSING (install ninja or make)}"
  echo "  NASM:       $( [ $HAVE_NASM = 1 ] && ver nasm -v || echo 'not found (assembly routines will fall back to C++)')"
  echo "  SDL3:       $SDL_STATUS"
  if [ "$OS" = "linux" ] && [ ! -f /usr/include/X11/Xlib.h ] && [ ! -f /usr/include/wayland-client.h ]; then
    echo "  X11/Wayland dev headers: MISSING (a source-built SDL3 would have no window). Install:"
    echo "    sudo apt install libx11-dev libxext-dev libxcursor-dev libxi-dev libxrandr-dev libxfixes-dev libxss-dev libxkbcommon-dev libwayland-dev libasound2-dev libgl1-mesa-dev"
  fi
  echo "  Config:     $CONFIG  asm=$ASM  sdl=$SDL  jobs=${JOBS:-auto}"
}

check_required() {
  local missing=0
  if [ -z "$CXX_BIN" ]; then
    fail "no C++ compiler found."
    case "$OS" in
      linux) echo "    install one:  sudo apt install build-essential" >&2 ;;
      windows) echo "    run from an 'x64 Native Tools' prompt (MSVC) or install MSYS2 g++" >&2 ;;
    esac
    missing=1
  fi
  if [ $HAVE_CMAKE = 0 ]; then
    fail "cmake not found (need >= 3.21)."
    [ "$OS" = "linux" ] && echo "    install it:  sudo apt install cmake   (or: pip install cmake)" >&2
    missing=1
  fi
  if [ -z "$GENERATOR" ]; then
    fail "no build tool found (ninja or make)."
    [ "$OS" = "linux" ] && echo "    install one:  sudo apt install ninja-build   (or make)" >&2
    [ "$OS" = "windows" ] && echo "    install Ninja:  choco install ninja   (or: pip install ninja)" >&2
    missing=1
  fi
  if [ "$ASM" = "ON" ] && [ $HAVE_NASM = 0 ]; then
    fail "--asm on requires NASM, which was not found."
    [ "$OS" = "linux" ] && echo "    install it:  sudo apt install nasm" >&2
    [ "$OS" = "windows" ] && echo "    install it:  choco install nasm   (and put it on PATH)" >&2
    missing=1
  fi
  return $missing
}

diagnose
if [ "$DIAG" = 1 ]; then
  check_required && { ok "all required tools present"; exit 0; } || exit 1
fi
check_required || exit 1
[ "$ASM" = "AUTO" ] && [ $HAVE_NASM = 0 ] && warn "NASM not found: building with the C++ fallbacks only."

CONFIG_LC="$(printf '%s' "$CONFIG" | tr '[:upper:]' '[:lower:]')"
[ -n "$BUILD_DIR" ] || BUILD_DIR="build/$CONFIG_LC"
[ -n "$JOBS" ] || JOBS="$( (nproc 2>/dev/null || getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2) )"
LOG_DIR="$BUILD_DIR/logs"
mkdir -p "$LOG_DIR" || { fail "cannot create $LOG_DIR"; exit 1; }

# ---- failure analysis -------------------------------------------------------------
# Print the failing target, source file and stage from a captured build log.
explain_build_failure() {
  local log="$1"
  echo "  ---- build failure summary ----" >&2
  local src stage target
  # Ninja: "FAILED: [code=1] path/to/file.o" ; Make: "make[2]: *** [path/file.o] Error 1"
  target="$(grep -m1 -E '^FAILED:' "$log" | sed -E 's/^FAILED: (\[code=[0-9]+\] )?//')"
  [ -n "$target" ] || target="$(grep -m1 -E '\*\*\* \[.*\] Error' "$log" | sed -E 's/.*\*\*\* \[([^]]*)\].*/\1/')"
  src="$(grep -m1 -E '^[^:(]+[:(][0-9]+[:,)].*(error|fatal error)[: ]' "$log" | sed -E 's/^([^:(]+)[:(].*/\1/')"
  stage="compile"
  if grep -qE 'undefined reference|ld returned|LNK[0-9]{4}|cannot find -l' "$log"; then stage="link"
  elif grep -qE '^[^ ]+\.asm:[0-9]+: error' "$log" || printf '%s' "$target" | grep -qE '\.asm'; then stage="assemble"
  elif grep -qE 'CMake Error' "$log"; then stage="cmake-generate"
  fi
  echo "  stage:  $stage" >&2
  [ -n "$target" ] && echo "  target: $target" >&2
  [ -n "$src" ] && echo "  source: $src" >&2
  echo "  first errors:" >&2
  grep -E -m5 ': (fatal )?error|undefined reference|LNK[0-9]{4}' "$log" | sed 's/^/    /' >&2
  echo "  full log: $log" >&2
}

run_logged() { # run_logged <log> <cmd...>
  local log="$1"; shift
  if [ "$VERBOSE" = 1 ]; then "$@" 2>&1 | tee "$log"; return "${PIPESTATUS[0]}"; fi
  "$@" >"$log" 2>&1
}

# ---- configure ----------------------------------------------------------------------
step "Configure ($CONFIG, generator: $GENERATOR)"
CFG_ARGS=(-S . -B "$BUILD_DIR" -G "$GENERATOR" "-DCMAKE_BUILD_TYPE=$CONFIG" "-DGE_ASM=$ASM" "-DGE_SDL=$SDL"
          "-DGE_WARNINGS_AS_ERRORS=$WERROR")
[ -n "$CXX_OVERRIDE" ] && CFG_ARGS+=("-DCMAKE_CXX_COMPILER=$CXX_OVERRIDE")
# With Ninja, CMake would otherwise pick a MinGW g++/gcc found on PATH before MSVC's cl, and SDL3 (C) would
# be built with a different compiler than our C++ code. Pin both to cl.
if [ "$OS" = "windows" ] && [ "$CXX_BIN" = "cl" ] && [ -z "$CXX_OVERRIDE" ]; then
  CFG_ARGS+=("-DCMAKE_C_COMPILER=cl" "-DCMAKE_CXX_COMPILER=cl")
fi
[ ${#EXTRA_CMAKE_ARGS[@]} -gt 0 ] && CFG_ARGS+=("${EXTRA_CMAKE_ARGS[@]}")
[ -n "$SANITIZE" ] && CFG_ARGS+=("-DGE_SANITIZE=$SANITIZE")
if ! run_logged "$LOG_DIR/configure.log" cmake "${CFG_ARGS[@]}"; then
  fail "CMake configuration failed (stage: configure)"
  grep -E -m8 -A3 'CMake Error' "$LOG_DIR/configure.log" | sed 's/^/    /' >&2
  echo "  full log: $LOG_DIR/configure.log" >&2
  exit 2
fi
ASM_LINE="$(grep -E 'SORTED .*assembly=' "$LOG_DIR/configure.log" | head -n1)"
[ -n "$ASM_LINE" ] && ok "${ASM_LINE#-- }"

# ---- build --------------------------------------------------------------------------
step "Build (jobs: $JOBS)"
START=$SECONDS
if ! run_logged "$LOG_DIR/build.log" cmake --build "$BUILD_DIR" --parallel "$JOBS"; then
  fail "build failed (stage: build)"
  explain_build_failure "$LOG_DIR/build.log"
  exit 3
fi
ok "built in $((SECONDS - START))s"

# Verify the expected artifacts exist; never report success without them.
DEMO="$BUILD_DIR/funkshot$EXE"
for artifact in "$DEMO" "$BUILD_DIR/libsorted.a"; do
  [ "$OS" = "windows" ] && [ "${artifact##*.}" = "a" ] && continue
  if [ ! -e "$artifact" ]; then
    # MSVC names static libs sorted.lib
    [ -e "${artifact%.a}.lib" ] && continue
    fail "expected build output is missing: $artifact"
    exit 3
  fi
done
ok "game: $DEMO"

# ---- test ---------------------------------------------------------------------------
if [ "$RUN_TESTS" = 1 ]; then
  step "Test"
  if ! run_logged "$LOG_DIR/test.log" ctest --test-dir "$BUILD_DIR" --output-on-failure -C "$CONFIG"; then
    fail "tests failed (stage: test)"
    grep -E 'Failed|\*\*\*|tests passed' "$LOG_DIR/test.log" | sed 's/^/    /' >&2
    echo "  full log: $LOG_DIR/test.log" >&2
    exit 4
  fi
  ok "$(grep -E 'tests passed' "$LOG_DIR/test.log" | head -n1)"
else
  warn "tests skipped (--no-tests)"
fi

# ---- package ------------------------------------------------------------------------
# Two products come out of one build:
#   FunkShot (the game): Linux  - CPack builds funkshot_<version>_amd64.deb (component "funkshot", cmake/Packaging.cmake)
#                        Windows - stage "cmake --install", then Inno Setup compiles packaging/windows/FunkShot.iss
#   SORTED (the engine SDK, component "sdk", cmake/Sdk.cmake): archived as SORTED-SDK-<version>-<system>.{tar.gz,zip}
find_iscc() {
  local c
  for c in iscc ISCC; do have "$c" && { command -v "$c"; return 0; }; done
  for c in "/c/Program Files (x86)/Inno Setup 6/ISCC.exe" "/c/Program Files/Inno Setup 6/ISCC.exe" \
           "${LOCALAPPDATA:-}/Programs/Inno Setup 6/ISCC.exe"; do
    [ -x "$c" ] && { echo "$c"; return 0; }
  done
  return 1
}

project_version() { sed -n 's/^CMAKE_PROJECT_VERSION:[A-Z]*=//p' "$BUILD_DIR/CMakeCache.txt" | head -n1; }

package_game_linux() {
  have cpack || { fail "cpack not found (ships with CMake)"; return 1; }
  have dpkg-deb || { fail "dpkg-deb not found (install dpkg-dev)"; return 1; }
  rm -f "$BUILD_DIR"/funkshot_*.deb   # never let a stale package pass for a new one
  ( cd "$BUILD_DIR" && run_logged "logs/package.log" cpack -C "$CONFIG" ) || return 1
  ls "$BUILD_DIR"/funkshot_*_amd64.deb >/dev/null 2>&1 || { fail "cpack produced no funkshot .deb"; return 1; }
}

package_game_windows() {
  local iscc version stage abs_build
  iscc="$(find_iscc)" || { fail "Inno Setup 6 (ISCC.exe) not found. Install it: choco install innosetup"; return 1; }
  version="$(project_version)"
  [ -n "$version" ] || { fail "cannot read the project version from $BUILD_DIR/CMakeCache.txt"; return 1; }
  abs_build="$(cd "$BUILD_DIR" && pwd)"
  stage="$abs_build/installer-stage"
  rm -rf "$stage"
  run_logged "$LOG_DIR/package.log" cmake --install "$BUILD_DIR" --config "$CONFIG" --component funkshot --prefix "$stage" || return 1
  rm -f "$BUILD_DIR/FunkShot-Setup.exe"
  # MSYS rewrites arguments that start with "/" (ISCC's option style) into paths; switch that off for this call.
  MSYS2_ARG_CONV_EXCL='*' run_logged "$LOG_DIR/package-iscc.log" "$iscc" /Q "/DAppVersion=$version" \
    "/DStageDir=$(cygpath -w "$stage")" "/O$(cygpath -w "$abs_build")" "$(cygpath -w "$ROOT/packaging/windows/FunkShot.iss")" || return 1
  [ -f "$BUILD_DIR/FunkShot-Setup.exe" ] || { fail "Inno Setup produced no FunkShot-Setup.exe"; return 1; }
}

package_sdk() {
  local version abs_build top archive
  version="$(project_version)"
  [ -n "$version" ] || { fail "cannot read the project version from $BUILD_DIR/CMakeCache.txt"; return 1; }
  abs_build="$(cd "$BUILD_DIR" && pwd)"
  if [ "$OS" = "windows" ]; then top="SORTED-SDK-$version-windows-x64"; archive="$top.zip"; else top="SORTED-SDK-$version-linux-x86_64"; archive="$top.tar.gz"; fi
  rm -rf "$abs_build/sdk-stage"; rm -f "$abs_build"/SORTED-SDK-*
  run_logged "$LOG_DIR/package-sdk.log" cmake --install "$BUILD_DIR" --config "$CONFIG" --component sdk --prefix "$abs_build/sdk-stage/$top" || return 1
  [ -f "$abs_build/sdk-stage/$top/lib/cmake/SORTED/SORTEDConfig.cmake" ] || { fail "no SDK was installed (configure with --sdl fetch)"; return 1; }
  if [ "$OS" = "windows" ]; then
    ( cd "$abs_build/sdk-stage" && cmake -E tar cf "../$archive" --format=zip -- "$top" ) || return 1
  else
    ( cd "$abs_build/sdk-stage" && cmake -E tar czf "../$archive" -- "$top" ) || return 1
  fi
  [ -f "$abs_build/$archive" ] || { fail "SDK archive was not written"; return 1; }
}

if [ "$PACKAGE" = 1 ]; then
  step "Package"
  rc=0
  case "$OS" in
    linux) package_game_linux || rc=1 ;;
    windows) package_game_windows || rc=1 ;;
    *) fail "packaging is supported on Linux and Windows only"; rc=1 ;;
  esac
  [ "$rc" -eq 0 ] && { package_sdk || rc=1; }
  if [ "$rc" -ne 0 ]; then
    fail "packaging failed (stage: package)"
    for l in "$LOG_DIR/package.log" "$LOG_DIR/package-iscc.log" "$LOG_DIR/package-sdk.log"; do [ -f "$l" ] && tail -n 15 "$l" | sed 's/^/    /' >&2; done
    exit 5
  fi
  ok "package(s) written to $BUILD_DIR: $(cd "$BUILD_DIR" && ls funkshot_*_amd64.deb FunkShot-Setup.exe SORTED-SDK-* 2>/dev/null | tr '\n' ' ')"
fi

printf '%s==> Build succeeded%s (%s)\n' "$G" "$N" "$CONFIG"
exit 0
