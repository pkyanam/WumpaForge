# Status — 2026-09-08

## Goal
Run the supplied game natively on Apple Silicon without emulation. **Native startup runs; game title/menu not yet reached.**

## Latest checkpoint: boot-15, correct Universal splash
- Real native ARM64 game code renders the correct white-background Universal
  splash. Actual GPU capture `local/reports/game-frame-15-fixed.png` was viewed.
  This is original game rendering, not an asset viewer or injected image.
- Native vblank callback invokes compiled game39910 at60/50Hz and passes the
  two-second hold. Boot15 reaches3D intro, but title/menu/gameplay remain unverified.
- Incorrect colors were proven a generic x87 FIST/FISTP rounding bug: original
  D3DX requests truncation, but old lift used nearest llrint, converting255.5 to256.
  `tools/test_x87_rounding.py` covers guest/host rounding modes, bounds and the
  exact white-byte sequence under sanitizers. Cumulative lifter patch fixed;
  sources regenerated and native build completed with2 jobs. Real capture confirms.
- Both128x128 texture render-target calls now pass in boot15. `src/graphics.c`
  uses real GL FBO storage and resolves GPU pixels into guest texture storage on
  switches. Native graphics tests pass offscreen drawing, orientation, target
  rebinding and resource lifetime using the CURRENT build/native D3D library.
- Boot15 still aborts at programmable pixel shader0x2606890 after rejecting vertex
  shader0x2606701. Cubemap resource AddRef/Release warnings also remain. No reliable
  time estimate to title/menu is established; shader integration is substantial.
- `src/nv2a_vertex.{h,c}` standalone generator is frozen and tested with native GPU
  transform feedback plus actual six-instruction shader compilation. Inputs v0/v3,
  physical constants112,113,119,120. It is NOT yet integrated/CMake-linked.
- `src/nv2a_pixel.{h,c}` standalone generator exists; native_audio agent is testing
  it. It is NOT yet integrated. Both use GLSL410 varyings vD0/vD1/vT0..3/vFog.
- Real fixture `boot14-shader-objects.bin` maps guest2606000..2608000. Vertex
  object2606700+118 contains six instructions (opcode in word1, not toolkit word0).
  Declaration slots at object+14+16*slot: stream,offset,format,tess bytes. Observed
  v0 offset0 float3(32), v1 offset12 float1(12), v2 offset16 color(40), v3 offset20
  float2(22), stride28. Pixel object+8 points to60-DWORD definition at260689C.
- NEXT root integration: audited SetVertexShaderConstant102AA0(index,data,count),
  ret12, physical index=index+96. Original pure-device mode skips guest cache:
  capture uploads in native storage. Shader0 programs require source generation,
  GL compilation/link/cache, attribute binding, native constants/state/texture
  upload. No CPU interpreter/JIT. Original pixel constant setter102D80 needs audit.
- Agent ownership: mac_runtime finishing declaration/constant ABI docs; native_audio
  testing standalone pixel generator; controller_support now confirms unbridged
  cubemap CreateFE9F0/GetSurface103CB0 caused resource warnings. Root graphics.c
  owns RTs/integration. Avoid shared edits and simultaneous native game runs.
- Audio decoder, native output, lazy voices and28 title SDK bridges are tested
  independently. `src/audio_bridge.c` is not yet linked into the real executable.
  Physical Bluetooth controller testing and real in-game audio remain pending.
- `tools/package.py` creates build/Wrath Native.app with asset symlink. Repackage
  after building; existing app copy is stale. CLI build/native/wrath_native current.

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
