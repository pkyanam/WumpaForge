# Status — September 9 Shield follow-up

The latest work is isolated under [android/shield2019](../android/shield2019/README.md).
The real AOT game library cross-compiles for Android ARM64, and a signed TV
development APK includes asset-free memory/graphics/input/audio diagnostics.
The user authorized real network ADB access. The 2019 Pro/API30 passes sparse
memory, desktop OpenGL4.1, full graphics component and PCM callback checks.
All2267 asset files were transferred and remotely verified. The original game
opening renders, but initial performance is severely slow (one measured segment
16.18FPS) with user-confirmed audio/video desync. Android context bind/release
cost dominates that segment. Performance and remote controls are being improved;
no playable Shield milestone is claimed. Mac build68 remains intact. The earlier
Mac handoff below is retained as its validation record.

## Midnight handoff

The requested evening work is wrapped for **2026-09-09 00:00 EDT** (04:00 UTC).
Build 68 is packaged, source checks are complete, and implementation streams are
frozen. The latest user asks for **source audits and synthetic CPU/GPU checks
instead of further live-game testing for now**. Do not promise 99.5% whole-game
confidence. Keep exact evidence and unknowns separate. Resume substantive work
when the user requests it; the earlier midnight instruction is not recurring.

Root owns full builds, packaging and all game/UI actions. At most two compiler
jobs total across agents. No CPU interpreter/JIT: game code stays ahead-of-time
compiled ARM64. ISO remains read-only. Original assets/generated game C/binaries
remain ignored. Generated project branding is intentionally tracked.

## Running and built artifacts

- User completed **Arctic Antics**, internal Level 7/Demo 0, using keyboard on build 61.
  The post-level hub return later hit a retained-array bounds guard.
- No game process is running as of 23:51 EDT. Build64's `play-25.log` ended
  with `window close requested` at 21:57 EDT; this is not evidence of a new crash.
  Check the process before any Computer Use call: querying a closed app can
  relaunch it. Preserve the current source-only validation preference.
- Combined native **build 68**, source ab6d29d, compiled successfully and is now
  packaged at `build/Wrath Native.app`. ARM64 code, icon/Info.plist and asset link
  are verified in `local/reports/build68-manifest.json`. No game launch was made.
  Build64 and 67 are preserved under `build/checkpoints/`; staged build 66 remains
  available under `build/staged/`. The attract-demo artifact is not declared fixed.
- `build/branding/WumpaForge.icns` is generated and valid. Icon source/provenance:
  [BRANDING](BRANDING.md). The normal bundle now includes it and the Display menu.
  Its Dock appearance awaits the user's next launch.
  Packaging now reads the actual Mach-O minimum macOS version: build 68 targets
  macOS 26.0, on this macOS 26.6 development host. The README distinguishes source
  prerequisites from a particular binary's deployment target.

## Latest fixes and evidence

| Work | Status and evidence |
| --- | --- |
| Retained vertex DMA bounds | f0c6a6c follows 64MiB physical aperture with owner/generation checks. ade383e GPU test uses an actual indexed NULL-stream draw and changes adjacent bytes to change pixels. Full post-level return still unverified. [Diagnosis](TANGENT-STREAM-DIAGNOSIS.md) |
| Shared offscreen depth/stencil |656f4c4 fixes five captured 256x256 texture views sharing 640x480 D24S8. Top-left region, stencil, real occlusion, NULL/rebind, alias and lifetime/rollback GPU tests pass. Included build 64. [Contract](TEXTURE-DEPTH-TARGETS.md) |
| Vertex NaN colors |a3aedbf fixes 16 reproduced diffuse/specular component mismatches against pinned xemu. Actual 56-byte bone input and 16 matrix tests pass; no ARL epsilon or RSQ change justified. [Audit](research/VERTEX-NUMERIC-AUDIT.md) |
| Pixel arithmetic |9a1fd88:3072 synthetic comparisons across 96 multistage shaders match independent scalar equations with max 0/255 output difference. No pixel-generator change needed. [Audit](research/PIXEL-NUMERIC-AUDIT.md) |
| Display controls |632baaf native Display menu, 720p/1080p/1440p output and Off/Light/Medium/Strong sharpening. Synthetic menu actions and displayed pixels pass; 1440p produced 2560x1440 drawable from 1280x720 Retina points. [Presentation](WINDOW-PRESENTATION.md) |
| Native spatial audio |de7d92a real min/max-distance attenuation, listener/source positioning and atomic deferred updates; UBSan spatial/stream/252-buffer regressions pass. Stereo positioning is a documented approximation; HRTF/reverb remain unsupported. [Audio](AUDIO-SPATIAL.md) |
| x87 correctness |992e5ba fixes guest FRNDINT and unordered FTST; fixture went 61→0 mismatches. dddf1a9 fixes FXAM/TLS occupancy and SAR/ROL/ROR widths, with exhaustive byte/count and original CRT sequence tests. FNSAVE/FRSTOR remain explicit unsupported boundaries. [Audit](research/AOT-CORRECTNESS-AUDIT.md) |
| Active surface aliases |cce7d26 fixes stale GPU reads/lost writes through identical shared texture views. Before/after synthetic CopyRects and full GL suite pass. [Audit](SURFACE-ALIAS-COHERENCE.md) |
| Atomic aggregate waits |dddf1a9/7eec696 preserve event/semaphore/mutex state until every object is ready. UBSan fixture went 4→0 failed checks. This XBE does not import the affected multi-object wait APIs; no current stall is attributed to it. [Audit](research/WAIT-ALL-AUDIT.md) |
| Logical/double shifts |1d2a8c5 masks actual operand widths/counts and removes reproduced C shift UB. Actual original CRT64 shift bodies pass edge values and every byte count under UBSan. [Audit](research/AOT-CORRECTNESS-AUDIT.md) |
| Carry rotations and division |0367fab implements RCL/RCR carry rings and wrapping NEG. Four actual original CRT64 division/remainder helpers pass 40000 random pairs plus edge cases under UBSan. Architectural division exceptions remain unchanged. [Audit](research/AOT-CORRECTNESS-AUDIT.md) |
| Mixed flag branches |7b39d0a fixes four original CFG joins, including Aku Aku follower-angle selection. 1236 emitted native checks pass; full translation adds no fallback sites. This is not a demonstrated Cortex-distortion cause. [Audit](research/MASK-ANGLE-FLAG-AUDIT.md) |
| Sharpening strengths |f081a53 tests Off/Light/Medium/Strong at HD/FHD/QHD against independent pixels and alpha/bars. 1440p Medium median 0.0522ms in a short isolated filter measurement; host load was uncontrolled and no whole-game latency inference follows. [Presentation](WINDOW-PRESENTATION.md) |
| Active read-only texture locks |1b385a3 resolves current GPU pixels for synchronized read-only locks of the active mip. Stale-blue baseline fails; full GPU suite passes after correction. Writable active-target coherence remains open. [Contract](TEXTURE-TARGET-LOCKS.md) |
| Guest SHA context |eaf3ace corrects 116-byte guest storage and 24-byte prefix. 79 layout/prefix failures become zero; standard hash vectors and canaries pass. Full save/load remains unverified. [Audit](research/SHA-CONTEXT-AUDIT.md) |
| SAHF and remainder status |383c2f1 couples actual AH flag snapshots with FPREM completion/quotient bits. 805376 flag checks, 15040 remainder cases and the original F4344 loop pass. Exceptional FPREM remains an explicit boundary. [Audit](research/UNORDERED-FLAG-AUDIT.md) |
| Guest frame state |6b2a99e initializes saved EBP and publishes actual frame state at calls, returns and tails. Five baseline failures become 11 passing cases, with independent SEH/longjmp review. [Audit](research/GUEST-FRAME-STATE.md) |

Build61 snow measurements varied: median 42.35 FPS in a heavier sampled area and
60 FPS later. The user reported smooth full-level play. Build63 hub samples had
median 60 FPS. The 4 KiB resource cache passes 10000 warm-hit/collision/release tests;
these facts do not establish constant60 FPS across the game. Sharpening preserves
640x480 internal rendering and original 4:3 content; it does not reconstruct detail.

## Private source staging

[**pkyanam/WumpaForge**](https://github.com/pkyanam/WumpaForge) is created and
verified **PRIVATE**, with origin configured and source history pushed. Final
handoff verification records the remote SHA and privacy in the ignored local
report `local/reports/private-handoff68.json`.
A fresh authenticated clone plus setup dry-run passed. All five dependency
patches replayed on a clean pinned checkout and matched all 43 affected files
through build 68 (`local/reports/patch-replay68.json`). Source ab6d29d is pushed;
packaging metadata and handoff documentation follow it. A fresh private clone at
that commit passed setup dry-run and configured component targets without game
assets/generated C, reusing the already verified pinned dependency source. See
`local/reports/clean-checkout68.json`; this was not another full clean game build.

[README](../README.md) includes the private clone/setup one-liner, BYO supported
USA Xbox ISO, runtime asset requirement, controls and optional AI agents.
[Repository audit](REPOSITORY-AUDIT.md) inspected original reachable history;
new icon artwork is the intentional binary exception. [License inventory](LICENSING.md)
and [notices](../THIRD_PARTY_NOTICES.md) preserve GPL/LGPL and other provenance.
No blanket proprietary license or public/prebuilt release is asserted.

## Shield preparation

Target is **NVIDIA SHIELD TV Pro 2019 only**. 784c61a adds the primary-source
[Android TV plan](ANDROID-TV-PLAN.md) and isolated [android/](../android/README.md)
preflight/memory-probe groundwork. Nine offline tests and common C syntax pass.
No NDK download, Android cross-build, device execution, APK or playable port.
Pro desktop-GL capability is a promising driver path that still needs a device
probe. Root macOS build is unchanged by this scaffolding.

## Validation and next session

- All agent implementation streams are frozen. Independent frame and Pro 2019
  plan reviews reported no actionable new findings.
- Root packaged ARM64 build 68 with icon and Display menu. All 16 selected CPU
  scripts pass through `tools/check.py --suite all`; the SAHF synthetic-only mode
  also passes with original input explicitly disabled. Both CMake GPU/filter
  targets pass with assertions enabled. Original-function audit of 4546 functions keeps 47→47
  fallback predicates; its eight text changes only publish frames in tail bodies.
- Logs: `local/reports/checks/20260909T033047.293080Z/results.json`,
  `component-graphics-check.log`, `component-filter-check.log`,
  `flag-impact68.json`, `build68.log` and the manifest/replay reports above.
- Documentation, package provenance and private staging close the evening pass.
  No game launch was made under the current source-only preference. The next
  useful gameplay checks, when requested, are a full Arctic Antics return to the
  hub, save/load and the reported attract-demo/story artifacts. Continue to pair
  original code evidence with focused fixtures before changing uncertain paths.

Intermittent story/hologram/demo visuals remain unproven; prior GetStatus target 0
in story 13 remains unreproduced. Physical Xbox/PS5 Bluetooth controllers remain
untested; keyboard completed a level and virtual SDL/ABI tests pass.

Earlier checkpoints and commands are preserved in
[historical notes](history/2026-09-08-checkpoints.md) and Git history; their old
process IDs, blockers and next-step instructions are superseded by this page.
