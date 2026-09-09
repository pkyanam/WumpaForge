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
Keyboard and mouse controls are also required, scoped to the focused game window.
The next playable milestone is at least one of the first five levels in the first
realm; user now specifically requests the winter/penguin level in area1. Keep
the original intro/story available to watch or skip. Maintain correct
assets, audio synchronization and a 60 FPS performance target; report measured
performance and unverified physical hardware honestly.
After initial playability, the user also requests a resizable/maximizable window,
fullscreen, and optional efficient sharpened upscaling to HD/FHD/QHD output.
Keep original internal rendering and aspect ratio separate from output sizing;
measure cost and latency rather than promising zero performance loss.
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

## Private staging and future platforms
- Project name: WumpaForge. The user authorizes creating and pushing the private
  `pkyanam/WumpaForge` GitHub repository via `gh`. Keep it private; public release
  and prebuilt binary distribution are separate future decisions.
- Audit tracked files and history before uploading. Never force-add the ISO,
  extracted assets, generated game code, game captures, credentials or binaries.
- Contributors bring their own supported Xbox ISO; extracted assets are needed
  at runtime as well as during setup. Preserve existing upstream license notices.
  A future restrictive commercial license must not be promised for third-party work.
- macOS Apple Silicon is the only validated platform. Document future ARM64 OS
  seams without claiming iOS/Android/Shield support from CPU compatibility alone.
- The user completed Arctic Antics on build61. Retained physical vertex-fetch
  bounds on return to the hub were fixed in build63 and need live verification.
- Root coordinates full builds and game/UI tests. Run at most two compiler jobs
  total. Parallel agents should own disjoint files and commit bounded changes.

## September 8 evening wrap-up
- The user requested continued work until midnight America/New_York, wrapping by
  2026-09-09 00:00 EDT (04:00 UTC). Reserve the last15 minutes for validation,
  packaging, private push and a candid handoff; avoid unreviewed late changes.
- Add a generated fruit/crate app icon (root owns branding/package tooling),
  discoverable1440p output and sharpening controls (mac_runtime owns presentation),
  and test actual displayed pixels and performance. Output scaling is not new
  internal scene detail; document actual drawable size on Retina/fullscreen.
- Finish with a primary-source Shield/AndroidTV plan and small validated source
  scaffolding only. No promised working APK or unrequested large SDK downloads.
- Newly generated project branding in assets/branding is intentionally tracked;
  original game assets, captures and binaries remain excluded.

- Latest validation preference: the user reported a glitched attract demo and
  requests source audits plus synthetic CPU/GPU tests instead of further live
  game testing for now. Report per-path evidence, not a made-up99.5% guarantee.
- Shield scope is specifically NVIDIA SHIELD TV Pro2019 (mdarcy), not other models.

## Midnight handoff checkpoint
- The September 8 evening work is wrapped. Build 68 is packaged from source
  ab6d29d; later commits update packaging metadata and handoff documentation.
- All implementation agents are finished. The requested first-level milestone
  was demonstrated by the user's completed Arctic Antics run on build 61.
  Build 68 passed CPU/GPU component checks but has not had another live play pass.
- Keep the latest source-only validation preference until the user requests game
  testing. Resume further development on user instruction; the elapsed midnight
  deadline is not authorization for a recurring or indefinite background task.
- Read current STATUS, README, CHANGELOG and Git history. Preserve known gaps;
  do not reinterpret a passing fixture as whole-game correctness or crash freedom.
