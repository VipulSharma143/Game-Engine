#!/usr/bin/env bash
# clean.sh - remove generated build state. Only known generated paths are touched.
#   ./clean.sh            normal: build output directories and generated frame dumps
#   ./clean.sh --deep     also caches, logs, packages, editor build dirs, stray CMake files
#   ./clean.sh --dry-run  show what would be removed
set -u -o pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT" || exit 1

DEEP=0; DRY=0; QUIET=0
for a in "$@"; do
  case "$a" in
    --deep) DEEP=1 ;;
    -n|--dry-run) DRY=1 ;;
    -q|--quiet) QUIET=1 ;;
    -h|--help) sed -n '2,5p' "$0" | sed 's/^# \{0,1\}//'; echo "  -q, --quiet      only print the summary"; exit 0 ;;
    *) echo "error: unknown option '$a'" >&2; exit 1 ;;
  esac
done

# Refuse to run from anywhere that is not this project.
if [ ! -f CMakeLists.txt ] || [ ! -d engine ] || ! grep -q 'project(GameEngine' CMakeLists.txt; then
  echo "error: not a Game Engine checkout; refusing to delete anything." >&2
  exit 1
fi

removed=0; bytes=0
remove() { # remove <path> : only relative paths inside the repo, never tracked files
  local p="$1"
  [ -e "$p" ] || [ -L "$p" ] || return 0
  case "$p" in /*|*..*|"") echo "refusing unsafe path: $p" >&2; return 1 ;; esac
  if command -v git >/dev/null 2>&1 && [ -d .git ] && [ -n "$(git ls-files -- "$p" 2>/dev/null | head -n1)" ]; then
    echo "skipping $p: contains tracked files" >&2; return 0
  fi
  local sz; sz="$(du -sk -- "$p" 2>/dev/null | cut -f1)"; bytes=$((bytes + ${sz:-0}))
  [ "$QUIET" = 1 ] || printf '  %s %s\n' "$([ $DRY = 1 ] && echo 'would remove' || echo 'removed')" "$p"
  [ "$DRY" = 1 ] || rm -rf -- "$p"
  removed=$((removed + 1))
}

echo "Cleaning ($([ $DEEP = 1 ] && echo deep || echo normal)$([ $DRY = 1 ] && echo ', dry run'))"

# Normal: build trees for every configuration and preset, and generated outputs.
remove build
remove out
for d in cmake-build-*; do remove "$d"; done
for f in *.ppm; do remove "$f"; done

if [ "$DEEP" = 1 ]; then
  remove CMakeCache.txt
  remove CMakeFiles
  remove compile_commands.json
  remove CMakeUserPresets.json
  remove Testing
  remove _CPack_Packages
  for f in *.tar.gz *.zip *.sha256 ge-demo ge-demo.exe; do
    # only generated root-level packages/binaries (never tracked files)
    remove "$f"
  done
  remove .cache
fi

verb=$([ $DRY = 1 ] && echo "would be removed" || echo "removed")
echo "Summary: $removed item(s) $verb (~$((bytes / 1024)) MB)"
exit 0
