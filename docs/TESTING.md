# Source regression checks

The focused tests exercise translated CPU instructions and native compatibility
contracts. Passing them does not establish whole-game correctness, constant60FPS,
physical Bluetooth support, or the absence of crashes in untested scenes.

After the README setup has installed the pinned toolkit and Python dependencies,
run the CPU-only synthetic suite from the repository root:

```sh
.venv/bin/python tools/check.py
```

This runs the existing checks sequentially, with at most one compiler process.
It does not launch the game, open graphics windows, play audio, or read the ISO.
The tests construct their own inputs. Covered areas include instruction boundaries,
incremental translation, guest frame/call boundaries, flag joins, integer shifts
and rotations, guest x87
rounding/classification, thread-local floating-point state, guest SHA context
layout/standard vectors, remainder status and diagnostic reads
with unavailable debugger metadata. Native arithmetic fixtures use Clang and,
where specified by the individual test, UBSan instrumentation.

To also test the original game's CRT64 shift/division helpers and floating
remainder loop:

```sh
.venv/bin/python tools/check.py --suite all
```

The original-function checks require the user's extracted `default.xbe` and completed local
analysis reports. The runner rejects missing inputs before starting; it does not
silently treat an unavailable original-function test as a pass. Original function
bodies are translated into temporary CPU executables. Nothing is uploaded and
these outputs are not added to Git. `--suite original` selects only those checks.

`--list` prints the selected commands without running them. Each run writes logs
and a JSON result under ignored `local/reports/checks/`, and stops at the first
failure. The result records script exit codes and elapsed time, not a game-wide
coverage percentage. Run with the checkout's `.venv/bin/python`, so the scripts
see the pinned translator dependencies.

## Graphics, audio and input

The GPU checks require a macOS graphics session. After the README prerequisites
and `python3 tools/bootstrap.py` (or full setup), build the component targets:

```sh
cmake -S . -B build/checks -DWRATH_COMPONENT_CHECKS_ONLY=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_OSX_ARCHITECTURES=arm64 -DOPENSSL_ROOT_DIR=/opt/homebrew/opt/openssl@3
cmake --build build/checks --target wrath_graphics_check wrath_filter_check --parallel 2
build/checks/wrath_graphics_check
build/checks/wrath_filter_check
```

This configuration requires no extracted assets or generated game C. The first
executable opens a temporary synthetic graphics window; the second uses a hidden
CGL context. Neither launches the game or plays audio. The graphics target uses
the actual native backend, including its AppKit build dependencies. It covers
resource/state/shader/target contracts and the active-texture lock regression.
The filter target checks HD/FHD/QHD pixels, all sharpening strengths, alpha and
aspect-ratio bars. Assertions remain enabled even in release configurations.
Both targets are excluded from the normal game build, so they add no routine
compilation work. Root verified both through this CMake path on Apple M3.

Audio and input fixtures remain separate. Coordinate visible UI execution with
the primary agent, and do not relaunch the game just to run source checks.

- [Presentation and sharpening](WINDOW-PRESENTATION.md): independent filter pixels,
  actual drawable sizes, menu actions, displayed buffers, and measurement limits.
- [Surface aliases](SURFACE-ALIAS-COHERENCE.md), [shared depth](TEXTURE-DEPTH-TARGETS.md)
  and [retained vertex arrays](TANGENT-STREAM-DIAGNOSIS.md): synthetic GPU contracts.
- [Vertex arithmetic](research/VERTEX-NUMERIC-AUDIT.md) and
  [pixel arithmetic](research/PIXEL-NUMERIC-AUDIT.md): independent numeric oracles.
- [Spatial audio](AUDIO-SPATIAL.md): actual PCM output, deferred state and thread
  stress. Numeric output tests do not substitute for listening in the game.
- [Controls](CONTROLS.md): virtual controller/guest ABI evidence and the outstanding
  physical Xbox/PS5 Bluetooth checks.
- [AOT arithmetic](research/AOT-CORRECTNESS-AUDIT.md) and
  [mixed flag branches](research/MASK-ANGLE-FLAG-AUDIT.md): counterexamples,
  original call-site evidence, exact validated cases, and remaining limitations.

Read [STATUS](STATUS.md) before using historical commands: old build numbers,
process IDs and next-step instructions may be superseded. Run the tests affected
by a change; repeat broader suites only for an integration change, failure or
unresolved concern. All agents share the two-compiler-job limit in
[AGENTS.md](../AGENTS.md).
