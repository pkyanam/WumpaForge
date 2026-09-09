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
- Current normal run: **build64, PID72315**, log `local/reports/play-25.log`, no
  watchdog. Build63 PID62888 was deliberately closed through its window button.
  Check PID before any Computer Use call: querying a closed app can relaunch it.
- Packaged `build/Wrath Native.app` is build64. User reported the attract demo
  looked glitched. That visual issue is not declared fixed.
- Combined native **build66** compiled successfully from7eec696, including the
  Display menu, NaN color, spatial audio, surface aliases and x87/rotate fixes.
  It is staged at `build/staged/Wrath Native.app`, with ARM64 machine code,
  icon/Info.plist and asset link verified in `local/reports/build66-manifest.json`.
  The CPU agent is investigating logical-shift count masking exposed by a
  compiler warning before a final candidate. Do not overwrite the running app.
  No further live gameplay validation is currently planned.
- `build/branding/WumpaForge.icns` is generated and valid. Icon source/provenance:
  [BRANDING](BRANDING.md). It is integrated in the staged candidate, not yet
  visible in the currently running older bundle.

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
patches replayed on a clean pinned checkout and matched all39 affected files
through build66 (`local/reports/patch-replay66.json`). Repeat after final pending
lifter/runtime patches. Source7eec696 is pushed.

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

- regression_audit: logical SHL/SHR operand/count masking review and focused
  emitted CPU regressions. Owns lifter/template patches; prior x87 work is frozen.
  Root fixed7 DWORD diagnostic formats and a comment in runtime source; include
  these in the agent's next runtime patch snapshot.
- Root: final integration, private push, packaging/icon, source-first validation.
  mac_runtime and native_audio completed their current source/test commits.
- After freeze: relift incrementally if required, full build with at most2 jobs,
  run affected CPU/GPU/audio regression suites and verify clean patch replay.
  Stage an ARM64 bundle with icon and documented1440p menu controls. Respect the
  user's pause on live-game testing; report gameplay/BT/listening gaps honestly.

Intermittent story/hologram/demo visuals remain unproven; prior GetStatus target0
in story13 remains unreproduced. Physical Xbox/PS5 Bluetooth controllers remain
untested; keyboard completed a level and virtual SDL/ABI tests pass.

Earlier checkpoints and commands are preserved in
[historical notes](history/2026-09-08-checkpoints.md) and Git history; their old
process IDs, blockers and next-step instructions are superseded by this page.
