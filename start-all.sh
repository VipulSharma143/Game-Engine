#!/usr/bin/env bash
# start-all.sh - build (or verify) and launch the Game Engine demo.
# Everything after "--" is passed to ge-demo unchanged.
#
# Exit codes: 0 ok, 1 usage/missing prerequisite, 6 demo already running,
#             other values: build.sh codes (2 configure, 3 build, 4 tests) or the demo's own exit code.
set -u -o pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT" || exit 1

CONFIG="Release"
SKIP_BUILD=0
RUN_TESTS=0
ALLOW_MULTI=0
BUILD_ARGS=()
DEMO_ARGS=()

usage() {
  cat <<'USAGE'
Usage: ./start-all.sh [options] [-- demo arguments]

  -d, --debug       use the Debug build
  -r, --release     use the Release build (default)
      --no-build    do not rebuild; use existing binaries (fails if missing or not runnable)
      --with-tests  run the test suite as part of the build
      --allow-multiple  do not refuse to start when another instance is running
      --asm MODE    pass through to build.sh (auto|on|off)
  -h, --help        this help

Example: ./start-all.sh --no-build -- --ascii --frames 120
USAGE
}

while [ $# -gt 0 ]; do
  case "$1" in
    -d|--debug) CONFIG="Debug" ;;
    -r|--release) CONFIG="Release" ;;
    --no-build|--skip-build) SKIP_BUILD=1 ;;
    --with-tests) RUN_TESTS=1 ;;
    --allow-multiple) ALLOW_MULTI=1 ;;
    --asm) shift; [ $# -gt 0 ] || { echo "error: --asm needs a value" >&2; exit 1; }; BUILD_ARGS+=(--asm "$1") ;;
    -h|--help) usage; exit 0 ;;
    --) shift; DEMO_ARGS=("$@"); break ;;
    *) echo "error: unknown option '$1' (demo arguments go after --)" >&2; usage >&2; exit 1 ;;
  esac
  shift
done

case "$(uname -s 2>/dev/null)" in
  MINGW*|MSYS*|CYGWIN*) EXE=".exe"; OS="windows" ;;
  Linux*) EXE=""; OS="linux" ;;
  *) EXE=""; OS="other" ;;
esac

CONFIG_LC="$(printf '%s' "$CONFIG" | tr '[:upper:]' '[:lower:]')"
BUILD_DIR="build/$CONFIG_LC"
DEMO="$BUILD_DIR/ge-demo$EXE"
LOCK="$BUILD_DIR/.ge-demo.pid"

say()  { printf '[start-all] %s\n' "$*"; }
die()  { printf '[start-all] error: %s\n' "$1" >&2; exit "${2:-1}"; }

say "platform: $OS, configuration: $CONFIG"

# ---- build or verify ---------------------------------------------------------------
if [ "$SKIP_BUILD" = 1 ]; then
  [ -e "$DEMO" ] || die "$DEMO does not exist. Run ./build.sh (or drop --no-build)."
  say "skipping build, using existing $DEMO"
else
  [ "$RUN_TESTS" = 1 ] || BUILD_ARGS+=(--no-tests)
  say "building via build.sh"
  "$ROOT/build.sh" "--$CONFIG_LC" "${BUILD_ARGS[@]}"
  rc=$?
  [ $rc -eq 0 ] || die "build.sh failed with exit code $rc" $rc
fi

# ---- verify launch prerequisites ---------------------------------------------------------
[ -f "$DEMO" ] || die "demo binary missing: $DEMO"
[ -x "$DEMO" ] || die "demo binary is not executable: $DEMO"
if ! "$DEMO" --help >/dev/null 2>&1; then
  die "$DEMO does not run (missing runtime library?). Try: ldd $DEMO"
fi
if [ "$OS" = "linux" ] && command -v ldd >/dev/null 2>&1; then
  missing="$(ldd "$DEMO" 2>/dev/null | grep 'not found' || true)"
  [ -z "$missing" ] || die "missing runtime libraries:
$missing"
fi
# The current headless demo needs no asset files; the directory is still checked so
# the failure is reported here once assets are required.
[ -d assets ] || die "assets/ directory not found (run from a complete checkout)"
say "binary ok, assets directory ok"

# ---- single instance ------------------------------------------------------------------------
if [ "$ALLOW_MULTI" = 0 ] && [ -f "$LOCK" ]; then
  old_pid="$(cat "$LOCK" 2>/dev/null || true)"
  if [ -n "$old_pid" ] && kill -0 "$old_pid" 2>/dev/null; then
    die "demo already running (pid $old_pid). Use --allow-multiple to start another." 6
  fi
  say "removing stale lock file (pid ${old_pid:-unknown} is not running)"
  rm -f "$LOCK"
fi

# ---- launch with clean shutdown --------------------------------------------------------------
child=""
cleanup() {
  if [ -n "$child" ] && kill -0 "$child" 2>/dev/null; then
    say "stopping demo (pid $child)"
    kill -TERM "$child" 2>/dev/null
    for _ in 1 2 3 4 5 6 7 8 9 10; do kill -0 "$child" 2>/dev/null || break; sleep 0.2; done
    kill -KILL "$child" 2>/dev/null || true
  fi
  [ "$ALLOW_MULTI" = 1 ] || rm -f "$LOCK"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

say "launching: $DEMO ${DEMO_ARGS[*]:-}"
"$DEMO" "${DEMO_ARGS[@]}" &
child=$!
[ "$ALLOW_MULTI" = 1 ] || echo "$child" > "$LOCK"
wait "$child"
rc=$?
child=""
if [ $rc -ne 0 ]; then
  say "demo exited with code $rc" >&2
  exit $rc
fi
say "demo finished normally"
