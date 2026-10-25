# CI/CD and releases

One workflow, `.github/workflows/ci.yml`. Every build step is a call to `./build.sh`, so CI runs the same commands as a
developer machine.

| Job | Runs on | What it does |
| --- | --- | --- |
| `linux` | ubuntu-24.04 | changelog checks; `./build.sh --release --sdl fetch --werror --package` (build, 17 tests, FunkShot `.deb`, SORTED SDK `.tar.gz`); installs the `.deb` and runs it from `/` with an empty environment; builds the SDK example against the unpacked SDK (`tools/sdk-smoke-test.sh`) |
| `windows` | windows-latest | MSVC x64 + NASM + Inno Setup: `./build.sh --release --sdl fetch --package` (build, tests, `FunkShot-Setup.exe`, SORTED SDK `.zip`); silently installs the game, checks the Start Menu shortcut, runs the installed game, uninstalls; builds the SDK example against the unpacked SDK |
| `release` | ubuntu-24.04 | Tags only, and only after both jobs passed: checks the tag is on `main`, matches `CMakeLists.txt` and has a dated changelog entry, that no release for it exists, then publishes **one** release with exactly `FunkShot-Setup.exe`, `funkshot_X.Y.Z_amd64.deb`, `SORTED-SDK-X.Y.Z-windows-x64.zip`, `SORTED-SDK-X.Y.Z-linux-x86_64.tar.gz` and `SHA256SUMS.txt`, using the changelog entry as notes |

Triggers: pushes and pull requests to `main`, and tags `vX.Y.Z`. Newer pushes cancel older runs of the same ref; release
runs are never cancelled. The release commit made by `release.sh` is skipped on the branch run because the tag run builds it.
Permissions are `contents: read`, except the `release` job (`contents: write`, built-in `GITHUB_TOKEN`, no secrets).
Packages from ordinary runs are kept 7 days as workflow artifacts and never published.

The SDL3 build is about 60% of a clean build, so each build job caches `build/release` (key: runner image version +
hash of `cmake/Dependencies.cmake`). Changed sources rebuild; SDL3 does not. Local simulation: 194 s -> 110 s on one core.

## Version and changelog

- The version is `project(SORTED VERSION x.y.z ...)` in `CMakeLists.txt`. Package names, the installer, the release
  title and the tag all derive from or are checked against it.
- `CHANGELOG.md`: the two newest releases (and at most one `## [X.Y.Z] - Unreleased` entry on top). `ARCHIVE_CHANGELOG.md`:
  older releases, newest first. Format: `## [X.Y.Z] - YYYY-MM-DD`, then `### Features`, `### Improvements`, `### Bug Fixes`,
  `### Breaking Changes` (only those that apply), each with bullet points.
- `python3 tools/changelog.py sync` moves older releases to the archive unchanged; `check [--tag vX.Y.Z]` validates and
  changes nothing; `release X.Y.Z` stamps today's date on the Unreleased entry; `notes X.Y.Z` prints the release notes.
  It fails with a clear message on bad headings, duplicate or misordered versions, or a version different from `CMakeLists.txt`,
  and refuses to write if anything could be lost. Running it again changes nothing. Tests: `python3 tools/test_changelog.py`.

## Publishing a release

1. Commit your work to `main`. Add `## [X.Y.Z] - Unreleased` with categorized notes at the top of `CHANGELOG.md`
   (this file may stay uncommitted; any other uncommitted file stops the release).
2. `./release.sh X.Y.Z --dry-run` shows every change and plan without touching anything.
3. `./release.sh X.Y.Z` validates, bumps `CMakeLists.txt`, stamps the date, syncs the changelogs, shows the diff and the notes,
   and waits for you to type the tag name. Then it commits `chore(release): vX.Y.Z`, creates the annotated tag and runs
   `git push --atomic origin main vX.Y.Z`. It uses your existing Git/GitHub authentication; no token is stored.
4. Watch the run on the Actions tab. When it is green the Releases page has the three files.

## Recovery

Nothing is ever deleted or overwritten automatically.

- **Rejected for an existing tag/release:** `vX.Y.Z` already exists on GitHub. Pick the next version. Never move or delete a published tag.
- **Push failed** (network, authentication, branch protection): the commit and tag exist locally and nothing was
  published. Fix the cause and run `./release.sh X.Y.Z` again: it sees the finished local commit/tag and only pushes.
  If `main` moved on origin meanwhile: `git tag -d vX.Y.Z && git pull --rebase`, then `./release.sh X.Y.Z` again (it
  re-creates the tag on the rebased release commit).
- **Commit made, tag missing:** run `./release.sh X.Y.Z` again; it creates the tag and pushes.
- **Tag pushed but the workflow failed** (nothing published): fix the problem on `main` and release the next patch version
  (a tag is never re-used), or, if the cause was a flaky runner, use "Re-run all jobs" on the tag's run.
- **Release job failed halfway:** if a release exists with missing files, delete that release in the GitHub UI yourself
  (keep the tag), then re-run the tag's workflow. The workflow refuses to publish while a release for the tag exists.
- **Changelog errors:** run `python3 tools/changelog.py check`; the message names the file, line and problem.

## Windows notes

The FunkShot installer installs `bin\funkshot.exe`, `share\funkshot\assets\` and docs, per user by default (`/ALLUSERS` or the
dialog for Program Files), with a Start Menu shortcut and an uninstaller. The exe links the C++ runtime and SDL3 statically
(`GE_STATIC_CRT`, `GE_SDL=FETCH`), so no redistributable or DLL is needed. The Windows job has not been run on GitHub yet:
expect the first run to need small fixes; send the failing step's log.

To build the installer by hand: x64 Native Tools prompt, NASM, Ninja and Inno Setup 6 installed, then
`bash build.sh --release --sdl fetch --package` from Git Bash.

The SORTED SDK for Windows is built with the static MSVC runtime, which the example and `docs/USING_SORTED.md` tell users to match.
Its Windows system-library list (`cmake/SORTEDConfig.cmake.in`) is checked only by the CI smoke test.
