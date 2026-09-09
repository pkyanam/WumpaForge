# Shield Pro 2019 development

User authorized a few hours of Android build work on September 9, starting
08:24 UTC, in this separate folder. Target only the 2019 Pro (`mdarcy`), ARM64.
Device access is unavailable until tomorrow; do not probe the network hostname.

- Check account weekly usage before each substantial work chunk. Started at
  65% used (35% remaining). Stop new work at 77% used and reserve the remaining
  buffer for documenting/committing; never intentionally consume below 20% left.
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
The cross-build and local verification pass is complete; device access is the
next meaningful gate. Do not start further background work or connect to SHIELD
until the user provides access and resumes testing. Read README for exact evidence.
