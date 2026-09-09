# WumpaForge on SHIELD TV Pro 2019

This isolated Android work targets **NVIDIA SHIELD TV Pro 2019 (`mdarcy`)** only.
The full original AOT game code and native runtime **cross-compile and link as
Android ARM64**, and a signed development TV APK has been produced. No Shield
is available for execution yet. This is **not a verified working/playable port**.
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
controller enumeration and starting the original game. The device gate requires
NVIDIA `mdarcy`, ARM64 app support and Android API 30+. The native game runs in a
separate process through SDL's Android Activity/JNI integration.

The renderer preserves the existing desktop GL backend through EGL and a typed
loader for the actually used GL functions. It requests a **desktop GL 4.1 core
context**; no GLES version-string substitution, CPU interpreter or JIT is used.
The public EGL driver must expose those desktop functions on the actual Shield.
The launcher probe can report this gate and a pixel round trip without assets.

Game assets use app-specific external storage; saves/run state use persistent
internal app storage. `native.log` is written beside the assets folder. Android
filesystem paths are used directly; document-provider URIs are not supported.

Bluetooth controllers are paired through Shield settings. SDL supplies mappings
for Android-recognized controllers of any brand, including Xbox and DualSense.
A name appearing in Android's device list is not proof that every button, axis,
rumble or reconnect behavior is correct. Unmapped controllers need a mapping.

## Evidence so far and next gates

- The API guard was corrected for NDK's legacy toolchain, which sets
  `CMAKE_SYSTEM_VERSION=1`; its effective API is `ANDROID_PLATFORM_LEVEL`.
  Ten offline preflight/configuration tests pass.
- Memory diagnostic: actual AArch64 Android executable cross-build passes.
- Full native game library: link passes with undefined symbols rejected.
- TV launcher Java/DEX/resources and APK signature/alignment checks pass.
- Device memory/GL pixels, surface lifecycle, audio, physical Bluetooth,
  original title/story/gameplay, saves and performance remain **untested**.

Tomorrow: capture exact device metadata, install the private APK, run diagnostics,
verify/import local game assets, then attempt the original title and Arctic
Antics. Stop on failed prerequisites and retain the exact logs. Background/surface
loss and worker timing require explicit testing before claiming lifecycle support.

See [runtime adaptations](RUNTIME-NOTES.md), [folder instructions](AGENTS.md),
[the original Android plan](../../docs/ANDROID-TV-PLAN.md) and the repository's
[licensing inventory](../../docs/LICENSING.md). SDL's zlib notice and Khronos's MIT
header notice remain in the pinned downloaded sources; they are not relabeled.
