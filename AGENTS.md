# WumpaForge contributor and agent guide

## Project scope

WumpaForge builds a native Apple Silicon macOS app from a user-supplied supported
USA Xbox disc image of Crash Bandicoot: The Wrath of Cortex. The game CPU code is
ahead-of-time recompiled to ARM64; Xbox graphics, audio and system calls use native
compatibility layers. Do not add a CPU interpreter or JIT fallback, and do not
present a native emulator executable or asset viewer as a native game port.

Read README.md for setup, docs/STATUS.md and recent Git history for current
validation, and docs/LICENSING.md and THIRD_PARTY_NOTICES.md before distribution
changes. The Android experiment is isolated under android/shield2019 and has its
own AGENTS.md. The public release workflow focuses on macOS.

## User input and repository contents

- Treat the supplied ISO as read-only. Never download or redistribute the game.
- Keep the ISO, extracted assets, generated game C, game captures, executables,
  dependency checkouts and credentials out of Git and release attachments.
- Source releases contain tooling, compatibility source, patches, synthetic tests
  and documentation. Locally built game apps contain user-supplied material and
  are not public release artifacts.
- Preserve upstream licenses and notices. A project license does not relicense
  the retail game or override component-specific GPL/LGPL and other terms.
- Newly generated project branding under assets/branding is intentionally tracked;
  its provenance is documented in docs/BRANDING.md.

## Workspace and resource conventions

- `tools/`: reproducible setup, packaging, diagnostics and tests.
- `src/`, `patches/`: native compatibility implementations and upstream changes.
- `docs/`: architecture, provenance, measured validation and known limitations.
- `third_party/`: ignored dependency checkouts at documented revisions.
- `local/assets/`, `local/reports/`: ignored extraction and diagnostic outputs.
- `build/`, `.venv/`: ignored build products and Python environment.
- Use at most two compile jobs total, including parallel agents. Prefer incremental
  builds, existing tools and bounded disjoint agent tasks where they save time.
- Keep changes small and descriptive in Git. Never overwrite unrelated work.
- Record concrete commands, results and remaining gaps in status documentation.
  A passing fixture does not prove whole-game correctness or crash freedom.

## Guest ABI and runtime correctness

- Guest pointer and size fields remain 32-bit DWORDs on ARM64. Read/write them
  explicitly; never pass their addresses to host APIs expecting pointer-sized
  output fields.
- Guest virtual releases must return exact owned blocks through
  xbox_HeapFreeChecked. Preserve tested heap splitting/coalescing and image-derived
  stack sizing.
- Keep callback-registration evidence with additional AOT function seeds.
  Unresolved code must be compiled or implemented faithfully, never silently skipped.
- Treat guest registers, dispatch selection and thread clocks according to their
  actual thread ownership. Check both generated source and compiled symbols when
  validating fixes that depend on thread-local storage.
- Apply patches to their intended upstream revision and verify actual mutations;
  successful command exit alone is insufficient when Git can discover a parent
  repository and silently skip nested build-directory patches.

## Graphics, audio and input

- Loading screens render from worker2BF30. Serialize graphics/resource access
  across game threads; do not restore a blanket main-thread-only guard.
  graphics.c acquires once per SDK entry through arg()/graphics_thread() and
  releases in finish(). Never retain the native graphics lock across arbitrary
  guest code without proving guest-lock and wait ordering.
- Keep SDL creation/events on the Cocoa main thread. The Mac backend uses native
  CGL context handoff and worker presentation with a monotonic refresh deadline.
  Do not call SDL window APIs from workers.
- The graphics toolkit patch includes src/d3d/d3d8_gl.c and its CMakeLists.txt
  OpenGL framework linkage. Preserve both when regenerating the patch.
- Pair original scene/fade/animation state with draw state and actual pixels before
  changing timing or graphics conventions to explain a black frame. Consult
  docs/research/README.md and relevant audit notes.
- Keep original intro/story playback available to watch or skip. Preserve audio
  synchronization, asset loading and controls while optimizing frame time.
- Keyboard/mouse controls must remain scoped to the focused game window. Use SDL
  controller mappings for Bluetooth Xbox One/Series and DualSense controllers;
  synthetic input tests do not validate physical pairing or rumble.
- Preserve resizable/fullscreen presentation. Keep original internal resolution
  and aspect ratio separate from HD/FHD/QHD output sizing and sharpening.

## Validation and release claims

The 60 FPS target applies to gameplay, hub and transitions; report measured scene
performance rather than promising zero lag or universal performance. Upscaling
changes output presentation, not the original scene detail. Check ARM64 machine
code and linked dependencies when packaging. Preserve known unimplemented paths
and visual limitations in documentation.

Run focused checks appropriate to changed code. Asset-free CI covers source and
synthetic checks; it cannot validate a complete game build or gameplay. Use only
user-supplied game material for local validation. Document physical testing
separately from offline tests and do not assume availability of someone's device
or permission to interrupt its screen from a prior development session.
