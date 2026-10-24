# CI/CD and releases

The pipeline is `.github/workflows/ci.yml`. Every build step is a call to `./build.sh`, so CI runs the same commands
as a developer machine.

| Job | Runs on | What it does |
| --- | --- | --- |
| `linux` | ubuntu-24.04 | `./build.sh --release --sdl fetch --werror --package`: builds with a statically linked SDL3, runs all tests, creates `game-engine-demo-<version>-linux-x86_64.tar.gz` + `.sha256`, unpacks it in an empty environment and runs it |
| `linux-checks` | ubuntu-24.04 | Debug with AddressSanitizer + UBSan, and Release without assembly (C++ fallbacks only), both `--werror` |
| `windows` | windows-latest | MSVC x64 + NASM: `./build.sh --release --sdl fetch --package` (static runtime, static SDL3), tests, creates `game-engine-demo-<version>-windows-AMD64.zip` + `.sha256`, unpacks it and runs it |
| `release` | ubuntu-24.04 | Only for tags `v*`, after the three jobs above pass: checks the tag matches the CMake version, merges the checksums into `SHA256SUMS.txt`, and publishes a GitHub Release with both packages |

Pushes to `main`/`master`, pull requests and manual runs (Actions tab -> CI -> Run workflow) build and test; the packages are
kept for 90 days as workflow artifacts.

## Publishing a release

1. Set the version in `CMakeLists.txt` (`project(GameEngine VERSION x.y.z ...)`) and commit.
2. `git tag vx.y.z && git push origin vx.y.z`
3. When the run finishes, the Releases page has `game-engine-demo-x.y.z-linux-x86_64.tar.gz`, `...-windows-AMD64.zip`, a `.sha256`
   file for each, and `SHA256SUMS.txt`. If the tag and the CMake version differ, the release job stops with an error.

No secrets are needed: the release job uses the built-in `GITHUB_TOKEN` with `contents: write`. If the repository or organisation
restricts workflow permissions, allow "Read and write permissions" under Settings -> Actions -> General.

## Windows status

The Windows job was written without a Windows machine and has **not been run yet**. Expect the first run to need small fixes
(missing includes under MSVC, warnings, path details). Send the failing step's log and it can be fixed quickly. Warnings are
deliberately not errors on Windows until the first run is clean; add `--werror` to that step afterwards.

The `.exe` links the C++ runtime and SDL3 statically (`GE_STATIC_CRT`, `GE_SDL=FETCH`), so it needs no Visual C++ redistributable
and no `SDL3.dll`. To build it by hand on Windows, open an "x64 Native Tools Command Prompt for VS" (or run `vcvars64.bat`),
install NASM and Git, and run `bash build.sh --release --sdl fetch --package` from Git Bash, or use the `windows-release` CMake preset.

## Local equivalent of the Linux job

```
./build.sh --release --sdl fetch --werror --package   # needs the X11/Wayland/ALSA dev packages listed in README.md
```
