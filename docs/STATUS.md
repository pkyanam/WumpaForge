# Status — 2026-09-08, evening

## Current instructions

Continue until midnight America/New_York, wrapping by **2026-09-09 00:00 EDT**
(04:00 UTC). Reserve the last15 minutes for validation, packaging, private push
and handoff. The latest user asks for **source audits and synthetic CPU/GPU
checks instead of further live-game testing for now**. Do not promise99.5%
whole-game confidence. Keep exact evidence and unknowns separate.

Root owns full builds, packaging and all game/UI actions. At most two compiler
jobs total across agents. No CPU interpreter/JIT: game code stays ahead-of-time
compiled ARM64. ISO remains read-only. Original assets/generated game C/binaries
remain ignored. Generated project branding is intentionally tracked.

## Running and built artifacts

- User completed **Arctic Antics**, Level7/Demo0, using keyboard on build61.
  The post-level hub return later hit a retained-array bounds guard.
- No game process is running as of22:58 EDT. Build64's `play-25.log` ended
  with `window close requested` at21:57 EDT; this is not evidence of a new crash.
  Check the process before any Computer Use call: querying a closed app can
  relaunch it. Preserve the current source-only validation preference.
- Combined native **build67**, source7b39d0a, compiled successfully and is now
  packaged at `build/Wrath Native.app`. ARM64 code, icon/Info.plist and asset link
  are verified in `local/reports/build67-manifest.json`. No game launch was made.
  Build64 is preserved under `build/checkpoints/build64/`; staged build66 remains
  available under `build/staged/`. The attract-demo artifact is not declared fixed.
- `build/branding/WumpaForge.icns` is generated and valid. Icon source/provenance:
  [BRANDING](BRANDING.md). The normal bundle now includes it and the Display menu.
  Its Dock appearance awaits the user's next launch.

## Latest fixes and evidence

| Work | Status and evidence |
| --- | --- |
| Retained vertex DMA bounds | f0c6a6c follows64MiB physical aperture with owner/generation checks. ade383e GPU test uses an actual indexed NULL-stream draw and changes adjacent bytes to change pixels. Full post-level return still unverified. [Diagnosis](TANGENT-STREAM-DIAGNOSIS.md) |
| Shared offscreen depth/stencil |656f4c4 fixes five captured256x256 texture views sharing640x480 D24S8. Top-left region, stencil, real occlusion, NULL/rebind, alias and lifetime/rollback GPU tests pass. Included build64. [Contract](TEXTURE-DEPTH-TARGETS.md) |
| Vertex NaN colors |a3aedbf fixes16 reproduced diffuse/specular component mismatches against pinned xemu. Actual56-byte bone input and16 matrix tests pass; no ARL epsilon or RSQ change justified. [Audit](research/VERTEX-NUMERIC-AUDIT.md) |
| Pixel arithmetic |9a1fd88:3072 synthetic comparisons across96 multistage shaders match independent scalar equations with max0/255 output difference. No pixel-generator change needed. [Audit](research/PIXEL-NUMERIC-AUDIT.md) |
| Display controls |632baaf native Display menu,720p/1080p/1440p output and Off/Light/Medium/Strong sharpening. Synthetic menu actions and displayed pixels pass;1440p produced2560x1440 drawable from1280x720 Retina points. [Presentation](WINDOW-PRESENTATION.md) |
| Native spatial audio |de7d92a real min/max-distance attenuation, listener/source positioning and atomic deferred updates; UBSan spatial/stream/252-buffer regressions pass. Stereo positioning is a documented approximation; HRTF/reverb remain unsupported. [Audio](AUDIO-SPATIAL.md) |
| x87 correctness |992e5ba fixes guest FRNDINT and unordered FTST; fixture went61→0 mismatches. dddf1a9 fixes FXAM/TLS occupancy and SAR/ROL/ROR widths, with exhaustive byte/count and original CRT sequence tests. FNSAVE/FRSTOR remain explicit unsupported boundaries. [Audit](research/AOT-CORRECTNESS-AUDIT.md) |
| Active surface aliases |cce7d26 fixes stale GPU reads/lost writes through identical shared texture views. Before/after synthetic CopyRects and full GL suite pass. [Audit](SURFACE-ALIAS-COHERENCE.md) |
| Atomic aggregate waits |dddf1a9/7eec696 preserve event/semaphore/mutex state until every object is ready. UBSan fixture went4→0 failed checks. This XBE does not import the affected multi-object wait APIs; no current stall is attributed to it. [Audit](research/WAIT-ALL-AUDIT.md) |
| Logical/double shifts |1d2a8c5 masks actual operand widths/counts and removes reproduced C shift UB. Actual original CRT64 shift bodies pass edge values and every byte count under UBSan. [Audit](research/AOT-CORRECTNESS-AUDIT.md) |
| Carry rotations and division |0367fab implements RCL/RCR carry rings and wrapping NEG. Four actual original CRT64 division/remainder helpers pass40000 random pairs plus edge cases under UBSan. Architectural division exceptions remain unchanged. [Audit](research/AOT-CORRECTNESS-AUDIT.md) |
| Mixed flag branches |7b39d0a fixes four original CFG joins, including Aku Aku follower-angle selection.1236 emitted native checks pass; full translation adds no fallback sites. This is not a demonstrated Cortex-distortion cause. [Audit](research/MASK-ANGLE-FLAG-AUDIT.md) |
| Sharpening strengths |f081a53 tests Off/Light/Medium/Strong at HD/FHD/QHD against independent pixels and alpha/bars.1440p Medium median0.0522ms in a short isolated filter measurement; host load was uncontrolled and no whole-game latency inference follows. [Presentation](WINDOW-PRESENTATION.md) |

Build61 snow measurements varied: median42.35FPS in a heavier sampled area and
60FPS later. The user reported smooth full-level play. Build63 hub samples had
median60FPS. The4KiB resource cache passes10000 warm-hit/collision/release tests;
these facts do not establish constant60FPS across the game. Sharpening preserves
640x480 internal rendering and original4:3 content; it does not reconstruct detail.

## Private source staging

[**pkyanam/WumpaForge**](https://github.com/pkyanam/WumpaForge) is created and
verified **PRIVATE**, with origin configured and source history pushed. Root
must push the newest evening commits again before handoff and verify remote SHA.
A fresh authenticated clone plus setup dry-run passed. All five dependency
patches replayed on a clean pinned checkout and matched all41 affected files
through build67 (`local/reports/patch-replay67.json`). Source7eec696 is pushed;
the later evening commits still require the final private push.

[README](../README.md) includes the private clone/setup one-liner, BYO supported
USA Xbox ISO, runtime asset requirement, controls and optional AI agents.
[Repository audit](REPOSITORY-AUDIT.md) inspected original reachable history;
new icon artwork is the intentional binary exception. [License inventory](LICENSING.md)
and [notices](../THIRD_PARTY_NOTICES.md) preserve GPL/LGPL and other provenance.
No blanket proprietary license or public/prebuilt release is asserted.

## Shield preparation

Target is **NVIDIA SHIELD TV Pro2019 only**.784c61a adds the primary-source
[Android TV plan](ANDROID-TV-PLAN.md) and isolated [android/](../android/README.md)
preflight/memory-probe groundwork. Nine offline tests and common C syntax pass.
No NDK download, Android cross-build, device execution, APK or playable port.
Pro desktop-GL capability is a promising driver path that still needs a device
probe. Root macOS build is unchanged by this scaffolding.

## Active ownership and next steps

- regression_audit: read-only independent review of mixed flag changes and their
  interaction with shift/rotate provenance. No production edits or compiler.
- mac_runtime: read-only animated geometry/resource lifetime audit for a concrete
  stale-state counterexample. No production edits or compiler.
- Root: integrated build67 is packaged. Continue bounded source audits until the
  final23:45 EDT freeze; private push and final verification remain. Do not launch
  the game for validation unless the user changes the current preference.

Intermittent story/hologram/demo visuals remain unproven; prior GetStatus target0
in story13 remains unreproduced. Physical Xbox/PS5 Bluetooth controllers remain
untested; keyboard completed a level and virtual SDL/ABI tests pass.

Earlier checkpoints and commands are preserved in
[historical notes](history/2026-09-08-checkpoints.md) and Git history; their old
process IDs, blockers and next-step instructions are superseded by this page.
