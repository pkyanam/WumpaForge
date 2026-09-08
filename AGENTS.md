# Native Wrath of Cortex workspace

## Objective and authorization
Get the user's supplied Crash Bandicoot: The Wrath of Cortex ISO running as a native
Apple Silicon macOS game, without emulation. The user authorizes local development,
downloads of relevant tools/source, launches, and UI testing. Work resource efficiently.
The user explicitly clarified that ahead-of-time ARM64 recompilation with Xbox
graphics, audio, and system-call compatibility layers is acceptable. Use the
xboxrecomp static pipeline and runtime as needed; no CPU interpreter/JIT fallback.
Wireless Xbox One/Series X and PS5 DualSense controllers paired through macOS
Bluetooth must be supported. SDL's game-controller mappings are the host interface.
The user permits subagents when they speed the work up; use bounded disjoint tasks.
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

## Guest ABI and memory conventions
- Guest pointer/size fields remain DWORDs on arm64. Read and write them explicitly;
  never pass their addresses to host APIs expecting pointer-sized output fields.
- Guest virtual releases return exact owned blocks through xbox_HeapFreeChecked.
  Preserve tested heap splitting/coalescing and image-derived stack sizing.
- Keep actual callback registration evidence with extra AOT function seeds.
  Unresolved code must be compiled or implemented faithfully, never silently skipped.

## Native graphics conventions
- The original game renders its loading screen from worker2BF30. Serialize native
  graphics/resource access across game threads; do not reinstate a blanket main
  thread-only guard or call SDL window APIs from workers. graphics.c acquires once
  per SDK entry through arg()/graphics_thread() and releases in finish().
- Keep SDL creation/events on the Cocoa main thread. The backend uses native CGL
  for context handoff and worker presentation, plus a monotonic refresh deadline.
- The graphics toolkit patch includes both src/d3d/d3d8_gl.c and its CMakeLists.txt
  (OpenGL framework linkage). Preserve both when regenerating that patch.
- See docs/STATUS.md for the current unimplemented boundary. Native component
  tests and a correct splash do not prove title/menu/gameplay completion.
- Read docs/research/README.md for the architecture audit and next diagnostic.
  Pair original scene/fade/animation state with draw state and actual pixels before
  changing timing or graphics conventions to explain a black frame.
