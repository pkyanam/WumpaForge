# Shield Pro 2019 contributor guide

This directory preserves an experimental native Android build for NVIDIA SHIELD
TV Pro 2019 (`mdarcy`), ARM64. It is outside the Mac-focused public setup promise.
Read README.md, PERFORMANCE-AUDIT.md, RUNTIME-NOTES.md and the latest root
`docs/STATUS.md` before changing it. Root AGENTS.md also applies.

## Isolation and build limits

- Keep Android-only source/configuration here. Apply Android patches to disposable
  build-tree copies of the patched runtime; do not modify the Mac dependency
  checkout or change working Mac behavior as an incidental Android optimization.
- Downloads, copied runtime, generated GL loaders, objects, APKs and local signing
  keys belong under ignored build/shield2019. Diagnostic outputs belong under
  ignored local/reports/shield2019.
- Run at most two compiler jobs total across all contributors/agents. Reuse existing
  SDK/NDK installations and the documented pinned SDL dependency where possible.
- Keep native AOT ARM64 game execution; no CPU interpreter/JIT fallback.
- Never include original game assets in Git or an APK. A compiled APK or native
  library is not evidence of a playable or performant Shield build.

## Device and input

Use an explicitly supplied ADB serial; do not discover or connect to a private
network device from historical notes. Coordinate display testing with its owner.
Offline work can continue while the TV is occupied. Record exact device model,
Android version, graphics capabilities and observed behavior with test results.

Use SDL mappings for Android-recognized Bluetooth controllers. Preserve remote
control mappings, focus handling and stale-input clearing. Physical pairing,
reconnection, rumble and control usability require actual device validation.

## Correctness before performance claims

- Preserve guest-lock/native-graphics-lock ordering. Never force-unlock locks or
  skip guest work to hide a loading failure.
- Kernel service dispatch selection is thread-local. Verify
  `g_kernel_dispatch_slot` is a TLS symbol in the built native library. The first
  historical fix silently skipped inside an ignored build directory: prepare.py
  now fences Git discovery and reverse-checks patch applications.
- Keep experimental GL RPC, indexed shader and AOT optimization switches explicit.
  Compare equivalent scene windows, count actual presented frames, and separate
  CPU, GPU, synchronization and loading costs.
- Use the real context version/extensions before enabling graphics states. The
  tested driver exposed OpenGL 4.1; do not assume GL 4.3 states are available.
- Borrowed vertex/index memory must remain valid until rendering completes. Keep
  serialization, resource lifetime and thread-local profiling behavior intact
  when moving work onto a persistent GL owner thread.
- Root owns complete builds and device tests during parallel development; agents
  should use bounded disjoint source audits or host fixtures.

Arctic Antics reached gameplay on the Shield during development, but stable
60 FPS and a completed level were not demonstrated. Later experimental paths may
have only offline or component evidence. Preserve these distinctions and consult
current status rather than promoting an old checkpoint into a release claim.
