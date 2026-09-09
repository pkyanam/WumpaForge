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
- Native build65 compiled successfully with the new Display menu. Subsequent
  NaN-color/audio/lifter work needs a combined build66 after source freezes.
  Do not overwrite the running app; `tools/package.py --output` can stage a
  separate candidate. No further live gameplay validation is currently planned.
- `build/branding/WumpaForge.icns` is generated and valid. Icon source/provenance:
  [BRANDING](BRANDING.md). It is integrated by packaging, not yet visible in the
  currently running older bundle.

## Latest fixes and evidence

| Work | Status and evidence |
| --- | --- |
| Retained vertex DMA bounds | f0c6a6c follows64MiB physical aperture with owner/generation checks. ade383e GPU test uses an actual indexed NULL-stream draw and changes adjacent bytes to change pixels. Full post-level return still unverified. [Diagnosis](TANGENT-STREAM-DIAGNOSIS.md) |
| Shared offscreen depth/stencil |656f4c4 fixes five captured256x256 texture views sharing640x480 D24S8. Top-left region, stencil, real occlusion, NULL/rebind, alias and lifetime/rollback GPU tests pass. Included build64. [Contract](TEXTURE-DEPTH-TARGETS.md) |
| Vertex NaN colors |a3aedbf fixes16 reproduced diffuse/specular component mismatches against pinned xemu. Actual56-byte bone input and16 matrix tests pass; no ARL epsilon or RSQ change justified. [Audit](research/VERTEX-NUMERIC-AUDIT.md) |
| Pixel arithmetic |9a1fd88:3072 synthetic comparisons across96 multistage shaders match independent scalar equations with max0/255 output difference. No pixel-generator change needed. [Audit](research/PIXEL-NUMERIC-AUDIT.md) |
| Display controls |632baaf native Display menu,720p/1080p/1440p output and Off/Light/Medium/Strong sharpening. Synthetic menu actions and displayed pixels pass;1440p produced2560x1440 drawable from1280x720 Retina points. [Presentation](WINDOW-PRESENTATION.md) |
| Native spatial audio |de7d92a real min/max-distance attenuation, listener/source positioning and atomic deferred updates; UBSan spatial/stream/252-buffer regressions pass. Stereo positioning is a documented approximation; HRTF/reverb remain unsupported. [Audio](AUDIO-SPATIAL.md) |
| x87 correctness |992e5ba fixes FRNDINT using guest rounding and unordered FTST; emitted-code CPU fixture went61→0 mismatches. Regression agent is auditing FXAM occupancy and byte rotates; wait for its freeze before relift/build. |

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
patches replayed on a clean pinned checkout and matched local changes through
build64; repeat after final pending lifter/runtime patches.

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

- regression_audit: bounded x87/rotate/TLS source audit, CPU regressions, pending
  FXAM state and explicit handling of unmodeled x87 opcodes. Owns lifter template
  and relevant runtime TLS definitions/patches; coordinate before building.
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
