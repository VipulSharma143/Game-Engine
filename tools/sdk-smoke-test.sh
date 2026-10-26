#!/usr/bin/env bash
# Proves the developer side of an installed SORTED works: builds the example that ships inside it with find_package(SORTED) in a
# temporary directory and runs it.
#
#   tools/sdk-smoke-test.sh            SORTED installed system-wide (sorted .deb under /usr): no hints are given,
#                                      or, if SORTED_ROOT is set (what SORTED-Setup.exe sets), only that
#   tools/sdk-smoke-test.sh <prefix>   SORTED installed in <prefix>: passed as CMAKE_PREFIX_PATH
set -euo pipefail
native() { if command -v cygpath >/dev/null 2>&1; then cygpath -m "$1"; else echo "$1"; fi; }

hints=()
if [ $# -ge 1 ]; then
  prefix="$1"; hints=(-DCMAKE_PREFIX_PATH="$(native "$prefix")")
elif [ -n "${SORTED_ROOT:-}" ]; then
  prefix="$SORTED_ROOT"
else
  prefix=/usr
fi
command -v cygpath >/dev/null 2>&1 && prefix="$(cygpath -u "$prefix")"   # Git Bash: accept C:\... paths
example="$prefix/share/sorted/examples/hello-sorted"
[ -f "$example/main.cpp" ] || { echo "error: no SORTED installation found at $prefix (missing $example)" >&2; exit 1; }

work="$(mktemp -d)"; trap 'rm -rf "$work"' EXIT
extra=()
[ "${OS:-}" = "Windows_NT" ] && extra+=(-DCMAKE_CXX_COMPILER=cl)
cmake -S "$(native "$example")" -B "$(native "$work/build")" -G Ninja -DCMAKE_BUILD_TYPE=Release \
      ${hints[@]+"${hints[@]}"} ${extra[@]+"${extra[@]}"}
cmake --build "$(native "$work/build")"
out="$("$work/build/hello-sorted")"
echo "$out"
echo "$out" | grep -q "SORTED hello: 120 frames" || { echo "error: the example did not run 120 frames" >&2; exit 1; }
echo "SDK smoke test passed"
