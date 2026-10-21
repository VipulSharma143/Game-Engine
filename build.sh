
#!/usr/bin/env bash
# Game Engine build driver.
# Supports Linux, Windows Git Bash/MSYS2/Cygwin, CMake, C++20, NASM,
# optional SDL3, CTest and CPack.
#
# Exit codes:
#   0 success
#   1 usage error or missing required tool
#   2 configuration failure
#   3 compilation/link failure
#   4 test failure
#   5 packaging failure
#
# SDL3 can be detected, fetched locally, required, or disabled.
# No administrator privileges are required for normal builds.

set -u
set -o pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT" || exit 1

# ---------------------------------------------------------------------------
# Defaults
# ---------------------------------------------------------------------------

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
INSTALL_DEPS=0
AUTO_FETCH_SDL=0

START_TIME=$SECONDS

usage() {
    cat <<'EOF'
Game Engine build script

Usage:
  ./build.sh [options]

Build configuration:
  -r, --release            Release build (default)
  -d, --debug              Debug build
  --asm auto|on|off        NASM assembly mode (default: auto)
  --no-asm                 Disable assembly optimizations
  --sdl auto|on|fetch|off  SDL3 support (default: auto)
  --auto-fetch-sdl         Fetch SDL3 if it is not detected
  --no-tests               Skip CTest
  --package                Package after a successful build
  --install-deps           Offer to install Linux build dependencies

Performance:
  -j, --jobs N             Parallel build jobs (default: detected CPUs)
  -G, --generator NAME     CMake generator
  --build-dir DIR          Build directory (default: build/<config>)

Compiler and diagnostics:
  --cc PATH                C compiler
  --cxx PATH               C++ compiler
  --werror                 Treat warnings as errors
  --sanitize LIST          Sanitizers, e.g. address;undefined
  --diagnose, --diag       Detect tools and libraries, then exit
  -v, --verbose            Print full build output
  -h, --help               Display this help

Examples:
  ./build.sh
  ./build.sh --sdl fetch
  ./build.sh --debug --jobs 4
  ./build.sh --sdl off --no-tests
  ./build.sh --diagnose
  ./build.sh --install-deps
  ./build.sh --package

SDL3 fetch/build behavior is controlled by CMake through GE_SDL.
Fetched dependencies should remain in the build tree.
Normal compilation does not require sudo.
EOF
}

fail() {
    printf 'ERROR: %s\n' "$*" >&2
}

warn() {
    printf 'WARNING: %s\n' "$*" >&2
}

step() {
    printf '\n==> %s\n' "$*"
}

ok() {
    printf '  OK: %s\n' "$*"
}

have() {
    command -v "$1" >/dev/null 2>&1
}

first_line() {
    "$@" 2>&1 | sed -n '1p'
}

# ---------------------------------------------------------------------------
# Argument parsing
# ---------------------------------------------------------------------------

while (($#)); do
    case "$1" in
        -r|--release)
            CONFIG="Release"
            ;;
        -d|--debug)
            CONFIG="Debug"
            ;;
        --asm)
            shift
            (($#)) || { fail "--asm requires a value"; exit 1; }
            ASM="$(printf '%s' "$1" | tr '[:lower:]' '[:upper:]')"
            ;;
        --no-asm)
            ASM="OFF"
            ;;
        --sdl)
            shift
            (($#)) || { fail "--sdl requires a value"; exit 1; }
            SDL="$(printf '%s' "$1" | tr '[:lower:]' '[:upper:]')"
            ;;
        --auto-fetch-sdl)
            AUTO_FETCH_SDL=1
            ;;
        --no-tests)
            RUN_TESTS=0
            ;;
        --package)
            PACKAGE=1
            ;;
        --install-deps)
            INSTALL_DEPS=1
            ;;
        -G|--generator)
            shift
            (($#)) || { fail "--generator requires a value"; exit 1; }
            GENERATOR="$1"
            ;;
        -j|--jobs)
            shift
            (($#)) || { fail "--jobs requires a number"; exit 1; }
            JOBS="$1"
            ;;
        --cc)
            shift
            (($#)) || { fail "--cc requires a path"; exit 1; }
            CC_OVERRIDE="$1"
            ;;
        --cxx)
            shift
            (($#)) || { fail "--cxx requires a path"; exit 1; }
            CXX_OVERRIDE="$1"
            ;;
        --werror)
            WERROR="ON"
            ;;
        --sanitize)
            shift
            (($#)) || { fail "--sanitize requires a value"; exit 1; }
            SANITIZE="$1"
            ;;
        --build-dir)
            shift
            (($#)) || { fail "--build-dir requires a path"; exit 1; }
            BUILD_DIR="$1"
            ;;
        --diagnose|--diag)
            DIAG=1
            ;;
        -v|--verbose)
            VERBOSE=1
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            fail "Unknown option: $1"
            usage >&2
            exit 1
            ;;
    esac
    shift
done

case "$CONFIG" in
    Debug|Release) ;;
    *) fail "Invalid build configuration"; exit 1 ;;
esac

case "$ASM" in
    AUTO|ON|OFF) ;;
    *) fail "--asm must be auto, on, or off"; exit 1 ;;
esac

case "$SDL" in
    AUTO|ON|FETCH|OFF) ;;
    *) fail "--sdl must be auto, on, fetch, or off"; exit 1 ;;
esac

if [[ -n "$JOBS" && ! "$JOBS" =~ ^[1-9][0-9]*$ ]]; then
    fail "--jobs must be a positive integer"
    exit 1
fi

# ---------------------------------------------------------------------------
# Platform and tool detection
# ---------------------------------------------------------------------------

UNAME="$(uname -s 2>/dev/null || printf unknown)"

case "$UNAME" in
    Linux*) OS="linux" ;;
    MINGW*|MSYS*|CYGWIN*) OS="windows" ;;
    Darwin*) OS="macos" ;;
    *) OS="unknown" ;;
esac

EXE=""
[[ "$OS" == "windows" ]] && EXE=".exe"

CXX_BIN=""
CC_BIN=""

if [[ -n "$CXX_OVERRIDE" ]]; then
    CXX_BIN="$CXX_OVERRIDE"
elif [[ -n "${CXX:-}" ]] && have "$CXX"; then
    CXX_BIN="$CXX"
else
    for candidate in g++ clang++ c++; do
        if have "$candidate"; then
            CXX_BIN="$candidate"
            break
        fi
    done
fi

if [[ -n "$CC_OVERRIDE" ]]; then
    CC_BIN="$CC_OVERRIDE"
elif [[ -n "${CC:-}" ]] && have "$CC"; then
    CC_BIN="$CC"
else
    for candidate in gcc clang cc; do
        if have "$candidate"; then
            CC_BIN="$candidate"
            break
        fi
    done
fi

HAVE_CMAKE=0
HAVE_MAKE=0
HAVE_NINJA=0
HAVE_NASM=0
HAVE_PKG_CONFIG=0

have cmake && HAVE_CMAKE=1
have make && HAVE_MAKE=1
have ninja && HAVE_NINJA=1
have nasm && HAVE_NASM=1
have pkg-config && HAVE_PKG_CONFIG=1

if [[ -z "$GENERATOR" ]]; then
    if [[ "$HAVE_NINJA" == 1 ]]; then
        GENERATOR="Ninja"
    elif [[ "$HAVE_MAKE" == 1 ]]; then
        GENERATOR="Unix Makefiles"
    fi
fi

# ---------------------------------------------------------------------------
# SDL3 detection
# ---------------------------------------------------------------------------

SDL_VERSION=""
SDL_SOURCE=""

detect_sdl3() {
    SDL_VERSION=""
    SDL_SOURCE=""

    # Prefer pkg-config when the SDL3 development package provides it.
    if [[ "$HAVE_PKG_CONFIG" == 1 ]] &&
       pkg-config --exists sdl3 2>/dev/null; then
        SDL_VERSION="$(pkg-config --modversion sdl3 2>/dev/null || true)"
        SDL_SOURCE="pkg-config"
        return 0
    fi

    # Check common CMake package locations.
    local path
    local paths=(
        "$ROOT/build/_deps/sdl3-build"
        "$ROOT/build/_deps/sdl3-src"
        "/usr/local/lib/cmake/SDL3"
        "/usr/local/lib64/cmake/SDL3"
        "/usr/lib/cmake/SDL3"
        "/usr/lib/x86_64-linux-gnu/cmake/SDL3"
        "/usr/lib/aarch64-linux-gnu/cmake/SDL3"
        "/opt/homebrew/lib/cmake/SDL3"
        "/usr/local/share/SDL3"
    )

    for path in "${paths[@]}"; do
        if [[ -f "$path/SDL3Config.cmake" ||
              -f "$path/sdl3-config.cmake" ]]; then
            SDL_SOURCE="$path"
            SDL_VERSION="installed CMake package"
            return 0
        fi
    done

    # Search common prefix directories for less conventional installations.
    for path in /usr/local/lib/cmake /usr/lib/cmake \
                /usr/lib/x86_64-linux-gnu/cmake; do
        if [[ -d "$path/SDL3" ]] &&
           find "$path/SDL3" -maxdepth 1 \
             -iname '*sdl3*config.cmake' -print -quit 2>/dev/null |
             grep -q .; then
            SDL_SOURCE="$path/SDL3"
            SDL_VERSION="installed CMake package"
            return 0
        fi
    done

    return 1
}

detect_sdl3 || true

SDL_STATUS="not detected"
[[ -n "$SDL_VERSION" ]] &&
    SDL_STATUS="$SDL_VERSION ($SDL_SOURCE)"

# Resolve AUTO into the actual CMake mode.
SDL_EFFECTIVE="$SDL"

if [[ "$SDL" == "AUTO" && -z "$SDL_VERSION" &&
      "$AUTO_FETCH_SDL" == 1 ]]; then
    SDL_EFFECTIVE="FETCH"
fi

# ---------------------------------------------------------------------------
# Optional Linux dependency installation
# ---------------------------------------------------------------------------

install_linux_dependencies() {
    if [[ "$OS" != "linux" ]]; then
        warn "--install-deps currently supports Ubuntu/Debian Linux only"
        return 1
    fi

    if ! have apt-get; then
        fail "apt-get was not found; install dependencies using your distribution"
        return 1
    fi

    cat <<'EOF'

The following system packages will be installed:
  build-essential, cmake, pkg-config, git, nasm,
  SDL3 build dependencies and common Linux windowing/audio headers.

This operation requires administrator privileges.
The build itself does not require sudo.
EOF

    if [[ ! -t 0 ]]; then
        fail "Dependency installation requires an interactive terminal"
        return 1
    fi

    printf 'Install these packages now? [y/N] '
    local answer
    read -r answer

    case "$answer" in
        y|Y|yes|YES)
            ;;
        *)
            warn "System package installation cancelled"
            return 1
            ;;
    esac

    local packages=(
        build-essential cmake pkg-config git nasm
        libasound2-dev libpulse-dev libudev-dev
        libx11-dev libxext-dev libxrandr-dev
        libxcursor-dev libxfixes-dev libxi-dev
        libxss-dev libxkbcommon-dev libwayland-dev
        wayland-protocols libgl1-mesa-dev
        libegl1-mesa-dev libgles2-mesa-dev
        libdecor-0-dev libdbus-1-dev libibus-1.0-dev
    )

    # sudo handles its own password prompt when required.
    if ! sudo apt-get update; then
        fail "apt-get update failed"
        return 1
    fi

    if ! sudo apt-get install -y "${packages[@]}"; then
        fail "System dependency installation failed"
        return 1
    fi

    ok "System dependencies installed"
}

if [[ "$INSTALL_DEPS" == 1 ]]; then
    install_linux_dependencies || exit 1

    # Refresh tool detection after installing packages.
    have cmake && HAVE_CMAKE=1
    have make && HAVE_MAKE=1
    have ninja && HAVE_NINJA=1
    have nasm && HAVE_NASM=1
    have pkg-config && HAVE_PKG_CONFIG=1

    detect_sdl3 || true
    SDL_STATUS="not detected"
    [[ -n "$SDL_VERSION" ]] &&
        SDL_STATUS="$SDL_VERSION ($SDL_SOURCE)"
fi

# ---------------------------------------------------------------------------
# Environment diagnostics
# ---------------------------------------------------------------------------

diagnose() {
    step "Environment"

    printf '  Project:    Game Engine\n'
    printf '  Root:       %s\n' "$ROOT"
    printf '  OS:         %s (%s, %s)\n' \
        "$OS" "$UNAME" "$(uname -m 2>/dev/null || printf unknown)"
    printf '  C compiler: %s\n' "${CC_BIN:-not found}"
    printf '  C++:        %s\n' "${CXX_BIN:-not found}"

    if [[ "$HAVE_CMAKE" == 1 ]]; then
        printf '  CMake:      %s\n' "$(first_line cmake --version)"
    else
        printf '  CMake:      MISSING\n'
    fi

    printf '  Generator:  %s\n' "${GENERATOR:-not found}"
    printf '  NASM:       %s\n' \
        "$( [[ "$HAVE_NASM" == 1 ]] &&
            first_line nasm -v || printf 'not found (C++ fallback)')"
    printf '  SDL3:       %s\n' "$SDL_STATUS"
    printf '  Config:     %s\n' "$CONFIG"
    printf '  Assembly:   %s\n' "$ASM"
    printf '  SDL mode:   %s\n' "$SDL_EFFECTIVE"
    printf '  Jobs:       %s\n' "${JOBS:-auto}"
    printf '  Tests:      %s\n' "$([[ "$RUN_TESTS" == 1 ]] && echo enabled || echo disabled)"
}

diagnose

if [[ "$DIAG" == 1 ]]; then
    [[ -n "$CXX_BIN" ]] || { fail "C++ compiler not found"; exit 1; }
    [[ "$HAVE_CMAKE" == 1 ]] || { fail "CMake not found"; exit 1; }
    [[ -n "$GENERATOR" ]] || { fail "No build generator found"; exit 1; }

    if [[ "$ASM" == "ON" && "$HAVE_NASM" == 0 ]]; then
        fail "NASM is required by --asm on"
        exit 1
    fi

    ok "Required build tools detected"
    exit 0
fi

# ---------------------------------------------------------------------------
# Required tool validation
# ---------------------------------------------------------------------------

if [[ -z "$CXX_BIN" ]]; then
    fail "No C++ compiler found"
    [[ "$OS" == "linux" ]] &&
        echo "Install one with: sudo apt install build-essential"
    exit 1
fi

if [[ "$HAVE_CMAKE" == 0 ]]; then
    fail "CMake is missing (version 3.21 or newer is recommended)"
    [[ "$OS" == "linux" ]] &&
        echo "Install it with: sudo apt install cmake"
    exit 1
fi

if [[ -z "$GENERATOR" ]]; then
    fail "No supported build generator found"
    [[ "$OS" == "linux" ]] &&
        echo "Install one with: sudo apt install ninja-build"
    exit 1
fi

if [[ "$ASM" == "ON" && "$HAVE_NASM" == 0 ]]; then
    fail "NASM is required by --asm on"
    [[ "$OS" == "linux" ]] &&
        echo "Install it with: sudo apt install nasm"
    exit 1
fi

if [[ "$SDL_EFFECTIVE" == "ON" && -z "$SDL_VERSION" ]]; then
    fail "SDL3 was requested but was not detected"
    echo "Try: ./build.sh --sdl fetch"
    exit 1
fi

if [[ "$SDL_EFFECTIVE" == "AUTO" && -z "$SDL_VERSION" ]]; then
    warn "SDL3 not detected; window support may be disabled"
    echo "  Fetch SDL3 with: ./build.sh --sdl fetch"
    echo "  Or enable automatic fetching with: ./build.sh --auto-fetch-sdl"
fi

# ---------------------------------------------------------------------------
# Build paths and logging
# ---------------------------------------------------------------------------

CONFIG_LC="$(printf '%s' "$CONFIG" | tr '[:upper:]' '[:lower:]')"

[[ -n "$BUILD_DIR" ]] || BUILD_DIR="build/$CONFIG_LC"

# Resolve relative build directories against the project root.
if [[ "$BUILD_DIR" != /* ]]; then
    BUILD_DIR="$ROOT/$BUILD_DIR"
fi

LOG_DIR="$BUILD_DIR/logs"

if ! mkdir -p "$LOG_DIR"; then
    fail "Cannot create build directory: $BUILD_DIR"
    exit 2
fi

CONFIG_LOG="$LOG_DIR/configure.log"
BUILD_LOG="$LOG_DIR/build.log"
TEST_LOG="$LOG_DIR/test.log"
PACKAGE_LOG="$LOG_DIR/package.log"

if [[ -z "$JOBS" ]]; then
    JOBS="$(nproc 2>/dev/null || getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)"
fi

# Avoid unreasonable automatic parallelism on constrained environments.
if [[ "$JOBS" -gt 64 && -z "${BUILD_JOBS_OVERRIDE:-}" ]]; then
    JOBS=64
fi

run_logged() {
    local log="$1"
    shift

    if [[ "$VERBOSE" == 1 ]]; then
        "$@" 2>&1 | tee "$log"
        local status="${PIPESTATUS[0]}"
        return "$status"
    fi

    "$@" >"$log" 2>&1
}

show_log_errors() {
    local log="$1"

    [[ -f "$log" ]] || return 0

    echo "  Relevant errors:"
    grep -n -i -m 12 -E \
        'fatal error:|error:|undefined reference|cannot find -l|CMake Error|FAILED:|not found|could not find|no rule to make target' \
        "$log" | sed -n '1,12p' || true

    echo "  Full log: $log"
}

# ---------------------------------------------------------------------------
# CMake configuration
# ---------------------------------------------------------------------------

step "Configure ($CONFIG, $GENERATOR)"

CFG_ARGS=(
    -S "$ROOT"
    -B "$BUILD_DIR"
    -G "$GENERATOR"
    "-DCMAKE_BUILD_TYPE=$CONFIG"
    "-DGE_ASM=$ASM"
    "-DGE_SDL=$SDL_EFFECTIVE"
    "-DGE_WARNINGS_AS_ERRORS=$WERROR"
)

[[ -n "$CC_BIN" ]] &&
    CFG_ARGS+=("-DCMAKE_C_COMPILER=$CC_BIN")

[[ -n "$CXX_BIN" ]] &&
    CFG_ARGS+=("-DCMAKE_CXX_COMPILER=$CXX_BIN")

[[ -n "$SANITIZE" ]] &&
    CFG_ARGS+=("-DGE_SANITIZE=$SANITIZE")

if ! run_logged "$CONFIG_LOG" cmake "${CFG_ARGS[@]}"; then
    fail "CMake configuration failed (exit code 2)"
    show_log_errors "$CONFIG_LOG"
    exit 2
fi

# Report the project-specific configuration summary, if CMake prints one.
if grep -q 'GameEngine .*assembly=' "$CONFIG_LOG"; then
    grep -m 1 'GameEngine .*assembly=' "$CONFIG_LOG"
else
    ok "CMake configuration completed"
fi

# ---------------------------------------------------------------------------
# Compile and link
# ---------------------------------------------------------------------------

step "Build (jobs: $JOBS)"

if ! run_logged "$BUILD_LOG" \
    cmake --build "$BUILD_DIR" --parallel "$JOBS" \
        --config "$CONFIG"; then
    fail "Compilation or linking failed (exit code 3)"
    show_log_errors "$BUILD_LOG"
    exit 3
fi

ok "Build completed in $((SECONDS - START_TIME)) seconds"

# ---------------------------------------------------------------------------
# Verify expected artifacts
# ---------------------------------------------------------------------------

DEMO="$BUILD_DIR/ge-demo$EXE"
LIBRARY="$BUILD_DIR/libge_engine.a"

if [[ "$OS" == "windows" ]]; then
    LIBRARY="$BUILD_DIR/ge_engine.lib"
fi

# Support common multi-configuration generator output directories.
if [[ ! -f "$DEMO" && -f "$BUILD_DIR/$CONFIG/ge-demo$EXE" ]]; then
    DEMO="$BUILD_DIR/$CONFIG/ge-demo$EXE"
fi

if [[ ! -f "$LIBRARY" && -f "$BUILD_DIR/$CONFIG/libge_engine.a" ]]; then
    LIBRARY="$BUILD_DIR/$CONFIG/libge_engine.a"
fi

if [[ ! -f "$DEMO" ]]; then
    fail "Expected demo executable not found: $DEMO"
    echo "Inspect the build log: $BUILD_LOG"
    exit 3
fi

if [[ ! -f "$LIBRARY" ]]; then
    fail "Expected engine library not found: $LIBRARY"
    echo "Inspect the build log: $BUILD_LOG"
    exit 3
fi

ok "Demo: $DEMO"
ok "Library: $LIBRARY"

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

if [[ "$RUN_TESTS" == 1 ]]; then
    step "Test"

    if ! run_logged "$TEST_LOG" \
        ctest --test-dir "$BUILD_DIR" \
            --output-on-failure --no-tests=error \
            -C "$CONFIG"; then
        fail "Tests failed (exit code 4)"
        show_log_errors "$TEST_LOG"
        exit 4
    fi

    if grep -q 'tests passed' "$TEST_LOG"; then
        grep -m 1 'tests passed' "$TEST_LOG"
    else
        ok "CTest completed"
    fi
else
    warn "Tests skipped (--no-tests)"
fi

# ---------------------------------------------------------------------------
# Packaging
# ---------------------------------------------------------------------------

if [[ "$PACKAGE" == 1 ]]; then
    step "Package"

    if ! have cpack; then
        fail "CPack was not found; reinstall or repair the CMake installation"
        exit 5
    fi

    if ! (
        cd "$BUILD_DIR" &&
        run_logged "$PACKAGE_LOG" cpack -C "$CONFIG"
    ); then
        fail "Packaging failed (exit code 5)"
        show_log_errors "$PACKAGE_LOG"
        exit 5
    fi

    ok "Packages generated in $BUILD_DIR"
fi

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------

step "Build succeeded"

printf '  Configuration: %s\n' "$CONFIG"
printf '  SDL3 mode:     %s\n' "$SDL_EFFECTIVE"
printf '  Assembly:      %s\n' "$ASM"
printf '  Build time:    %s seconds\n' "$((SECONDS - START_TIME))"
printf '  Output:        %s\n' "$DEMO"
printf '  Logs:          %s\n' "$LOG_DIR"

exit 0
