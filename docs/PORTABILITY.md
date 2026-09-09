# Future ARM64 platform plan

This is a design seed, not an implemented port. Apple Silicon macOS is the only
current development target. There are no iOS, iPadOS, Android or NVIDIA Shield
builds or device test results. See [STATUS.md](STATUS.md) for the evolving macOS
gameplay evidence and unresolved correctness issues; even macOS is experimental.

Sharing ARM64 makes the generated game C a useful starting point. It does not
make a macOS executable run on another operating system. Each target needs its
own compiler/SDK, ABI-compatible libraries, application lifecycle, graphics
backend, memory mapping and packaging. Future ports must keep the existing
ahead-of-time model: Xbox x86 game code becomes native machine code at build time;
unresolved calls and unsupported instructions remain explicit failures. There is
no CPU interpreter or JIT fallback in this plan. Runtime GPU shader compilation
is a separate graphics operation.

## Existing seams and constraints

Paths under `third_party/xboxrecomp/` refer to the pinned, patched checkout;
reproducible modifications belong in the tracked `patches/` files.

| Area | Current implementation | Future boundary and validation |
| --- | --- | --- |
| Build and launch | Root `CMakeLists.txt` forces Apple arm64, links Homebrew paths and epoxy; `src/main.c` owns startup. `tools/package.py` makes a local macOS `.app`. | Separate host tools (extract/analyze/lift) from target compilation. Select OS and graphics backend explicitly; `APPLE`/`__APPLE__` alone cannot distinguish macOS from iOS. Build all native dependencies for the target; do not reuse host dylibs or objects. |
| Guest CPU and ABI | Generated C in `local/generated/`, bridge decoding in `src/*_bridge.c`, runtime TLS/dispatch and explicit guest DWORD fields. | Recompile the same image-specific source/config for each target. Preserve 32-bit guest pointers, wrapping, stack/TLS layout, callback discovery and stop diagnostics. Re-run numerical, atomic and ABI tests with the target compiler; do not assume Apple and Android host C types or calling conventions match. |
| Address space | Patched `kernel/xbox_memory_layout.c` reserves a sparse 4 GiB host span on Apple hosts, then maps retail RAM and aliases. `platform/win32_compat.c` backs Apple shared mappings with an unlinked file in `/tmp`; the non-Apple path uses `memfd_create`. | Make reservation and shared backing explicit platform services. Probe contiguous virtual-address availability, writable temporary storage, shared aliases, page alignment and safe release on physical devices. A 4 GiB reservation is not 4 GiB resident RAM, but must still be possible in the process. Do not assume the existing non-Apple address-placement path reproduces the tested macOS mapping. |
| Graphics | `src/graphics.c` and `src/shader_bridge.inc` directly use desktop GL. `src/nv2a_vertex.c` and `src/nv2a_pixel.c` translate original shader semantics to GLSL. Patched `d3d/d3d8_gl.c` owns SDL window/context and CGL worker handoff on macOS. | Separate Xbox resource/state/shader semantics from API calls before adding a second backend. Candidate future backends are Metal for Apple mobile and GLES or Vulkan for Android, subject to feature tests. CGL and desktop GLSL cannot simply be enabled by changing the CPU target. Validate formats, palette/swizzle conversion, dependent texture modes, depth, fences, fog and render targets against current fixtures and actual game frames. |
| Threading and timing | POSIX compatibility, `src/timing_bridge.c`, per-entry graphics locking and a monotonic presentation deadline coordinate original game workers. | Add host lifecycle hooks for pause, resume, surface loss and shutdown. Keep original worker drawing valid; retain serialized resource access and keep host window/event APIs on their required thread. Define how clocks, audio and guest workers pause together instead of accumulating catch-up frames after backgrounding. |
| Audio | `src/audio_bridge.c`, runtime `audio/` decoder/stream state and `apu/apu_xaudio2.c` SDL PCM output. | Keep guest packet completion, ADPCM decoding and mixing separate from device output. Exercise device interruptions, route changes, suspend/resume and negotiated output format on each target. Measure actual queue latency, underruns and story synchronization; passing the existing desktop sink tests is insufficient. |
| Input | `src/input_bridge.c` and runtime `input/xinput_device.c` use SDL game-controller mappings and focused keyboard/mouse snapshots. | Retain one guest controller state fed by target SDL mappings. Test Xbox One/Series and DualSense reconnect, axes, triggers, hotplug and focus/background clearing on physical hardware. Touch controls and TV remote/menu behavior need deliberate mappings; they are not implemented. Preserve keyboard/mouse support where available without global capture. |
| Assets and saves | `src/main.c` resolves an extracted asset directory, then creates sibling `saves` and `run` paths. The macOS app links back to the workspace assets. | Pass independent read-only asset, writable save and cache/log roots to the host entry point. Mobile bundles must not be treated as writable directories. Provide an import/extraction flow for the user's own supported disc, preserving paths/case and image verification. A workspace symlink is not a distributable asset package. |

The address-map probe is an early feasibility gate. Guest accesses currently rely
on a host-base offset and real shared aliases. If a target cannot support that
layout, an alternative translation strategy is a substantial runtime/lifter
change requiring separate evidence; silently truncating addresses, copying alias
memory or expanding guest RAM is not a valid workaround. Host page size and guest
page size must remain distinct.

## Staged work when a port is requested

1. **Establish a macOS reference.** Verify real Arctic Antics gameplay, original
   story watch/skip, save/load and clean shutdown. Capture repeatable scene/input
   traces, shader fixtures and frame/audio measurements. Continue resolving
   existing macOS failures before multiplying platform variables.
2. **Extract only proven host boundaries.** Introduce explicit asset/save/cache
   roots, lifecycle callbacks, memory services and target dependency discovery
   while retaining the current macOS behavior. A small capability report should
   eventually record OS/ABI, graphics backend, page size, successful alias-map
   probe, audio format and available inputs. Add fields when actually measured;
   do not advertise untested targets with placeholder capability flags.
3. **Prove one physical target at a time.** Select a named device, OS version and
   application ABI. First run a small native runtime probe for address mapping,
   TLS/threads, guest-width canaries and AOT dispatch; then audio/input and GPU
   fixtures. A Shield's ARM-capable processor alone does not establish that its
   installed OS accepts an ARM64 application. Verify the exact model's supported
   application ABIs before choosing an Android `arm64-v8a` build. A 32-bit-only
   environment cannot satisfy this ARM64 milestone.
4. **Add one renderer and app shell.** Implement the required backend against
   the same Xbox semantic tests. Add mobile/TV lifecycle, orientation/surface
   handling, controller navigation and target asset import/save directories.
   Preserve the original internal render resolution and aspect independently of
   output resolution and optional sharpening. Compile/sign/package with the
   target SDK; distribution requirements need a separate review at that time.
5. **Verify the game, then optimize.** Run original New Game, watch and skip the
   intro, enter Arctic Antics in real gameplay rather than attract mode, move,
   jump, spin, pause, save/load and resume after backgrounding. Record device/OS,
   build identity, frame-time percentiles, sustained thermal behavior, peak memory,
   audio latency and controller observations. Keep 60 FPS as a measured target;
   desktop results cannot predict mobile or Shield performance.

No target presets, application shells, new graphics backends or portability
runtime changes were added with this document. Review was source-only on
2026-09-08; no mobile SDK downloads, cross-builds or physical-device tests were
performed. The next useful implementation remains the current macOS milestone.
