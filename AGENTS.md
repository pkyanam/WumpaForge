# Native Wrath of Cortex workspace

## Objective and authorization
Get the user's supplied Crash Bandicoot: The Wrath of Cortex ISO running as a native
Apple Silicon macOS game, without emulation. The user authorizes local development,
downloads of relevant tools/source, launches, and UI testing. Work resource efficiently.
The user explicitly clarified that ahead-of-time ARM64 recompilation with Xbox
graphics, audio, and system-call compatibility layers is acceptable. Use the
xboxrecomp static pipeline and runtime as needed; no CPU interpreter/JIT fallback.
Do not equate a native emulator executable, title-screen mockup, asset viewer, or
compilable console binary with a working native game.

## Continuity
- Read `docs/STATUS.md` and recent `git log` when resuming.
- Update status with concrete evidence, commands, blockers, and the next useful step.
- Make small descriptive Git commits at meaningful milestones. Do not commit game
  assets, the original ISO, binaries, build output, or dependency checkouts.
- Preserve the supplied ISO in place and treat it as read-only.

## Layout
- `tools/`: reproducible local tooling.
- `docs/`: findings, source provenance, status, and plans.
- `third_party/`: ignored upstream source checkouts; record URLs and revisions in docs.
- `local/assets/`: ignored extracted assets, only as needed.
- `local/reports/`: ignored generated inventories and diagnostics.
- `build/`: ignored build products; `.venv/` for Python packages if needed.

## Resource and validation conventions
- Prefer existing system/Homebrew tools, stdlib Python, shallow clones, and selective
  extraction. Inspect before downloading SDKs or doing large builds.
- Use at most two compile jobs initially on this fanless 24 GB M3 MacBook Air.
- Validate actual native arm64 machine code and playable behavior before claiming
  success. Record incomplete subsystems honestly; do not conceal stubs.
- Ahead-of-time game-code translation and graphics/audio/system compatibility are
  authorized. Verify that game CPU instructions execute as compiled ARM64 code.
- Do not publish assets or contact others without user authorization.
