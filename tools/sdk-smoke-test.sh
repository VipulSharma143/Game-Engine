#!/usr/bin/env bash
# Proves a SORTED SDK archive works on its own: unpacks it somewhere else, builds the example that ships inside it
# with find_package(SORTED), and runs the result. Usage: tools/sdk-smoke-test.sh <SORTED-SDK-...tar.gz|zip>
set -euo pipefail
[ $# -eq 1 ] && [ -f "$1" ] || { echo "usage: $0 <sdk archive>" >&2; exit 2; }
archive="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
native() { if command -v cygpath >/dev/null 2>&1; then cygpath -m "$1"; else echo "$1"; fi; }

work="$(mktemp -d)"; trap 'rm -rf "$work"' EXIT
( cd "$work" && cmake -E tar xf "$(native "$archive")" )
sdk="$(echo "$work"/SORTED-SDK-*/)"; sdk="${sdk%/}"
[ -f "$sdk/lib/cmake/SORTED/SORTEDConfig.cmake" ] || { echo "error: no SORTED package in the archive" >&2; exit 1; }

extra=()
[ "${OS:-}" = "Windows_NT" ] && extra+=(-DCMAKE_CXX_COMPILER=cl)
cmake -S "$(native "$sdk/share/sorted/examples/hello-sorted")" -B "$(native "$work/build")" -G Ninja \
      -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$(native "$sdk")" "${extra[@]}"
cmake --build "$(native "$work/build")"
out="$("$work/build/hello-sorted")"
echo "$out"
echo "$out" | grep -q "SORTED hello: 120 frames" || { echo "error: the example did not run 120 frames" >&2; exit 1; }
echo "SDK smoke test passed"
