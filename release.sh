#!/usr/bin/env bash
# release.sh - prepare, tag and push a release. GitHub Actions then builds and publishes it.
#
#   ./release.sh X.Y.Z [--dry-run]
#
# Before running: commit your work to main and add "## [X.Y.Z] - Unreleased" with categorized notes to
# CHANGELOG.md (CHANGELOG.md and ARCHIVE_CHANGELOG.md are the only files allowed to be uncommitted).
#
# The script checks the version, the changelog and Git state, bumps the version in CMakeLists.txt, stamps the
# release date, moves older releases into ARCHIVE_CHANGELOG.md, shows you the result, and after you confirm
# creates the commit "chore(release): vX.Y.Z", the annotated tag vX.Y.Z, and pushes both in one atomic push.
# It never deletes or overwrites an existing tag or release and never needs a token: git uses whatever
# GitHub authentication you already have (SSH key, credential manager, gh auth login).
#
# --dry-run runs every check and shows the changes, then restores your files and stops.
# See docs/CI_CD.md for recovery from a partly finished release.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"

die() { printf 'error: %s\n' "$*" >&2; exit 1; }
say() { printf '==> %s\n' "$*"; }

VERSION=""; DRY=0
for arg in "$@"; do
  case "$arg" in
    --dry-run) DRY=1 ;;
    -h|--help) sed -n '2,16p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    -*) die "unknown option '$arg' (see ./release.sh --help)" ;;
    *) [ -z "$VERSION" ] || die "give exactly one version"; VERSION="$arg" ;;
  esac
done
[[ "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || die "usage: ./release.sh X.Y.Z [--dry-run]   (for example ./release.sh 0.1.0)"
TAG="v$VERSION"
MESSAGE="chore(release): $TAG"
PY="$(command -v python3 || command -v python || true)"
[ -n "$PY" ] || die "python3 is required (tools/changelog.py)"
command -v git >/dev/null || die "git is required"

# ---- Git state ----------------------------------------------------------------------------------
git rev-parse --is-inside-work-tree >/dev/null 2>&1 || die "not inside a Git repository"
[ "$(git symbolic-ref --short -q HEAD || true)" = "main" ] || die "releases are made from the main branch"
git remote get-url origin >/dev/null 2>&1 || die "no 'origin' remote is configured"
say "Fetching origin"
git fetch --quiet --tags origin || die "cannot reach origin (check your network and GitHub authentication)"
git rev-parse -q --verify refs/remotes/origin/main >/dev/null || die "origin has no main branch"
[ "$(git rev-list --count HEAD..origin/main)" = "0" ] || die "main is behind origin/main; run 'git pull --ff-only' first"

if git ls-remote --exit-code --tags origin "refs/tags/$TAG" >/dev/null 2>&1; then
  die "tag $TAG already exists on origin. Existing releases are never overwritten; choose a new version."
fi
LOCAL_TAG=0
if git rev-parse -q --verify "refs/tags/$TAG" >/dev/null; then
  [ "$(git rev-parse "$TAG^{commit}")" = "$(git rev-parse HEAD)" ] && [ "$(git log -1 --format=%s)" = "$MESSAGE" ] \
    || die "tag $TAG already exists locally and does not point at the release commit. See docs/CI_CD.md (recovery)."
  LOCAL_TAG=1
fi
latest="$(git tag -l 'v[0-9]*.[0-9]*.[0-9]*' | grep -vx "$TAG" | sort -V | tail -n1 || true)"
if [ -n "$latest" ] && [ "$(printf '%s\n%s\n' "${latest#v}" "$VERSION" | sort -V | tail -n1)" != "$VERSION" ]; then
  die "$TAG is not newer than the existing tag $latest"
fi

# Only the changelog files may differ from HEAD, so nothing unrelated ends up in the release commit.
unrelated="$(git status --porcelain | grep -vE '^.. (CHANGELOG|ARCHIVE_CHANGELOG)\.md$' || true)"
[ -z "$unrelated" ] || die "uncommitted changes would not belong in the release commit:
$unrelated
Commit or stash them first."

# ---- prepare the release files -------------------------------------------------------------------
BACKUP="$(mktemp -d)"; PUBLISHED=0
for f in CMakeLists.txt CHANGELOG.md ARCHIVE_CHANGELOG.md; do [ -f "$f" ] && cp "$f" "$BACKUP/$f"; done
restore() {
  if [ "$PUBLISHED" = 0 ]; then
    for f in CMakeLists.txt CHANGELOG.md ARCHIVE_CHANGELOG.md; do
      if [ -f "$BACKUP/$f" ]; then cp "$BACKUP/$f" "$f"; else rm -f "$f"; fi
    done
  fi
  rm -rf "$BACKUP"
}
trap restore EXIT

current="$(sed -n 's/^project(SORTED VERSION \([0-9][0-9.]*\).*/\1/p' CMakeLists.txt | head -n1)"
[ -n "$current" ] || die "CMakeLists.txt has no project(SORTED VERSION x.y.z ...) line"
if [ "$(git log -1 --format=%s)" = "$MESSAGE" ] && [ -z "$(git status --porcelain)" ]; then
  say "HEAD is already the release commit for $TAG; checking it"
  "$PY" tools/changelog.py check --tag "$TAG"
  PREPARED=1
else
  PREPARED=0
  [ "$(printf '%s\n%s\n' "$current" "$VERSION" | sort -V | tail -n1)" = "$VERSION" ] || die "$VERSION is lower than the current version $current"
  if [ "$current" != "$VERSION" ]; then
    sed -E "s/^(project\(SORTED VERSION )[0-9]+\.[0-9]+\.[0-9]+/\1$VERSION/" CMakeLists.txt > CMakeLists.txt.new
    mv CMakeLists.txt.new CMakeLists.txt
  fi
  "$PY" tools/changelog.py release "$VERSION"
  "$PY" tools/changelog.py check --tag "$TAG"
fi

# ---- review and confirm --------------------------------------------------------------------------
echo
git --no-pager diff --stat -- CMakeLists.txt CHANGELOG.md ARCHIVE_CHANGELOG.md || true
git --no-pager diff -- CMakeLists.txt CHANGELOG.md ARCHIVE_CHANGELOG.md || true
echo
echo "Release notes for $TAG:"; "$PY" tools/changelog.py notes "$VERSION" | sed 's/^/    /'
echo
echo "This will:"
[ "$PREPARED" = 1 ] || echo "  1. commit CMakeLists.txt, CHANGELOG.md, ARCHIVE_CHANGELOG.md as \"$MESSAGE\""
[ "$LOCAL_TAG" = 1 ] || echo "  2. create the annotated tag $TAG"
echo "  3. git push --atomic origin main $TAG   (GitHub Actions then builds and publishes the release)"
if [ "$DRY" = 1 ]; then say "dry run: nothing was committed, tagged or pushed; your files are restored"; exit 0; fi
read -r -p "Type '$TAG' to publish, anything else cancels: " answer || answer=""
[ "$answer" = "$TAG" ] || { say "cancelled; your files are restored"; exit 1; }

# ---- commit, tag, push ---------------------------------------------------------------------------
if [ "$PREPARED" = 0 ]; then
  git add -- CMakeLists.txt CHANGELOG.md ARCHIVE_CHANGELOG.md
  if git diff --cached --quiet; then say "nothing to commit; tagging the current commit"; else git commit -q -m "$MESSAGE"; fi
fi
PUBLISHED=1   # from here on the files are committed; never roll them back
[ "$LOCAL_TAG" = 1 ] || git tag -a "$TAG" -m "Game Engine $TAG"
if ! git push --atomic origin main "$TAG"; then
  cat >&2 <<EOF

error: the push failed. Your commit and tag $TAG exist locally; nothing was published.
Fix the cause (network, GitHub authentication, branch protection, a newer origin/main), then run
    ./release.sh $VERSION
again: it detects the finished local commit/tag and only pushes. See docs/CI_CD.md.
EOF
  exit 1
fi
say "Pushed $TAG. GitHub Actions is now building the release: $(git remote get-url origin | sed -E 's#(\.git)?$#/actions#; s#^git@github.com:#https://github.com/#')"
