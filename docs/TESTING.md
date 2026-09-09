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
incremental translation, flag joins, integer shifts and rotations, guest x87
rounding/classification, thread-local floating-point state, guest SHA context
layout/standard vectors, and diagnostic reads
with unavailable debugger metadata. Native arithmetic fixtures use Clang and,
where specified by the individual test, UBSan instrumentation.

To also test the original game's CRT64 shift/division/remainder functions:

```sh
.venv/bin/python tools/check.py --suite all
```

Those two checks require the user's extracted `default.xbe` and completed local
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

These require separate fixtures and sometimes a macOS graphics session. They are
intentionally separate from the CPU runner. Keep visible UI execution coordinated
with the primary agent, and do not relaunch the game just to run source checks.

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
