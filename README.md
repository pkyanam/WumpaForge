# Wrath of Cortex — native Apple Silicon port in progress

**Development build; gameplay validation is in progress.** This workspace statically translates the
supplied original Xbox executable into C and compiles it to native ARM64. The user
approved Xbox graphics/audio/system compatibility layers; no Xbox CPU interpreter
or JIT is used. See [current status](docs/STATUS.md) and `git log` for progress.

The original ISO stays read-only at the workspace root. Game assets, generated
sources, build outputs, virtual environments, and upstream clones are ignored by Git.

## Launch the local build

After building, run `python3 tools/package.py`, then open `build/Wrath Native.app`.
The app finds the local extracted assets automatically and has no diagnostic
watchdog unless one is explicitly enabled in the environment. The bundle links
to this workspace's assets and currently relies on its local native libraries.

Use WASD to move, Space to jump/confirm, X to spin, and Enter to pause. Space can
skip the story and hub hologram. Walk into portal1 and wait to enter Arctic Antics.
See [all controls](docs/CONTROLS.md) for mouse and gamepad mappings.

The window can be resized or maximized. F11 (or Control–Command–F) toggles
fullscreen; F10 toggles sharpening. Output preserves the original4:3 aspect
ratio. The game still renders internally at640x480; larger output is scaled.
See [presentation](docs/WINDOW-PRESENTATION.md) for validation and limitations.

## Reproduce

Existing host tools: Xcode Command Line Tools, Python 3.11+, CMake, Homebrew SDL2,
libepoxy, pkg-config, and OpenSSL. If missing, the Homebrew packages are `sdl2
libepoxy pkg-config openssl@3 cmake`.

```sh
python3 tools/bootstrap.py
.venv/bin/python tools/pipeline.py prepare
.venv/bin/python tools/pipeline.py analyze
.venv/bin/python tools/pipeline.py lift
.venv/bin/python tools/pipeline.py assets
cmake -S . -B build/native -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build/native --parallel 2
file build/native/wrath_native
```

`prepare` extracts only `default.xbe` and inventories/hashes the disc; `assets`
extracts the remaining files when ready to run. Existing extracted files are
preserved. All stage logs go into `local/reports/`.

## Diagnose startup

```sh
python3 tools/boot.py boot-next
```

The helper captures crash backtraces in `local/reports/boot-next.log` and bounds
development runs. It is not a playable launcher. Reaching
missing generated code deliberately stops with the original Xbox address; it must
be fixed before claiming functional behavior.

Controller and audio backend tests are documented in [CONTROLLERS](docs/CONTROLLERS.md)
and [AUDIO](docs/AUDIO.md). Both are independently tested; game integration remains
in progress. Source URLs, versions, and attribution are in [UPSTREAM](docs/UPSTREAM.md).
