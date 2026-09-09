# SHIELD TV Pro 2019 preparation

Research and source-only groundwork, 2026-09-08. The selected future target is
**NVIDIA SHIELD TV Pro (2019)**. No other Shield model is in scope. WumpaForge
currently runs only on Apple Silicon macOS; the user completed Arctic Antics on
build61. No Android executable has been compiled or run, and no Android game,
APK, graphics backend or device performance result exists. Current macOS issues
remain in [STATUS.md](STATUS.md).

The next useful Android work is a small device feasibility check, followed by
reuse of the existing renderer if the actual driver permits it. A full Vulkan
rewrite is not the starting assumption. This plan preserves build-time x86-to-C
lifting and native ARM64 compilation, with explicit unsupported-code failures
and no CPU interpreter/JIT fallback.

## What the primary sources establish

| Question | Evidence | Consequence for this project |
| --- | --- | --- |
| Which product? | Google's supported-device table lists manufacturer NVIDIA, marketing name SHIELD TV Pro, device `mdarcy`, model `SHIELD Android TV`. | Use manufacturer/device identity plus the actual application ABI; the generic displayed model string alone is insufficient. [Google device catalog](https://storage.googleapis.com/play_public/supported_devices.html) |
| Does the Pro accept 64-bit applications? | NVIDIA explicitly documents that its 2019 Pro can run applications requiring a 64-bit Android environment. | `arm64-v8a` is the planned application ABI, subject to reading the specific device's supported ABI lists. This is not inferred from the CPU or `uname`. [NVIDIA support](https://nvidia.custhelp.com/app/answers/detail/a_id/4923), [Android ABI definitions](https://developer.android.com/ndk/guides/abis) |
| Is desktop OpenGL available? | Khronos records OpenGL 4.6 conformance for SHIELD TV Pro 2019 on Android 9 and Android 11; the latter entry is dated 2022-01-07, submission 305. | First test an EGL desktop-GL context and the required GL4.1/GLSL410 features on the actual firmware. A conformance listing does not validate our loader, SDL integration or rendering. [Khronos conformance registry](https://www.khronos.org/conformance/adopters/conformant-products/opengl) |
| Is Vulkan an alternative? | NVIDIA documents Vulkan 1.1 on Shield Android 8 / Experience 7 and later. Android requires applications to query device support. | Treat Vulkan as a later, larger backend option; do not assume modern Vulkan 1.3 features from current NDK headers. [NVIDIA Vulkan support](https://developer.nvidia.com/vulkan-android), [Android Vulkan engine guidance](https://developer.android.com/games/develop/vulkan/native-engine-support) |
| Which tools? | Google's downloads page currently lists NDK r30, revision `30.0.16248370`. Its macOS download is about 1.07 GB. | Pin an installed, reviewed NDK revision when device work starts. No SDK, NDK or Android Studio was downloaded for this preparation. [NDK downloads](https://developer.android.com/ndk/downloads) |

The source review does **not** establish the user's Pro's installed firmware,
page size, driver version, free address space, controller mapping or performance.
Those fields stay unknown. The checked-in preflight requires `mdarcy`; a custom
firmware with a different identity requires a deliberate review rather than an
automatic claim of compatibility. The future application should also read
`Build.SUPPORTED_64_BIT_ABIS`. [Android Build API](https://developer.android.com/reference/android/os/Build#SUPPORTED_64_BIT_ABIS)

## Build and runtime work that is actually required

Use a separate Android toolchain build directory. The NDK CMake toolchain takes
`ANDROID_ABI=arm64-v8a` and an explicit `ANDROID_PLATFORM`; this minimum runtime
API is distinct from the app's compile/target SDK. The isolated probe chooses API
30 because the existing non-Apple memory backend calls `memfd_create`, whose
bionic wrapper starts there. A Pro on an earlier OS would require a separately
reviewed compatibility path; this plan does not update its firmware.
[NDK CMake guide](https://developer.android.com/ndk/guides/cmake),
[bionic memory API](https://android.googlesource.com/platform/bionic/+/master/libc/include/sys/mman.h)

Root `CMakeLists.txt` currently links Homebrew paths and epoxy, builds a desktop
executable, and pulls the entire patched Xbox runtime. Those are not Android
dependencies. Keep host extraction/analyze/lift tools separate, then compile the
same generated title C and required runtime libraries with the NDK. Preserve
guest DWORD layouts, callback seeds, TLS/stack ownership and arithmetic behavior.
The first runtime compile must audit all platform symbols: current
`platform/win32_compat.c` calls `pthread_cancel`, which bionic intentionally lacks.
A faithful cancellation/lifetime contract must replace that reached dependency;
neither success stubs nor compiling with an undeclared-function warning fix it.
[bionic implementation status](https://android.googlesource.com/platform/bionic/+/master/docs/status.md)

SDL2's Android integration uses an Activity/JNI layer and a native shared game
library; the desktop `main` executable is not an APK. Later use the matching SDL
Android sources and Activity glue from a pinned revision, with a small subclass
for application paths and lifecycle. Guest pthread workers that invoke SDL/JNI
need correct attachment and cleanup. Pause/resume must coordinate those workers,
audio and graphics; blocking only the SDL event loop can leave original loading
or audio threads running against a lost surface. Preserve the current per-entry
graphics lock and original loading-worker draws.
[SDL2 Android integration](https://github.com/libsdl-org/SDL/blob/SDL2/docs/README-android.md)

## Memory is an early feasibility gate

The current Apple path owns a sparse 4 GiB virtual span, then maps 64 MiB of retail
RAM and real shared aliases at guest offsets. Android presently selects a
different, older address-placement branch in `kernel/xbox_memory_layout.c`.
General AArch64 Linux virtual-address layouts are much larger than 4 GiB; this
makes a reservation plausible, **not proven** in an Android app with its actual
ASLR, mappings and resource limits. [Linux AArch64 memory layout](https://www.kernel.org/doc/html/latest/arch/arm64/memory.html)

The isolated [memory probe sources](../android/probe/) reserve the span with
`PROT_NONE`, map a sparse 64 MiB shared backing at offsets zero and `0x80000000`,
touch a few boundary bytes in both directions, replace/recommit the mirror and
release the owned span. All fixed mappings remain inside that reservation. It
does not allocate 4 GiB of physical RAM or request executable guest memory. It
does not yet test the complete Xbox apertures, TLS, graphics coherence or game.
Even a successful shell probe must be repeated inside the application sandbox.

Use the runtime host page size for mapping offsets, lengths and protection.
Guest 4 KiB conventions must not be mechanically changed to 16 KiB. Android's
guidance separately requires suitable ELF/package alignment and removing host
4 KiB assumptions; NDK r28+ emits 16 KiB-aligned libraries by default. Check
dependencies too. The probe accepts measured 4096/16384-byte host pages and
rejects other values for review, but neither case has been tested on a device.
Audit existing `0x1000` protection requests and sub-page device regions before
promising 16 KiB compatibility. [Android page-size guide](https://developer.android.com/guide/practices/page-sizes)

## Graphics decision and smallest useful tests

**First choice to investigate: desktop GL over EGL on this Pro.** NVIDIA has
documented reusing desktop GL code on Shield through EGL. That supports a bounded
probe, not an assumption that SDL's default Android context creation or epoxy
will expose the required API unchanged. Query `EGL_CLIENT_APIS`, request
`EGL_OPENGL_API` and an appropriate desktop core context, then record vendor,
renderer, GL/GLSL versions, extensions and required entry points. Prove the path
inside an ordinary app, without relying on root or private vendor-library paths.
[NVIDIA Shield porting presentation](https://developer.download.nvidia.com/assets/events/GDC15/SHIELD/Bringing_BorderLands2_to_SHIELD_GDC2015.pdf)

The current shader generators emit `#version 410 core`; the backend also uses
desktop GL state and packed BGRA attributes/uploads. Its macOS context handoff
uses CGL, and presentation depends on correct default-FBO binding through swap.
Replace the host context/surface boundary, preserving Xbox semantics. Start with
the existing synthetic shader, retained-vertex, shared depth/stencil, render
target and final-presentation fixtures. Include surface destruction/recreation
and serialized worker handoff before trying original scene rendering.

If a usable desktop context is unavailable, explicitly choose a second project:
GLES adaptation or Vulkan. GLES needs shader-language and API changes, including
desktop polygon mode, border sampling, BGRA vertex handling and diagnostic
texture readback; replacing only the version string is insufficient. Vulkan adds
pipeline/descriptor construction, resource transitions, synchronization and
SPIR-V generation. Android's standard NDK graphics interfaces are EGL/GLES and
Vulkan, and GPU versions must be queried; desktop GL support on this Pro is not a
general Android guarantee. [Android native graphics APIs](https://developer.android.com/ndk/guides/stable_apis)

## TV application, controls, audio and assets

The eventual manifest needs an exported launcher Activity with `MAIN` and
`LEANBACK_LAUNCHER`, an app banner, and touchscreen declared optional. Mark the
app as a game; declare gamepad capability with `required="false"`, then explain
controller requirements in the UI. No manifest was added now because the real
Activity/native library, resources and graphics requirement are not chosen yet.
[Create a TV app](https://developer.android.com/training/tv/get-started/create),
[TV game declarations](https://developer.android.com/training/tv/games)

Retain SDL game-controller mappings for Xbox One/Series and DualSense. Test
buttons, trigger ranges, reconnect and background clearing on the Pro itself;
macOS virtual-device tests do not establish Bluetooth behavior there. Remote
D-pad/select/Back must navigate setup and permit exit; if full gameplay needs a
gamepad, say so before entering it. Home/Back and controller disconnection should
pause coherently, without trapping the user in the app. Keep focused keyboard and
mouse support where attached. [TV controller behavior](https://developer.android.com/training/tv/get-started/controllers)

Keep the existing ADPCM decoder, guest packet lifecycle and mixer, with a target
SDL output sink. Confirm actual negotiated rate, queue timing and audio-device
interruptions on HDMI/Bluetooth output. Measure source audio against original
scene time after pause/resume; do not hide drift with arbitrary playback-rate
changes. Surface refresh and TV processing latency need measurement independently
of the 60 FPS game target. Preserve 640x480 internal rendering and 4:3 aspect while
evaluating larger output and sharpening; the Pro's advertised video upscaling is
not evidence that it can upscale this game's render targets for free.

`src/main.c` currently creates save/run directories adjacent to extracted assets.
Separate those roots before an app port. Import the user's supported ISO or
extracted files into a verified readable asset tree, retaining image/path checks;
keep saves in persistent app storage and logs/cache separately. A document
provider URI is not necessarily a POSIX path, so copy/import or supply an actual
I/O adapter before original `fopen`-style reads. Do not place the supplied ISO or
game assets in an APK/repository. Account for storage capacity, import failure and
save export independently. [Android document access](https://developer.android.com/training/data-storage/shared/documents-files),
[app storage](https://developer.android.com/training/data-storage/app-specific)

## Ordered milestones and evidence

1. **Preparation now:** the offline preflight, isolated probe source and target
   guards below; source/synthetic checks only. No SDK downloads or live game tests.
2. **When the Pro is available:** verify `mdarcy`, OS/application ABI and page
   size; pin an NDK; cross-build the memory diagnostic and inspect its ELF machine,
   interpreter and segment alignment. Run it only after review, then repeat inside
   an app sandbox. Stop on a failed reservation or incorrect alias behavior.
3. **Minimal host integration:** fix evidenced bionic/thread/path boundaries;
   build a real SDL Activity and desktop-EGL capability fixture, retaining all
   failure diagnostics. Prove actual pixels and lifecycle/audio/input fixtures.
4. **Title integration later:** package native AOT game code without user assets;
   validate imported data, original story watch/skip, real Arctic Antics gameplay,
   completion/return, save/load and shutdown. This is the first Android game gate.
5. **Performance later:** record exact Pro firmware/build, frame-time distribution,
   sustained temperature behavior, peak RSS, audio queue/sync and input latency at
   original internal resolution. Compare output/filter options only after correct
   rendering. There is no supported Shield FPS estimate from current macOS data.

See [android/README.md](../android/README.md) for scaffold commands and exact
validation limits. Broader iOS/iPadOS/general Android architecture remains in
[PORTABILITY.md](PORTABILITY.md), outside this Pro-only preparation.
