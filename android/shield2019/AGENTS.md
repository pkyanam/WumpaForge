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

## Latest rendering continuation
The user reiterated parallel Shield60FPS work. Three bounded agents audited
context/draw submission, shader data, and presentation. Real GPU suite passes
through vertex-uniform cache source9166913; installed latest APK. Mac untouched.
Read PERFORMANCE-AUDIT.md and root docs/STATUS.md before further optimization.
Do not mistake the earlier worker-held guestCS observation for a proven lock leak:
33 traced outer worker releases reached depth0 with successfulpthread unlocks.
A subsequent story-skip/hub load crashed in original heapEF72D atEF7DB, reading
MEM16(ESI-8) withESI0; trace heapMEM32(9453B0), head+180, requestF1629.
Do not bypass originalEndStateBlock or force-unlock locks to hide that corruption.
Never keep native graphics mutex across arbitrary generated guest code without
proving guest-lock/wait ordering. Current context release overhead remains large.
Useful ignored reports: render-current.data/dump/report (indexed scan10.38%CPU),
bounds-current-report (different later scene; do not claim controlledcomparison),
uniform-values-graphics.log (fullphysicalGPU pass), hub-crash-logcat.txt.
Latest weeklycheck77%used (23%remaining): new work paused at buffer. Finish
source handoff only; preserve20%remaining. All implementation agents finished.

## Explicit resume authorization
The user explicitly resumed after the77%buffer pause and requests parallel
performance/allocation work plus primary-source Shield architecture research.
Continue bounded work beyond the old77%buffer; preserve20%remaining and check
before builds. The old clock deadline/pause is superseded. User reports hub
movement and level selection, followed by endless green loading; not levelplay.

## Latest allowance authorization
The user explicitly authorizes working past20% weekly remaining to get the game
working. The earlier20%floor and77%buffer are superseded. Remain token/resource
efficient; no arbitrary usage burn. Root owns device testing and two-job builds.
