# WumpaForge on SHIELD TV Pro 2019

This isolated Android work targets **NVIDIA SHIELD TV Pro 2019 (`mdarcy`)** only.
The full original AOT game code and native runtime **cross-compile and link as
Android ARM64**. The APK is installed on a real Shield and renders the original
opening screen. Memory, graphics components and PCM callback checks pass.
Initial performance is severely slow with audio/video desynchronization, so this
is **not yet a verified playable port**.
The Mac app and its original dependency checkout are preserved.

## Build on the current Mac

First complete the normal repository setup with your own supported USA Xbox ISO.
That supplies the verified local assets, patched runtime and generated game C.
Then run from the repository root:

```sh
python3 android/shield2019/build.py
```

The script uses the already installed Android SDK, NDK **27.1.12297006 (r27b)**,
build-tools **36.0.0**, Android **36** compile SDK and Homebrew OpenJDK. Override
SDK/NDK/JDK locations with `--sdk`, `--ndk` and `--java-home`. It downloads only a
pinned SDL **2.32.10** archive and Khronos GL declarations, verifies SHA-256,
prepares isolated sources, builds with two jobs and packages/signs the APK.
It neither installs software on the Shield nor discovers/connects to it.

Output: `build/shield2019/WumpaForge-Shield-Pro-2019-dev.apk`.
All binaries, copied runtime, dependencies and the local development signing key
remain in ignored `build/shield2019`. The APK includes compiled game logic but
**no ISO or extracted game assets**. It is for private local testing, not release.

## Application behavior

The TV launcher has remote-focusable actions for memory/desktop-EGL diagnostics,
full graphics regressions, live controller input/rumble, stereo audio, controller
enumeration and starting the original game. The device gate requires
NVIDIA `mdarcy`, ARM64 app support and Android API 30+. The native game runs in a
separate process through SDL's Android Activity/JNI integration.

The renderer preserves the existing desktop GL backend through EGL and a typed
loader for the actually used GL functions. It requests a **desktop GL 4.1 core
context**; no GLES version-string substitution, CPU interpreter or JIT is used.
The public EGL driver must expose those desktop functions on the actual Shield.
The launcher probe can report this gate and a pixel round trip without assets.
The separate graphics Activity runs the same synthetic guest-ABI/rendering suite
against the Android backend, with assertions enabled. It passed on the actual
Tegra driver after allowing one RGB quantization step in a float-color fixture.

Game assets use app-specific external storage; saves/run state use persistent
internal app storage. `native.log` is written beside the assets folder. Android
filesystem paths are used directly; document-provider URIs are not supported.

Bluetooth controllers are paired through Shield settings. SDL supplies mappings
for Android-recognized controllers of any brand, including Xbox and DualSense.
A gamepad is promoted over a TV remote on player one using capabilities, not a
brand whitelist. Existing player-one gamepads remain stable. This passed a host
SDL virtual-device regression, including reconnect, stale input and rumble state.
An optional `gamecontrollerdb.txt` in app external files supplies custom SDL
mappings. Android `VIBRATE` permission is declared; paired-device Bluetooth access
is declared through API 30 for SDL's optional BLE path.

A name appearing in Android's device list is not proof that every button, axis,
rumble or reconnect behavior is correct. Unmapped controllers need a mapping.

## Evidence so far and next gates

- The API guard was corrected for NDK's legacy toolchain, which sets
  `CMAKE_SYSTEM_VERSION=1`; its effective API is `ANDROID_PLATFORM_LEVEL`.
  Ten offline preflight/configuration tests pass.
- Memory diagnostic: actual AArch64 Android executable cross-build passes.
- Full native game library: link passes with undefined symbols rejected.
- TV launcher Java/DEX/resources and APK signature/alignment checks pass.
- Real mdarcy/API30 device: sparse memory aliases and OpenGL4.1/GLSL4.10
  NVIDIA495.00 pixel probe pass; full graphics fixture returns0.
- PCM callback consumes all96000 submitted frames with nonzero samples; this
  does not establish speaker routing, game audio sync or glitch-free playback.
- All2267 original asset files transferred and remotely SHA-256 verified.
- Original game opening renders. Baseline profile measures16.18FPS in one
  opening segment, bind14.87ms + detach35.38ms per frame; user confirms severe
  loading slowness and audio/video desync. Other segments are slower.
- Lazy EGL binding and ordered scalar GL batching, cached profile flags, API
  binding guard and uniform locations improve that same segment to51.08–51.35FPS.
  Other intro segments measure roughly20–40FPS; this remains below the target.
  Later indexed-resource/texture changes passed the full GPU suite; measured
  opening windows51.22/52.18FPS and heavier28.54/21.64FPS remain below60.
  Actual output is1920x1080 from640x480 internal rendering.
- Opt-in on-disk shader binaries pass the real GPU suite cold and warm; the warm
  run loaded23 cached programs, saved0 new ones, and returned0. The host fixture
  also rejects corrupt, mismatched-driver and link-rejected entries and bounds
  disk use to256 files/64MiB. This is not a measured whole-game speedup yet.
- ADB remote Start press/release reached guest input, and Center selected New
  Game/name confirmation. Physical controller/remote chord testing remains open.
- An AFK Arctic Antics load stalled with the main game thread waiting for the
  loading worker's guest lock0x4EA440. The worker holds it during presentation;
  a bounded opt-in trace is testing lock balance versus driver delay.
- Physical Bluetooth, story/gameplay completion, saves, lifecycle and sustained
  performance remain unverified.

The source pass also found 43 case mismatches among 108 matched original asset
filename literals. The Shield-only path adapter resolves existing on-disk
spelling, including saves, without renaming assets; its host UBSan fixture passes.
Input event pumping shares the graphics lock through SDL's blocking pause. A
reported EGL context reset ends the game process with a diagnostic because full
resource restoration is not implemented. Normal pause/resume remains unverified.

## Device session

Enable debugging and authorize this Mac on the Shield, then select its **exact**
ADB serial. These commands do not auto-discover or connect to a network address:

```sh
python3 android/shield2019/device.py inspect --serial YOUR_ADB_SERIAL
python3 android/shield2019/device.py install --serial YOUR_ADB_SERIAL
```

Open WumpaForge on the TV once to create its app storage. Run memory/GL, graphics,
controller and audio checks before the game. Then import the verified local
assets (roughly 972 MB for this disc):

```sh
python3 android/shield2019/device.py import-assets --dry-run
python3 android/shield2019/device.py import-assets --serial YOUR_ADB_SERIAL
```

Import checks every local asset against the prior disc-verification report,
stages files separately, verifies SHA-256 on the device and only then activates
the directory. It refuses an existing asset destination and preserves saves.
Failed staging remains for inspection; a concurrent-import lock is released when
the command exits normally or with a handled error. The launcher verifies the
supported executable's SHA-256 again before launching the game.

Try the original title/story, Arctic Antics, save/load and return to the hub.
Use Back to leave a diagnostic; collect output afterward:

```sh
python3 android/shield2019/device.py logs --serial YOUR_ADB_SERIAL
```

Logs are pulled into ignored `local/reports/shield2019`. Do not infer a pass from
a disappearing graphics-check window: inspect its final PASS line or failure.
The audio test checks actual callback consumption and plays quiet left/right
tones, but only listening can establish output routing and audibility.

## Local checks

```sh
python3 -B -m unittest discover -s android -p 'test_*.py'
python3 -B -m unittest discover -s android/shield2019 -p 'test_*.py'
python3 android/shield2019/tests/test_input_tv.py
python3 android/shield2019/tests/test_path_case.py
python3 android/shield2019/artifact_check.py build/shield2019/WumpaForge-Shield-Pro-2019-dev.apk
```

The ELF check enforces AArch64, 16 KiB LOAD alignment, no writable executable
segments or executable stack, and an allowlist of APK payload classes. Packaging
also verifies the APK signature and ZIP alignment. None of these executes Android
code. The small input/path tests execute host-native synthetic fixtures.

See [runtime adaptations](RUNTIME-NOTES.md), [folder instructions](AGENTS.md),
[the original Android plan](../../docs/ANDROID-TV-PLAN.md) and the repository's
[licensing inventory](../../docs/LICENSING.md). SDL's zlib notice and Khronos's MIT
header notice remain in the pinned downloaded sources; they are not relabeled.

## Earlier September 9 cross-build handoff

The integrated private development APK includes all six ARM64 libraries (game,
SDL, memory/EGL probe, graphics checks, controller checks and audio checks).
Twenty-one Python checks and the two host-native UBSan input/path fixtures pass.
A second preparation produced 133 byte-identical source files. The APK passes
ELF inventory/alignment checks, signature verification and package metadata checks.
The previously playable Mac build 68 binary has the same SHA-256 as before.

That pre-device checkpoint is superseded by the device evidence above. The Mac
build remains preserved; current Android work is isolated in this folder.

### Playing with the Shield remote

The game activity now maps a limited TV remote to keyboard controls before SDL
can treat its buttons as a partial gamepad. Analog-capable gamepads and devices
with A, B and Start retain SDL controller mappings. Home, volume and voice retain
Android behavior. In the game, Back is a game action; use Home to leave.

| Remote button | Movement layer (default) | Camera layer | Extra buttons layer |
| --- | --- | --- | --- |
| D-pad | Move left stick + menu D-pad | Right stick / camera | Up: White; Down: Black; Left: LT; Right: RT |
| Center / Select | A / jump / confirm | A | Xbox Back |
| Back | B / spin / cancel | B | Left stick click |
| Play/pause | Start / pause | Start | Right stick click |
| Rewind | X | X | X |
| Fast-forward | Y | Y | Y |
| Menu | Cycle to Camera | Cycle to Extra buttons | Cycle to Movement |

Menu displays a brief layer toast and releases held controls. Android repeats do
not retrigger buttons; release, focus loss, activity pause and device removal
clear held input. If the remote's configurable Menu button is assigned a system
shortcut, configure it to send Menu before using the extra layers. The available
buttons cannot replace two simultaneous analog sticks; a gamepad is preferable
for normal play. This mapping covers the Xbox button set for debugging, but
physical remote delivery and simultaneous-button support still need device tests.
The host Java fixture checks repeats, layer transitions, overlapping logical
buttons, releases and preservation of system keys; it does not prove Bluetooth
behavior or Android firmware key delivery.

### September 9 performance investigation

The driver does not expose `EGL_KHR_context_flush_control`; requesting release
NONE therefore falls back to normal behavior and is not a speed fix here.
Deferring context binding until a native GL operation, while retaining the device
mutex, passed the full device graphics suite. In comparable opening windows it
improved16–17FPS to34–35FPS. Heavier later windows still measured13–25FPS;
this is not60FPS or an audio synchronization fix.

Ignored device reports under `local/reports/shield2019` retain the baseline and
lazy-binding traces, actual screenshots and complete graphics/PCM diagnostics.
App-external marker files `profile-context`, `lazy-bind`, `no-release-flush` and
`defer-state` select diagnostic/experimental paths on the next process launch.
The profiling marker also enables bounded guest-input traces. Markers are
local development controls; do not treat them as a finished settings UI.

### Optional development diagnostics

Empty marker files in app external `files/` are read only at process startup:
`lazy-bind`, `defer-state`, `profile`, `profile-context`, `shader-cache`, and
`trace-loading-cs`. The context/state optimizations currently require their
markers for controlled comparisons; `defer-state` implies lazy binding.
`profile` enables lightweight frame counters; `profile-context` adds more costly
context timings. `shader-cache` uses internal private `files/shader-cache/` and
falls back to normal compilation on any rejection. `trace-loading-cs` starts at
worker2BF30's first lock acquisition and stops after3000 records; it does not
change locking. Remove its marker and restart for clean performance measurements.
The NVIDIA driver does not advertise the optional release-flush control extension;
`no-release-flush` therefore produced no applicable optimization on this device.

## Resumed Shield performance work

The user reports hub movement and level selection, followed by endless green
portal loading. This is progress beyond menus, but not a successful level load.
See [architecture research](SHIELD-ARCHITECTURE-RESEARCH.md) for primary sources
and the driver-threading experiment, which showed no useful gain and was removed.
New bounded work reuses up to1MiB of indexed-draw scratch memory, skips adjacent
identical queued state setters, and limits routine successful read diagnostics.
Read failures and short reads remain visible. Direct DXT1/3/5 upload and bounded Morton conversion pass physical GPU validation.
The first loading window upload CPU fell39.297→21.109ms/frame, with the same
131.753MiB uploaded; longest pause10.41→8.87seconds in single-run comparisons.
Heavy scenes remain13–28FPS. Do not infer successful level loading from these tests.
An external `trace-loading-cs-outer` marker captures only outer loading-worker lock
pairs across later loads; remove it for uninstrumented measurements.
