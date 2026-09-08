# Status — 2026-09-08

## Goal
Run the supplied game natively on Apple Silicon without emulation. **Native startup runs; game title/menu not yet reached.**

## Latest checkpoint: boot30, original copyright screen visible
- Commit174ede7 fixes compatible comparison-state joins. Lift29 corrects actual
  WAV parser138567 and reduces unknown flag branches70→57. Original parser now
  reads fmt/data chunks, confirmed in boot29. Native regression tests pass.
- Commit0a7faa5 adds bounded actual GPU diagnostics. Boot29 draw1 renders stars;
  draw2 covers all pixels with black and near depth~0.592803. This explained the
  black frame while the original intro was held, without changing render state.
- Commit0a16572 connects real packet streaming. Native decode, completion,
  pause/resume/flush/queue/lifetime tests pass (UBSan and actual producer thread).
  Full build30 and bootstrap30 passed. Manual exclusions now90.
- Boot30 clears original audio gate, advances animation300→347.5 at0.5/update,
  and actual frame180 shows original copyright text over a starfield. Captured
  and viewed local/reports/game-frame-180-boot30.png. Title/menu/gameplay not yet
  verified. Sound output quality not heard/verified; debug run reports overloads.
- New exact stop: unresolved original callback31E20 at34820→34C40→34CA0→79060→
  7A310→7AB90→7AC40→7ACC0→2D950. Runtime agent audits/adds seed from original
  emitter callback table199C1C and call34A82. Root owns next analyze/lift/build.
- No game intentionally left running. Audio/graphics sources frozen; no pending
  production compile. docs/research/FLAG-FALLBACK-AUDIT.md and CSV inventory57
  remaining flag branches; some are decoded data/tail aliases, others real gaps.
  In particular input6E130 has two mixed/back-edge joins needing faithful ZF work.
- Goal remains active. Next: compile the evidenced callback and re-run through
  original intro toward menu. Preserve max2 build jobs, native-only CPU and original
  assets/control flow; later verify undisturbed sound and physical BT controllers.

## Previous checkpoint: boot28, research complete and intro hold measured
- Research completed in bcd1249; docs/research/README.md links four primary-source
  audits. Graphics implementation before the pause is86b9630. Decision: continue
  current AOT/native SDK route with targeted decomp references, no wholesale pivot.
- Post-research boot27/28 read-only LLDB probes prove fade255→0 after32 intro
  updates, then stays0 through watchdog (~539 swaps in boot28). Animation position
  stays300 with increment0; intro0, scene0, audio gate1, pause0. Frame120 boot27
  captured/viewed still black. These runs used the existing build, no behavior edits.
- Boot28 early channel4 index−1/kind0 and intro_mode2 later become index0/kind2
  and mode255; cut_audio172 throughout. Audio investigator traces original stream
  status5D4B0 because the initial mode2 gate clears but animation remains held.
  Stream wrapper for index0 is431ED8, relevant byte+24 and COM pointer+4. Do not
  force animation to advance or label missing stream creation as proven until traced.
- First8 post-fade draws captured in local/reports/boot-28-intro-draw.log: native
  color mask15 and cull NONE; guest color-mask cache0 is not proof writes are off.
  Actual fixed FVF152 geometry and142 immediate quads, matrices/vertices/stages are
  recorded. Draw2 has disabled texture stages and black diffuse, depth enabled/write;
  later sprites have white diffuse and alpha GREATER247. Several transformed sprite
  vertices fall inside clip bounds. Need actual fragment/target/depth evidence to
  explain black output; missing fixed combiners alone is not yet its proven cause.
- tools/intro_probe.py provides reproducible bounded snapshots; invoke through
  `python3 tools/boot.py NAME --seconds 18 --probe-intro`. It
  alters debugger timing, so logged wall time is not a performance measurement.
  Boot27/28 exited; no game intentionally left running. Next: finish stream-status
  trace and actual rendering ledger, then fix a reproduced contract.

## Implementation state through boot26
- Original XBE CPU code executes as AOT-compiled ARM64. Universal splash/fade
  verified; actual green loading object frame40 was captured and viewed at
  `local/reports/game-frame-40-boot23.png`. No verified Crash title/menu/gameplay.
- Boot26 completes loading, joins its worker, initializes native SDL CoreAudio,
  and enters original intro/cutscene loop2D950. Watchdog samples indexed rendering
  through A8140→A0990→9F370→A8240→A5D40→3B010 and P8 texture upload. Actual GPU
  frame120 (`local/reports/game-frame-120-boot26.png`) is solid black. This is not
  a proven title loop or a proven expected transition. Boot26 bounded run exited.
- Immediate Begin101EC0/End101F00 and SetVertexData4f101E60/2f101E20 are now native;
  fixed/programmed GPU tests cover real attributes, position emission and state
  restoration. `docs/IMMEDIATE-DRAWS.md` records retail4361 ABI evidence.
- Fixed XYZ draws now consume original device matrix cache (VIEW0, PROJ1, WORLD6),
  preserve original SetTransform execution, upload D3D row-vector matrices correctly
  and convert D3D clip depth into OpenGL clip depth. GPU test fixed_mvp covers
  noncommuting matrices, perspective, XYZRHW preservation and depth/viewport reads.
- Combined graphics smoke tests and native build26 passed. These component tests
  do not establish that every state of the actual intro is implemented correctly.
  Manual SDK exclusions total83. Graphics patch preserves both GL source and CMake.
- Committed heap/release fix6afd888 preserves guest DWORD layouts and reuses released
  allocations. Boot23 loading used~36.7MiB of53.875MiB heap. Registered callbacks
  2B880/2B950 execute as compiled code (1adb817). Runtime worker exit and audio
  integration are checkpoint4de3375; thread-object views are per-query snapshots.
- Audio initializes and original worker5DA30 runs; game sound is not heard/verified.
  DSP effects/HRTF/spatial features remain unsupported. Guest workers must stop
  before audio teardown and guest-memory unmap; clean exit remains incomplete.
- Three agents audited existing decomp projects, Xbox graphics contracts and
  startup/runtime behavior; root audited ARM64/AOT. Reports live in docs/research.
  No new game launch occurred during that research pause; probes followed it.
- Startup audit establishes fade255 decreases by8 per intro update, independent of
  wall-clock queries. Next diagnostic should pair exact fade/animation/selector
  state with actual draw state and pixels. Do not speculate that black means timing.
- Known graphics gaps include fixed texture combiners/sampling and multiple stages,
  mixed fixed/programmed pairs, advanced dependent modes, fog, volume textures,
  packed attributes and stream>0. P8 reuploads are a possible performance cost.
- Physical Xbox/DualSense Bluetooth input remains untested; virtual SDL tests pass.
  App bundle is stale. Use max2 compile jobs; preserve source/patches in Git and keep
  game assets/generated binaries ignored. Goal remains active and incomplete.

The older numbered checkpoints below are historical; this section is current.

## Environment
- Workspace started with only a 931 MiB ISO and `.DS_Store`; initialized Git on `main`.
- Host reports arm64. Xcode beta, Homebrew, Python 3, CMake, and SDL2 are present.
- Input: `Crash Bandicoot - The Wrath of Cortex (USA).iso` (preserved in place).
- Original Xbox XDVDFS confirmed; ISO 9660 is a dummy descriptor.

## Initial upstream findings
- https://github.com/Open-Travelers/OpenCrashWOC : GameCube-derived C reimplementation,
  no releases, README says gamecode 70%, build WIP, incomplete engine and dummy sound.
- https://github.com/denzi-gh/crashwoc-decomp-ps2 : active matching decompilation targeting
  PAL v1.03 SLES-50386, not a native host port. No releases.
- https://github.com/denzi-gh/crashwoc-decomp-gc : matching GameCube USA decompilation,
  no releases; branches include multiplayer and custom text, no advertised native port.
- https://github.com/calmsacibis995/crash-ps2 : USA v1.00 project explicitly unbuildable
  and very incomplete.

## Confirmed disc / selected approach
- Original Xbox XDVDFS at partition base 0; the ISO9660 descriptor is a dummy.
- Extracted only `default.xbe` (1,777,664 bytes) to `local/assets/`.
- Title `Crash Bandicoot: tWoC`, ID `0x56550003`, North America, certificate version 1.
- Build 2002-03-19, XDK 4361, entry point `0x000EF089`, 113 kernel imports.
- Inventory and XBE analysis in `local/reports/`; about 972 MB of file contents on disc.
- OpenCrashWOC CMake configures but compile fails immediately on incomplete C;
  it still uses GameCube GX and is not an available native game implementation.
- User explicitly accepts native ARM64 ahead-of-time recompiled game code using
  compatibility layers for Xbox graphics, audio, and system calls.
- Selected `third_party/xboxrecomp` (revision in UPSTREAM.md). No CPU emulation/JIT.
- Python venv installed only Capstone 5.0.7 (native arm64 wheel).

## Native build progress
- Fully extracted 2,267 disc files into ignored `local/assets/` for runtime I/O.
- `tools/pipeline.py` reproduces prepare/assets/analyze/lift stages, pinned to upstream.
- Disassembly recovered 4,627 functions (3,056 in main .text). C lift generated 689,660
  lines; 66 unresolved targets and 107 unsupported instruction instances are tracked.
- Generated unsupported instructions and unresolved calls now stop with diagnostics
  when reached, rather than silently skipping logic. See `patches/xboxrecomp-lifter.patch`.
- Generated ARM64 objects compile after handling a privileged debug-register move as
  an explicit unsupported-instruction diagnostic.
- Native host in `src/`; CMake build is `build/native`, max two jobs.
- Runtime port fixes are being saved in `patches/xboxrecomp-runtime.patch`.
- User permits helpful subagents. Controller agent implemented/tested native SDL2
  input and is inspecting D3D integration. Runtime agent fixes safe macOS mappings.
  Audio agent implements missing POSIX output/cadence.
- Homebrew libepoxy 1.5.10 installed (2.7 MB). SDL2 here is sdl2-compat over SDL3.

## First native boot (13:33 EDT)
- `build/native/wrath_native` links, 8.2 MB Mach-O arm64 executable.
- Real recompiled XBE entry executes memory/CRT initialization. Boot log:
  `local/reports/boot-01.log`. Exits through HalReturnToFirmware after kernel ordinal
  202 (`NtOpenFile`) returns C0000034. This is a game startup failure, not success.
- LLDB follow-up identifies failed path as bare `\\Device\\CdRom0`; path mapper only
  accepts a trailing slash. Runtime agent fixing actual device-root handling.
- Native controller virtual-device tests and audio dummy/CoreAudio tests pass.
  Physical controllers and in-game audio are not yet tested.
- Input agent identified exact XDK 4361 D3D API addresses using signature matches
  and local disassembly, implementing first four hooks in `src/graphics.c`.
- `tools/bootstrap.py` pins checkout, applies tracked patches without overwriting
  other local changes, and installs only the pinned Capstone dependency.

## Input fingerprints
- ISO SHA-256: `1caef217dd588655a851ce5ae3cdead912bf383ae86e574e9b33aa74c12900bc`
- XBE SHA-256: `e8d7cbf225d899eb88227c11d1f40434c34e27c1b23ed946fb2c3471168d2f4d`

## Current checkpoint (boot-10)
- Real native ARM64 entry and CRT execute. Disc root, optical media query, save
  directory opens, title metadata creation and sparse HDD metadata reads now pass.
- Safe guest runtime layout starts above the XBE image, avoiding its large .data.
  Sparse 4 GB virtual reservation has 64 MB of guest RAM; it is not 4 GB resident.
- Boot-06 reached first game main at 0x00087BF0 (LLDB breakpoint). No actual
  title/menu visible yet. Boot-07 crashes on NtDuplicateObject from game main: a
  current-thread pseudo handle was being treated as a native handle pointer. Fixed
  and tested independent duplicate lifetimes/current-thread identity; boot-08 passes.
- Boot-08 creates the real native 640x480 graphics device and a guest texture-level
  surface. It then stalls in the original D3D pushbuffer routine from render-state
  initialization (FD830 ->103740). Boot-09 passes that boundary with actual native
  blend/depth/cull state hooks, then stalls on FillMode (FDDF0). Graphics agent is
  handled FillMode, passthrough texture coordinates, and enabled alpha test with
  actual fragment discard. Boot-10 passes material setup, loads actual2.7MB and
 786432-byte asset reads, then stalls at FEF20 SetRenderTarget. Graphics agent is
  adding FEF20/FF830 native default-color/depth handling. No title/menu pixels verified.
- Boot-05 aborts on a missing target inside CRT memmove 0xF5DD0: function discovery
  interpreted inline jump-table data as instructions. Native audited cdecl memmove
  bridge added; overlap/alignment/stack tests and boot-06 confirm the fix.
- Native D3D create/clear/swap, texture/VB and non-indexed draw bridges are integrated.
  Synthetic textured-quad readback passes on GL4.1 Metal; this is not game rendering.
  Programmable shaders, indexed draws, render targets and other state need work.
- Seven native XAPI controller bridges integrated. Virtual-device tests pass;
  physical Bluetooth Xbox/DualSense testing remains pending.
- Native CoreAudio output works in isolated tests. Real game sound is not integrated;
  startup creates 252 Xbox ADPCM buffers, requiring decoding and lazy voice allocation.
- Runtime details: docs/RUNTIME.md. Graphics/input ABI evidence: D3D-INTEGRATION.md,
  INPUT-INTEGRATION.md. Build max two jobs; execute from local/run to contain logs.

## Next
Run the rebuilt native game through CRT/cache startup and stop on concrete faults.
Implement the next actually reached compatibility or translation gap, preserving
original game behavior. Keep all stubs/limitations explicit. Verify real title/menu
pixels and then gameplay/input before reporting success.

## Reproducible bounded boot
`python3 tools/boot.py boot-NN` writes the LLDB report under local/reports.
Use `--break-at sub_00087BF0` to verify game-main entry. Real crashes need LLDB
`-k` commands (the helper includes them); process exit 0 is not game success.

## Work after the graphics-init checkpoint
- Audio worker entry 0x5DA30 seeded from its actual CreateThread argument; analyze
  now finds it (4628 functions total). Four-pthread memory regression verifies
  independent guest TIB/TLS/registers/stacks and synchronized heap allocation.
- Recompiler chunk boundaries now retain original function positions when native
  overrides omit bodies; stable sorted declarations avoid unrelated C rebuilds.
  `tools/test_incremental_lift.py` and diagnostic lifter test pass.
- Audio agent completed a native Xbox ADPCM decoder/lazy buffer-voice backend,
  tested 252 idle buffers and bounded64 playing voices; patch/docs handoff pending.
  It will next work on exact title-specific audio bridges in src/audio_bridge.c.
- Computer-use skill read; node_repl has `sky` imported. Use that skill/API to
  inspect the actual game window once rendering advances. No UI screenshot yet.

## Latest handoff details
- boot-10-surfaces.log breaks at FEF20: color[1E3CC8]=019E1030 from our GetBackBuffer,
  depth[1E3CCC]=0 and fallback[1E8D60]=0. Color header six DWORDs:010D0001,019E1080,
  0,00011221,271DF27F,0. Singleton+2070=color,+2074=0,+207C=color,+2080=0.
- Runtime agent finished and frozen. Graphics agent active on FEF20/FF830 in
  src/graphics.c/docs. Audio agent active on new src/audio_bridge.c, only verified
  interfaces, explicit errors for unsupported effects/streams/spatial features.
- Native backend/worker/incremental-lift checkpoint ad1a6db; prior game-main/native
  graphics checkpoint ee6b9e9. Current graphics changes await next checkpoint.
- All five tracked toolkit patches pass idempotent bootstrap (including new
  graphics shader patch). Last native build/boot10 complete, no game left running.
