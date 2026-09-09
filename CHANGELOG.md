# Development changelog

This is a private experimental source project, not a published game release.
Detailed evidence and commit references are in [STATUS](docs/STATUS.md) and Git
history. Build numbers identify local development checkpoints.

## 2026-09-09 — Isolated Shield cross-build

- Cross-compiled the actual AOT game library and runtime for Android ARM64,
  packaged a signed private TV development APK, and preserved Mac build 68.
- Added asset-free memory/EGL, graphics, controller and stereo-audio diagnostics.
- Fixed Android toolchain API detection, owned guest-memory reservation,
  TV remote/gamepad priority and case-insensitive Xbox asset/save path lookup.
- Added serialized input event pumping and an explicit stop on EGL context loss
  until graphics resource restoration exists.
- Added verified local/device asset-import tooling, ELF/package checks and
  [tomorrow's device instructions](android/shield2019/README.md).
- Device access is unavailable; Android launch, gameplay and performance remain
  unverified. Development stopped early at the user's request once further
  meaningful validation needed the Shield.

## 2026-09-08 — Native playability and private staging

The user completed Arctic Antics with keyboard controls on native ARM64 build 61.
Build68 incorporates subsequent source-verified corrections and is packaged for
the next play session; those later changes have not received a new gameplay pass.

### Game and presentation

- Ahead-of-time Xbox x86 to ARM64 game code, with native system, graphics, audio
  and input compatibility. No CPU interpreter or JIT fallback.
- Original title, menus and story playback; the story remains available to watch
  or skip. Intermittent story/demo visual issues remain under investigation.
- Keyboard and mouse controls, plus SDL mappings for Xbox and DualSense pads.
  Physical Bluetooth testing is still outstanding.
- Resizable/fullscreen output and a native Display menu for 720p/1080p/1440p,
  with Off/Light/Medium/Strong sharpening. Internal rendering stays 640×480 at 4:3.
- Generated fruit/crate app icon, embedded in the local macOS bundle.

### Compatibility corrections

- Width/count-correct shifts and carry rotations, guest floating-point rounding
  and classification, and original CRT64 shift/division/remainder paths.
- Actual SAHF flag snapshots coupled with x87 remainder completion/status;
  mixed comparison branches, including Aku Aku follower-angle selection.
- Guest EBP initialization and call/return/tail publication, with focused
  frame, stack and SEH exchange checks.
- Retained vertex-array bounds, shared texture depth/stencil and surface pixels,
  synchronized read-only texture-target locks, and vertex NaN color handling.
- Native distance/pan audio state and atomic deferred updates; bounded resource
  lookup caching; atomic aggregate waits; correct 116-byte guest SHA context.

These are tested contracts, not a claim that every reported glitch is resolved.

### Build and contributor workflow

- WumpaForge private GitHub staging, with a BYO supported USA Xbox ISO setup
  command. Original assets, generated game C and binaries stay outside Git.
- Reproducible pinned dependency patches, a sequential CPU test command, and
  explicit CMake GPU/filter targets requiring no game assets or generated C.
- ARM64 bundle verification, atomic executable replacement and minimum macOS
  metadata taken from the compiled binary. The app still uses local assets and
  Homebrew libraries; it is not a standalone distribution package.
- Documentation and isolated diagnostic groundwork for **SHIELD TV Pro 2019**.
  No Android game build, APK or device execution is provided.

### Remaining validation

Whole-game correctness, sustained 60 FPS, full post-level hub return, save/load,
physical Bluetooth controllers, and intermittent story/audio/visual failures
remain outside the demonstrated coverage. Some unsupported instruction and
compatibility paths still stop explicitly. macOS on the development M3 is the
only tested platform. See [testing](docs/TESTING.md), [licensing](docs/LICENSING.md)
and the [Pro 2019 plan](docs/ANDROID-TV-PLAN.md) before extending or distributing.
