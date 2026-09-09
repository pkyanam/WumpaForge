# Shield Pro 2019 development

User authorized a few hours of Android build work on September 9, starting
08:24 UTC, in this separate folder. Target only the 2019 Pro (`mdarcy`), ARM64.
The user authorized network ADB to 192.168.1.46 on September 9; the device is
verified NVIDIA mdarcy, Android 11/API30, ARM64. Use the explicit ADB serial.

- Check account weekly usage before each substantial work chunk. Started at
  65% used (35% remaining). The latest user instruction is to keep working efficiently and preserve20%
  weekly remaining. Stop new work at77% used to reserve a documentation buffer;
  do not intentionally cross80% used.
  Account usage is shared, so recheck during builds and before final actions.
- Aim to wrap by 11:24 UTC or the usage threshold, whichever comes first.
- Source/configuration lives here. Downloads, copied runtime, generated loaders,
  object files, APKs and local signing keys go under ignored build/shield2019.
  Diagnostics go under ignored local/reports/shield2019.
- Preserve the macOS source/runtime and build 68. Apply Android-only patches to
  an isolated copy of the already patched runtime, never its macOS checkout.
- At most two compiler jobs total. Reuse the installed SDK/NDK and pinned SDL2.
- Native AOT ARM64 game code only; no CPU interpreter/JIT. No game assets in APK
  or Git. A cross-compiled library/APK is not proof of a working Shield launch.
- Use SDL mappings for Android-recognized gamepads of any brand. Reconnection,
  stale input clearing and actual Bluetooth behavior need on-device validation.
- Keep precise progress and remaining device gates in README.md and Git history.

## Handoff
The user explicitly allowed stopping early rather than filling the time budget.
Device testing has resumed with explicit authorization. Memory, desktop GL, full
graphics components and PCM callback checks pass. Actual game opening renders,
but initial performance was only about16FPS with severe audio/video desync.
Do not describe it as playable. Root is measuring Android context overhead and
adding a remote control scheme. Read README for the current evidence.
