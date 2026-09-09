# Status — 2026-09-08

## Current: build59/story21 reaches next snow fog path; combined build60 pending
- 91f3ed5 fixed XYZ fog passes fullGPU tests and build59/package. Actualstory21
  reachesLevel7/Demo0 andcrossespreviousXYZguard, thenstopsXYZRHWfog FVF0x144.
  Story21PID4981/session40898ended atshaderbreak; nogamerunning, appbuild59.
- Mac decodedoriginal102850 three microprograms: tableNONE→specularalpha;
  tableenabled +deviceflag2→originalZ; otherwise1/originalRHW. Rootverified
  Projection setterFEA20 flag2 iffprojectivecolumn(0,0,0,1). Story21perspective.
  All3implementedwithpreservedoriginaldistance staging; sourcesfrozenpendingGPU.
- Controllerintegratedinternal640x480color/depth back/frontFBOs, resizablewindow,
  aspectletterboxing, F11fullscreen/F10sharpen toggle. IsolatedresizeGPUtestpassed.
  Native_audio's isolatedspatialfilter0fbf5ad passes720/1080/1440GPU/CPUcomparison.
  Combinedpresentation+bothfogpaths smoke nowcontroller-owned(onecompiler).
  Rootwillbuild60 onlyafterpass+cumulativepatchrefreshedincludingCMakeOpenGL.
- Actualupscaler/fullscreen gameandlatency tests explicitlyrequestedbyuser,
  nextlivebuild. Defaultbilinearuntilmeasured, optionalstrength0.25 sharperpass.
  No extraframequeue, internalresolutionunchanged; donotclaimnativeHDdetail/zerooverhead.
- Earlier story19queuedreflectionoverride/retainedboundsfaultstillunresolved;
  no recurrenceonstory20/21. On-stopprobe includescachedaddresses andgenerations
  plusFVF/matrices; bootreloadsPythonatstop(1bd29d0). Hologramdistortionstillopen.
- Nextbuild60→NewGame→originalskip/userwalking→actualsnowcontrolverification,
  fullscreen/sharpenA/B. Goalremainsactive; neverclaimgameplayfromloadalone.

## Earlier: build55/story15 reflection passes; Xbox address5 is next hub setting
- b951812 implements actual[PROJECT2D,DOT,DOT,DOT_RFLCT_SPEC] shader chain, signed
  mapping1 andoriginalreflectionequation. FullGPUtests pass all6cube faces, exact128
  normalcenter, nonunitnormal denominator, diffuseRGBA andinvaliddependency checks.
- Actualstory15build55 reacheslinked31VSreflectionprogram54 then21VSprogram57.
  Nextstopshader_error(texture addressing,unsupportedmode): stage0/1 U=V=5,W=1.
  Stage0screen640x480; stage1/2sameboundtexture; PStexturemodes1057=[1,1,1,0].
  mac_runtime owns exact originaladdress5 mapping andnativeGPUvalidation; controller
  auditsremainingstaticPSdefinitions toreducefutureone-boundary-per-launch cycles.
- No game running. Story15PID38140/session8479endedatshaderbreak; appbuild55current.
  Nextfrozenaddressfix/build56/story16 thenoriginalNewGame/Spaceskip→hub→ArcticAntics.
- Audio-startup01and02 bothsurvived15seconds (watchdogstops). 01probe failedAppleLLDB
  missingSetExecutionPolicy; agentfixed toSetAllowJIT(False) andtestedactualhost.
  02probe succeeded: stream010FF430/16B70Cvtable/method1363D6/nativepointeragree,
  allotherwrappersinactive, pendingpacketsnormal. Originalstory13sporadicfault remains
  unresolved, notdismissed. Includeaudio_probeonstop inallfutureboots; fd9d9fa audit.

## Earlier: build54 texture adjustment passes; hub reflection shader and audio audit
-6c1494c implements original PS_ADJUST_TEXTUREMODES flag100: unbound PROJECT modes
  becomeNONE and boundsamplers adapt to resourceheader dimensions. NONE isRGBA0001.
  Bound/null/rebind and fullGPUtests pass. Native linker retained unusedsampler
  locations, so cacheduniformlookup alone is not relied on forcompatibility.
- Build54/story14 passes prior missingstage and reaches hubnormal-map reflection:
  modes[1,17,17,12], dotmapping111, inputtexture0, flags1FF. Stage0normal128x128,
  stage3cube128x128. mac_runtime implements actualDOTPRODUCT/DOT_RFLCT_SPEC stages;
  controller independently verifies originalpairedshader/dataflow/testvectors.
- Story13 had a separate earlyaudio worker failure~8.4s/movie0/swaps67: original
  5DABB GetStatus indirectcall0 in5DA30. Story14 startup succeeded. native_audio owns
  faithful lifetime/publication investigation; no speculativeaudiochange yet.
  New audio_probe.py reads TLS/wrappers/vtables/native slots withouttargetexecution,
  tolerating unavailablefields. Root starts15s audio-startup-01/session85259 to
  seek reproduction while graphics work continues. AllUI/game launches remainroot.
- Computer Use get_app_state autorelaunched exited app twice (97310 and7717), both
  alreadygone when root attemptedtargetedSIGTERM. AlwayscheckactualdebugPID/log
  beforeUI; no independentdemo is protected/current. Story14session85624ended.
- Pixelprobe nowcapturesactivedefinition successfully (story14). Story12 failed
  optionalredundantconstantshadowread wasremoved7e1b56a. Build54packaged, nextbuild55
  afterreflectionGPUchecks; then normalNewGame/skip→hub→ArcticAntics(Level7/Demo0).

## Earlier: story11/12 hub passes morph; programmable texture adjustment next
-16fe0e9 implements a conservative component dependency proof through registers,
  masks, paired instructions, ARL/relative constants and exactzero multiplication.
  CPU edges/fullrenderer smoke/actual88shader nativeGPU tests pass, including
  individual XYZ+W changes to all10secondary inputs and nonzero-weight rejection.
- Build53/story11 passes prior hub morphology stop and now reaches13VS/2combiner
  effect draw A5D40(type6,count254,stride36), but demands an unbound texturestage.
  Stage0 is640x480 live texture; stage1isnull. Filenamehub, Level37/Demo0.
  Player(0,7.848237,2.31); Arctic portal center(9.25314,7.80484,9.57952).
- Controller matched exact original PS1ADF40 and13VS1ADE68 registeredatB6F19.
  PStexturemodes21 declaresstage0/1PROJECT2D; equationsreadonlyt0. Finalconstants1FF
  enables original107CA0 automatic modeadjustment. mac_runtime implements flag-
  controlled null-stageNONE/bounddimensions adjustment plus originalNONEalpha1.
  Native GL retained the unused sampler in its fixture, so linkeduniform filtering
  alone is insufficient; originalSDKadjustment is the required compatibility rule.
- story12 repeated sameboundary to improveprobe; itsshaderJSON failed on optimized-
  away redundant s_pixel_constants shadow. Root removedthatread7e1b56a. Nextprobe
  stillincludescompleteactivepixeldefinition, actualtextures/boundmetadata/hubstate.
  No game running. Build53packaged; story11session75365/story12session40721 ended.
- Next frozen textureadjustment+GPUtests/build54/story13, normalNewGame/Spaceskip,
  verifyhubpixels/movement and originalportalentry toArcticAntics(Level7/Demo0).

## Earlier: story10 corrected Cortex, original skip, then hub morph dependency
- Build52 includes017dc9e null-texture combiner fallback andeb21d33 exact-dead
  bound-input handling. Combined native GPU tests pass, including actual69shader
  captured constants and dynamic nonzero regression. Cortex now visibly renders
  and animates in chamber near60FPS; no initial character-range errors occur.
- Root pressed Space during chamber; original story skip sets movie=-1/cut_on0
  and returns to Level37 hub. Previous fixed texture stage crash is crossed.
- Next bounded diagnostic stops at hub88-instruction shader, v6..v15 FLOAT4stream1,
  offsets0..144, retained1600-byte buffer stride160, primary13vertices index0..12.
  c122 is0 but direct proof is conservative and cannot prove transitive dependency.
  Original88shader slot23 multiplies accumulated morph byc122.x then addsv0;
  original B6310 disables morph without unbinding the prior stream. Controller
  implements conservative all-input dependency analysis with real88 GPU checks.
  mac_runtime confirmed original n*160 allocation and native metadata are consistent;
  no bounds relaxation or buffer expansion is justified (ce463f3 report).
- Exact target verified from original tables: ARCTIC ANTICS, Hub0/slot0, internal
  Level7, filenamea/snow_m/snow and penguinp assets. Enter by WASD into portal radius
  and remain60updates; no A-button selection. Verify Level7/Demo0 (attract usesDemo1).
  See ARCTIC-ANTICS-ENTRY.md; shader probe now reads position/portal/timers/nextlevel.
- No game running. Build52 packaged; story10 session64259 ended at deliberate first
  failed fetch breakpoint. Next frozen dependency fix, combined GPU validation,
  build53 and story11; validate original skip, hub pixels and movement to portal.

## Earlier: story08 completed; story09 identifies stale inactive character input
- User specifically requests the winter/penguin level in area1. This refines the
  first-realm playable milestone; keyboard remains available input. User confirms
  story kept playing with missing components, then crashed at its end.
- Story08 all11 original scene boundaries completed in365.153s. Both dialogue tracks
  played; native video sustained~60FPS. Masks/environment observed on actual frames.
  Full rendering correctness remains false: many character draws were rejected.
  33 SDL underrun callbacks over fullrun; modest offsets remain, no multi-second drift.
- Actual post-story stop: fixed pixel stage0 COLOROP4 ALPHAOP4 invalid binding at
  movie=-1, cut_on0, level37, fade82, swaps24305. mac_runtime owns original SDK
  null-texture combiner passthrough behavior and bound/unbound GPU regression.
- c99de6a adds bounded stream/index diagnostics and WRATH_BREAK_STREAM_ERROR.
  Build51/story09 catches first failed fetch atscene2/swaps3323: shader69instructions,
  v6 FLOAT3stream1, handle2D4E160 data2D51980 bytes1280 stride160, indices0..47,
  requiredend7532. Crucially exactdeadproof=1 and c122=(0,0,0,0): stale bound
  secondary data is provably irrelevant. controller_support owns applying that
  exactproof to bound buffers, with nonzero-weight/bounds GPU regressions.
- No game runs now. Build51 is packaged. Next combined frozen fixes/build52 then
  story10: verify character rendering before testing original skip and hub entry,
  navigate to first winter level through original game controls. No fake asset load
  substitutions or weakened live-input bounds. Root owns builds/launch/UI.

## Earlier: build50/story08, chamber now renders and plays beyond prior stop
- a987ae4 implements original shader constant mode0/1 and programmable fog modes
  using original SDK coefficients. Combined native GPU tests pass; build50 arm64
  packaged successfully, no additional lift needed.
- Actual New Game -> DONE -> story succeeds. Uka Uka is visibly animated in the
  chamber in Computer Use screenshots. Mode1 no longer aborts atscene2 frame1.5.
  Sustained chamber windows run60FPS; zero repeated texture uploads. This is
  partial character rendering, not complete asset/render correctness yet.
- Repeated stream1 attribute6 range failures reject some character draws. Current
  example type5 count144 indices2D2DD0C stream0data2D2A570 stride56. controller_support
  owns exact range/binding diagnosis and bounded optional failure probes; do not
  remove bounds checks or substitute geometry. Source edits ongoing, rootbuilds.
- story08 PID51552/session47281 launched480s; root intends to allow fullstory unless
  it fails naturally. Agents must not launch/attach/pause/terminate it. Audio agent
  monitors timing and mac_runtime monitors performance. Keep package untouched
  while live. Next capture actual subsequent scenes/stop and repair required data.

## Earlier: story07, 60 FPS station/corridor and next character boundary
- Build49 integrates the real secondary vertex streams, paged resources and exact
  P8 palette change detection. Actual original New Game -> default DONE works via
  keyboard/Computer Use. All11 story instances load; station and corridor complete.
- Palette rebind/lock no longer invalidates identical texture bytes. Station and
  corridor run60FPS with zero repeat texture uploads; corridor audio advances6.6293s
  against6.633s animation/vblank time. No audio underruns/overflows. Audit bdd21c0 corrects
  the original per-scene frame1 origin: stable119–123ms source offset is consistent
  with the original worker's100ms sleep after marking the stream ready, plus
  scheduling. No timing/rate changes justified; actual character lip sync remains
  unverified. See docs/research/STORY-PLAYBACK-AUDIT.md.
- Story07 crosses the previous stream1 deformation failure and stops at original
  SetShaderConstantMode1026F0(1), caller398CA, movie1 scene2 position1.5. Original
  mode1 enables the existing192 physical constants without clearing/remapping them.
  controller_support owns faithful mode0/1 transition support and GPU regression.
- Story06 attract Level7 crosses the fog-color setter and reaches enabled distance
  fog unsupported in shader_bridge.inc. mac_runtime owns native linear fog using
  original SDK coefficients and actual oFog output, with GPU regression.
- Root added all146 cached render states to shader_probe.py for read-only fog
  diagnosis. No game is running; build49 app is current. Next integrate frozen
  mode/fog work, build50 with at most2jobs, package only while no game runs, then
  story08 >=480s with sparse story/shader probes. Enter New Game promptly before
  attract timeout; get_app_state can relaunch a closed app, so check PID first.
- First-realm playable gameplay and physical Bluetooth tests remain unverified.
  User has no controller now: proceed using keyboard; original A/Start skipping
  remains the intended intro choice, to be tested after full playback advances.

## Earlier: story05, real keyboard navigation and first backstory scenes
- User confirms keyboard navigation works and the backstory space station/audio
  played, then drifted and stopped before characters. Root also verified actual
  automated Space -> New Game -> default DONE -> Space with build48. e0076f3 fixes
  original double input reads36A21/363AB consuming brief taps; both guest reads now
  see A255 in story05 trace. Physical controllers unavailable per user; continue
  keyboard and leave Bluetooth verification explicitly pending.
- Resource paging b60db46 is proven needed: story02 attract load uses2947 live
  records; story04/05 backstory uses3471 in14pages (capacity3584),25,207,796 described
  guest bytes. All11 original story instances load. Prior small-handle Releases
  and model memcpy crash no longer occur along this story path.
- Original backstory movie1 is11scenes, nominal364.967s plus loading/menu. Audio
  INTRO1+INTRO2 total365.978s, supported mono44.1kHz XboxADPCM. tools/story_probe.py
  provides sparse original scene/start/end snapshots; reports STORY-PLAYBACK-AUDIT
  and STORY-SYNC-AUDIT. boot.py supports --probe-story and up to600seconds.
- User report of closure before OK on story03 coincided with root intentionally
  stopping its short180s diagnostic to relaunch480s; root explicitly apologized.
  story04/05 later closures are actual shader breakpoints, not watchdog expiration.
- story04/05 reach movie1 scene2(chamber), animation1.5 then stop at live vertex
  stream6 declaration: FLOAT3 stream1, c122.x1. Actual story05 rejected bind is
  stream1 handle15C9450 stride12 caller3A6D1 swaps4104. This is real deformation
  data generated by A5F50, not absent/default data. controller_support implements
  all16 bindings and matching indexed/base/start/quad fetches; native GPU fixture
  passed, shared source awaiting final combined commit/build49.
- Station desync quantified:90animationunits(3s) cost509vblanks(8.483s),~5.48s lag.
  Station20–22FPS with38–42ms/frame texture uploads; corridor returns60FPS. Audio
  output stays active, short512–1024frame queue, no new underruns. mac_runtime found
  SetPalette/Lock increments revision despite identical exact palette bytes,
  defeating P8 reuse. It owns byte-change-only revision fix +regressions now.
- Attract green LOADING in story02 is main thread GPU-wait103370 through unbridged
  FDA80 fog-color setter;86BC0worker completed. Exact FDA80 native cache bridge
  and actual fogcolor uniform pass GPU tests and are included in build48.
  Enabled distance fog remains explicitly unsupported. Actual attract advance
  beyond this setter not yet verified. docs/FOG-COLOR.md has evidence.
- Build48 packaged arm64 passes. story05 session66597 and all game processes have
  exited. Root must integrate frozen multistream/cache/fog shared graphics source,
  build49, refresh package only with no game running, then story06 with>=480s and
  profile/audio/input traces. Stop at real unsupported boundaries; no fake skips.

## Latest: build45/46, near60FPS intro and backstory loading priority
- User explicitly prioritizes the original backstory movie alongside first-realm
  gameplay. Preserve native original story playback and A/Start skipping.
- f76a459 removes the measured streaming VBO overwrite stall by replacing storage
  for each UP upload. Ordered1024draw GPU regression passes256 distinct pixels.
  Matched intro boot44→45:53.02→59.29FPS overall, accumulated animation/vblank lag
  2.77→0.25s; sustained heavy windows now59–60FPS, remaining isolated long frames.
- 741d5e2 fixes retail CreateIndexBuffer ignoring format/usage/pool: D3DX passes65,
  game passes2C, both valid. Boot44 empty mesh registry confirms old rejection;
  native draw/lifetime regression passes. Build45 crosses the old null-mesh stop.
- 777c71c adds keyboard/mouse logical port0 merged with SDL gamepads. Focus/mapping,
  reconnect/rumble and packed guest ABI fixtures pass. docs/CONTROLS.md documents
  keys. No real keyboard navigation or physical Bluetooth test is yet confirmed.
- Boot45 now fails in original read97380 (REP MOVSD) copying to a bad destination
  returned by3A400 during model loading:97B00→A9680→A9960→9C3D0→9CAE0→9CC80→9CD00
  →2B7E0→2B850→2BD80→2C020→87400→2D950. Level37,Demo0. Invalid Release small
  handles precede it. This is a new memory/registry blocker; no backstory frames
  or gameplay completion claimed. Native mac_runtime agent owns investigation.
- Root's bounded Release diagnostics plus debugger native/TLS register commands
  are in build46/boot46 (PID62949 at launch). All earlier game processes exited;
  Computer Use get_app_state may relaunch a closed app, so check PID first.
- Packaged build now refreshes from native binary and boot.py --app debugs it
  using stable local.wrath.native identity. Window close/CmdQ ends host process;
  graceful original guest-return worker shutdown remains separate unfinished work.
- native_audio owns generic mixed CMP/TEST/backedge ZF fixes for proven original
  menu cancel branches6ECA4/7012A. No lift/rebuild until it is validated/frozen.
  controller_support audited short tap latch option; current snapshot can miss
  presses entirely between polls. Root owns all UI/game launches and builds.

## Current checkpoint: boot43, verified title/menu and next gameplay blocker
- The user's standalone family demo naturally exited after ~109 seconds at a real
  unresolved null call. PID94972 is gone; no protected/live demo remains. Boot43
  reproduces the same loading failure under LLDB. No game is running at checkpoint.
- The viewport constant fix restores the Traveller's Tales model/logo and the
  Crash Bandicoot: The Wrath of Cortex title. Actual GPU captures were inspected:
  local/reports/boot43-frames/frame-000900.png and frame-001800.png. The latter
  visibly contains NEW GAME / LOAD GAME. Title/menu launch is now verified.
- Boot43 stops at original call3B460 through a D3DX mesh vtable slot+38 resolving
  to0. Stack3B2E0 ->9A6C0 ->A9960 ->A9BF0 ->A9D40 ->A9F60 ->7AF50 ->1DBC0 ->87870.
  State Level7, Demo1 identifies attract-demo loading; gameplay remains unverified.
  Original mesh constructors initialize nonzero slots111140/11115D. Capture the
  live registers/mesh table/object/vtable next; never bypass the null call.
- All2267 extracted files match SHA-256 of their original ISO extents; report
  local/reports/asset-sha256-verification.json. Boot43 missing cubemaps are optional
  in original1DEA0; missing stuff/xloading.nux falls back to existing xloading.nus
  in A9F60. These misses do not explain the mesh crash.
- Audio is audible, but full-speed sync remains unfinished. Heavy boot43 windows
  run30–40FPS. At30FPS, work31.5ms wall/12.4ms CPU, context0.86ms, event0.04ms,
  draw submission0.97ms, upload5.28ms. Profile remaining waits before optimization.
- User's next milestone: playable first-realm level (at least one of the first
  five), optional viewing/skipping of original intro/story, keyboard/mouse and
  Xbox/DualSense gamepads, optimized performance. Three active agent assignments:
  controller_support owns input; native_audio owns mesh crash/runtime/probe;
  mac_runtime owns performance profiling. Root integrates and runs the game.
  Keep at most2 compiler jobs total and no concurrent game launches.
- Build43 and combined GPU viewport/cache/driver fixtures pass. Current uncommitted
  integration includes viewport source, diagnostics, asset audit and frame capture.

## Latest implementation checkpoint: boot41, cache improvement and live scene1
- 4c06da8 adds exact encoded-byte texture/palette snapshots, capped64MiB with
  correct direct-write detection, release and uncached fallback. Actual GPU tests
  cover unchanged reuse and mutations to base/mip/cube/index/palette data.
- 56430ef proves the unbound v6 input has only exact-zero-multiply uses at live
  c122=0. Rechecks each draw; nonzero/relative/other uses remain errors. Full build41
  passes, boot41 crosses the previous stream failure and runs to45s watchdog.
- Texture optimization is substantial but not uniformly60fps: heaviest matched
  intro windows20→39fps, decode/upload37.56→4.59ms/frame (~88% reduction).
  Earlier assistant commentary saying the previously slow section runs60fps was
  too broad. Later scene1 windows are mostly60fps; earlier intro still accumulates
  ~3.6s animation lag over1260updates. User's sync concern remains unresolved.
- Slow residual windows have~24–25ms work wall versus~10ms thread CPU, with
  Present1–2ms and no software pacing sleep. Context/driver waits within SDK work
  are next profiling target; do not remove locks/pacing based only on inference.
  Up to175draws and245texture binds per frame in later intro windows.
- Actual frame1300 capture viewed: local/reports/game-frame-1300-boot41.png shows
  stars/nebula during transition. Scene1 loops animation0..100 at near60fps after
  that, but Crash title/menu pixels are not yet visually verified. Boot42 was
  intended to capture frame1800; inspect its files before claiming that artifact.
- All agents completed/frozen. Source tree committed except any later root docs.
  Goal remains active; physical controllers, full-speed sync, title/menu/gameplay,
  save/load and clean shutdown verification remain. Preserve the live dad demo.

## Goal
Run the supplied game natively on Apple Silicon without emulation. **Native title/menu verified; playable gameplay remains the current blocker.**

## Latest checkpoint: boot39, audible audio; video slow and second stream next
- User confirms the new run has audible publisher intro, but video runs below
  full speed and audio/video are out of sync. Treat this as an actual remaining
  defect, not completed audio/video integration. mac_runtime audits render/present
  pacing and proposes bounded low-overhead measurements for the next root run.
- Root read-only CoreAudio probe confirms MacBook Air Speakers, master mute0,
  volume1.0,48000Hz. tools/audio_output_probe.c never changes settings.
- Boot36 revealed every stream Process returned80070057; boot37 captured actual
  packet data80000000,36864bytes,status431EE4. The native bridge rejected the
  separate allocated contiguous-memory window. 0d5a232 fixes bounded validation
  while preserving high addresses. UBSan sparse-memory regression verifies actual
  ADPCM output from80000000 with deliberately different low-memory bytes.
- Boot39 accepts original44.1kHz mono ADPCM and emits actual native PCM. Last
  audio trace:961792 callback frames,1604910 nonzero samples,peak32768,one underrun,
  zero overflows. The earlier boot30 gate advancement did not prove playback;
  it was silently rejecting packets until this fix. User hearing is now confirmed.
- 3bdd8eb adds nine verified emission seeds; analyze/lift36 and generated compile
  pass. dfb7813 restores actual render defaults; full component GPU tests pass.
  5b3d519 adds ARL and relative constants, with actual69-instruction GPU compilation
  and matrix-index transform-feedback tests. Full builds38/39 pass.
- Boot39 links that69-instruction native shader, then stops at782 swaps on its
  vertex declaration: used attribute6 is FLOAT3 in stream1; current binder accepts
  stream0 only. Same original mesh as prior ARL failure. Live declarations, program
  and constants: local/reports/boot-39-shader.json. Stride optimized away, so actual
  vertex byte capture is explicitly unavailable. controller_support owns the
  demonstrated next stream binding contract and graphics/index/shader integration.
- Boot36 used new audio diagnostics/generated callbacks with prior graphics/vertex
  objects to avoid concurrent edits; boot38/39 are consistent full builds. All
  sessions3543/65368/35412/93466 have exited; no game intentionally running.
  tools/boot.py --probe-shader plus tools/shader_probe.py now captures state at an
  actual shader boundary or bounded stop, including slots and vertices if available.
- No verified Crash title/menu/gameplay. Physical Bluetooth controllers, frame rate
  and sync, save/load, sound quality and clean shutdown remain. Goal active; max2
  compile jobs. Root coordinates all game launches and integration builds.

## Previous checkpoint: boot35, Traveller's Tales animation
- User-requested live preview ran build31 once (user-launch-01.log) and naturally
  stopped at the known mixed shader failure. Session96785 exited134; no game left
  running. Estimate communicated: hours to days for menu, days to weeks for reliable
  gameplay, highly uncertain because unseen scenes expose additional contracts.
- a4d1358 implements programmed vertex with actual fixed pixel-stage equations;
  5db4822 restores original SDK texture defaults from loaded-image table10BD9C.
  Standalone combined native GPU tests pass, including default initialization,
  modulation+ADD with retained alpha, live factor uniforms and existing regressions.
- Incremental build34 passed. Boot34 and read-only shader capture boot35 pass the
  prior mixed failure at614 swaps. Actual frame760 shows the Traveller's Tales
  publisher text beginning over stars/planet: local/reports/game-frame-760-boot35.png.
  Still no verified Crash title/menu/gameplay. Both bounded runs have exited.
- Next explicit failure at782 swaps: vertex translation instruction5 MAC13 (ARL),
  original vertex handle29706641, pixel0,69instructions, indexed36 vertices stride56.
  Captured actual instructions/live constants in local/reports/boot35-shader.json;
  tools/shader_probe.py reproduces read-only LLDB export at shader_error.
  native_audio owns bounded vertex ARL/relative addressing implementation/tests.
  controller_support audits remaining SDK render defaults read-only; mac_runtime
  completes the independently evidenced11-entry emitter callback seed coverage.
- Root owns integration/build/launch. No full rebuild until seed changes are ready;
  max2 compile jobs. Title/menu, physical Bluetooth controllers, gameplay/audio
  quality, save/load and clean shutdown remain unverified; goal stays active.

## Previous checkpoint: boot31, animated Universal globe
- Native ARM64 AOT game code renders original Universal globe/planets/title text
  over stars at frame600. Actual captured/viewed artifact:
  local/reports/game-frame-600-boot31.png. This is the original publisher animation,
  not yet the Crash title menu/gameplay. No synthetic pixels or animation bypass.
- Commit174ede7 preserves compatible comparison snapshots at CFG joins, fixing
  WAV parser138567. Native tests pass; generated unknown flag branches70→57.
  Some57 sites are aliases/data, others real correctness gaps: see research audit.
- Commit0a16572 adds real bounded native packet streaming and seven verified COM
  bindings. Decode/queue/completion/pause/flush/lifetime tests pass (UBSan plus actual
  producer). Full build30 and bootstrap30 passed. Manual exclusions90. Audio gate
  clears normally, animation advances300→347.5 with original increment0.5.
- Commitcc99bcb seeds original emitter callback31E20 from table199C1C and call34A82.
  Analyze/lift/build31 passed; actual game executes through that prior failure.
- Boot31 without per-frame LLDB probes had zero logged CoreAudio overload messages;
  audible quality remains unverified. Bounded run stops after614 swaps at
  shader_program: mixed fixed/programmed stages not implemented, guestreturn3B0B1,
  indexed26vertices from3B010→A5D40→A8240→9F370→A0990→A8140→2D950.
- Boot32/33 captured exact vertex/pixel handles/FVF/stage state at shader_error;
  inspect local/reports/boot-33-pairing-full.log. Both sessions8100/63799 exited.
- Native GPU probe0a7faa5 explained old black frame: stars rendered, then an
  original black near-depth quad occluded later geometry while intro waited for
  sound. Audio startup removed that hold; no graphics-state bypass was needed.
- Goal active and incomplete. Physical Xbox/DualSense Bluetooth tests, real game
  menu/input/gameplay/audio-quality verification and clean shutdown remain ahead.
  Use at most2 compile jobs; preserve original assets and no CPU interpreter/JIT.

## Previous checkpoint: boot28, research complete and intro hold measured
- Research completed in bcd1249; docs/research/README.md links four primary-source
  audits. Graphics implementation before the pause is86b9630. Decision: continue
  current AOT/native SDK route with targeted decomp references, no wholesale pivot.
- Post-research boot27/28 read-only LLDB probes prove fade255→0 after32 intro
  updates, then stays0 through watchdog (~539 swaps in boot28). Animation position
  stays300 with increment0; intro0, scene0, audio gate1, pause0. Frame120 boot27
  captured/viewed still black. These runs used the existing build, no behavior edits.
- Boot28 early channel4 index−1/kind0 and intro_mode2 later become index0/kind2
  and mode255; cut_audio172 throughout. Audio investigator traces original stream
  status5D4B0 because the initial mode2 gate clears but animation remains held.
  Stream wrapper for index0 is431ED8, relevant byte+24 and COM pointer+4. Do not
  force animation to advance or label missing stream creation as proven until traced.
- First8 post-fade draws captured in local/reports/boot-28-intro-draw.log: native
  color mask15 and cull NONE; guest color-mask cache0 is not proof writes are off.
  Actual fixed FVF152 geometry and142 immediate quads, matrices/vertices/stages are
  recorded. Draw2 has disabled texture stages and black diffuse, depth enabled/write;
  later sprites have white diffuse and alpha GREATER247. Several transformed sprite
  vertices fall inside clip bounds. Need actual fragment/target/depth evidence to
  explain black output; missing fixed combiners alone is not yet its proven cause.
- tools/intro_probe.py provides reproducible bounded snapshots; invoke through
  `python3 tools/boot.py NAME --seconds 18 --probe-intro`. It
  alters debugger timing, so logged wall time is not a performance measurement.
  Boot27/28 exited; no game intentionally left running. Next: finish stream-status
  trace and actual rendering ledger, then fix a reproduced contract.

## Implementation state through boot26
- Original XBE CPU code executes as AOT-compiled ARM64. Universal splash/fade
  verified; actual green loading object frame40 was captured and viewed at
  `local/reports/game-frame-40-boot23.png`. No verified Crash title/menu/gameplay.
- Boot26 completes loading, joins its worker, initializes native SDL CoreAudio,
  and enters original intro/cutscene loop2D950. Watchdog samples indexed rendering
  through A8140→A0990→9F370→A8240→A5D40→3B010 and P8 texture upload. Actual GPU
  frame120 (`local/reports/game-frame-120-boot26.png`) is solid black. This is not
  a proven title loop or a proven expected transition. Boot26 bounded run exited.
- Immediate Begin101EC0/End101F00 and SetVertexData4f101E60/2f101E20 are now native;
  fixed/programmed GPU tests cover real attributes, position emission and state
  restoration. `docs/IMMEDIATE-DRAWS.md` records retail4361 ABI evidence.
- Fixed XYZ draws now consume original device matrix cache (VIEW0, PROJ1, WORLD6),
  preserve original SetTransform execution, upload D3D row-vector matrices correctly
  and convert D3D clip depth into OpenGL clip depth. GPU test fixed_mvp covers
  noncommuting matrices, perspective, XYZRHW preservation and depth/viewport reads.
- Combined graphics smoke tests and native build26 passed. These component tests
  do not establish that every state of the actual intro is implemented correctly.
  Manual SDK exclusions total83. Graphics patch preserves both GL source and CMake.
- Committed heap/release fix6afd888 preserves guest DWORD layouts and reuses released
  allocations. Boot23 loading used~36.7MiB of53.875MiB heap. Registered callbacks
  2B880/2B950 execute as compiled code (1adb817). Runtime worker exit and audio
  integration are checkpoint4de3375; thread-object views are per-query snapshots.
- Audio initializes and original worker5DA30 runs; game sound is not heard/verified.
  DSP effects/HRTF/spatial features remain unsupported. Guest workers must stop
  before audio teardown and guest-memory unmap; clean exit remains incomplete.
- Three agents audited existing decomp projects, Xbox graphics contracts and
  startup/runtime behavior; root audited ARM64/AOT. Reports live in docs/research.
  No new game launch occurred during that research pause; probes followed it.
- Startup audit establishes fade255 decreases by8 per intro update, independent of
  wall-clock queries. Next diagnostic should pair exact fade/animation/selector
  state with actual draw state and pixels. Do not speculate that black means timing.
- Known graphics gaps include fixed texture combiners/sampling and multiple stages,
  mixed fixed/programmed pairs, advanced dependent modes, fog, volume textures,
  packed attributes and stream>0. P8 reuploads are a possible performance cost.
- Physical Xbox/DualSense Bluetooth input remains untested; virtual SDL tests pass.
  App bundle is stale. Use max2 compile jobs; preserve source/patches in Git and keep
  game assets/generated binaries ignored. Goal remains active and incomplete.

The older numbered checkpoints below are historical; this section is current.

## Environment
- Workspace started with only a 931 MiB ISO and `.DS_Store`; initialized Git on `main`.
- Host reports arm64. Xcode beta, Homebrew, Python 3, CMake, and SDL2 are present.
- Input: `Crash Bandicoot - The Wrath of Cortex (USA).iso` (preserved in place).
- Original Xbox XDVDFS confirmed; ISO 9660 is a dummy descriptor.

## Initial upstream findings
- https://github.com/Open-Travelers/OpenCrashWOC : GameCube-derived C reimplementation,
  no releases, README says gamecode 70%, build WIP, incomplete engine and dummy sound.
- https://github.com/denzi-gh/crashwoc-decomp-ps2 : active matching decompilation targeting
  PAL v1.03 SLES-50386, not a native host port. No releases.
- https://github.com/denzi-gh/crashwoc-decomp-gc : matching GameCube USA decompilation,
  no releases; branches include multiplayer and custom text, no advertised native port.
- https://github.com/calmsacibis995/crash-ps2 : USA v1.00 project explicitly unbuildable
  and very incomplete.

## Confirmed disc / selected approach
- Original Xbox XDVDFS at partition base 0; the ISO9660 descriptor is a dummy.
- Extracted only `default.xbe` (1,777,664 bytes) to `local/assets/`.
- Title `Crash Bandicoot: tWoC`, ID `0x56550003`, North America, certificate version 1.
- Build 2002-03-19, XDK 4361, entry point `0x000EF089`, 113 kernel imports.
- Inventory and XBE analysis in `local/reports/`; about 972 MB of file contents on disc.
- OpenCrashWOC CMake configures but compile fails immediately on incomplete C;
  it still uses GameCube GX and is not an available native game implementation.
- User explicitly accepts native ARM64 ahead-of-time recompiled game code using
  compatibility layers for Xbox graphics, audio, and system calls.
- Selected `third_party/xboxrecomp` (revision in UPSTREAM.md). No CPU emulation/JIT.
- Python venv installed only Capstone 5.0.7 (native arm64 wheel).

## Native build progress
- Fully extracted 2,267 disc files into ignored `local/assets/` for runtime I/O.
- `tools/pipeline.py` reproduces prepare/assets/analyze/lift stages, pinned to upstream.
- Disassembly recovered 4,627 functions (3,056 in main .text). C lift generated 689,660
  lines; 66 unresolved targets and 107 unsupported instruction instances are tracked.
- Generated unsupported instructions and unresolved calls now stop with diagnostics
  when reached, rather than silently skipping logic. See `patches/xboxrecomp-lifter.patch`.
- Generated ARM64 objects compile after handling a privileged debug-register move as
  an explicit unsupported-instruction diagnostic.
- Native host in `src/`; CMake build is `build/native`, max two jobs.
- Runtime port fixes are being saved in `patches/xboxrecomp-runtime.patch`.
- User permits helpful subagents. Controller agent implemented/tested native SDL2
  input and is inspecting D3D integration. Runtime agent fixes safe macOS mappings.
  Audio agent implements missing POSIX output/cadence.
- Homebrew libepoxy 1.5.10 installed (2.7 MB). SDL2 here is sdl2-compat over SDL3.

## First native boot (13:33 EDT)
- `build/native/wrath_native` links, 8.2 MB Mach-O arm64 executable.
- Real recompiled XBE entry executes memory/CRT initialization. Boot log:
  `local/reports/boot-01.log`. Exits through HalReturnToFirmware after kernel ordinal
  202 (`NtOpenFile`) returns C0000034. This is a game startup failure, not success.
- LLDB follow-up identifies failed path as bare `\\Device\\CdRom0`; path mapper only
  accepts a trailing slash. Runtime agent fixing actual device-root handling.
- Native controller virtual-device tests and audio dummy/CoreAudio tests pass.
  Physical controllers and in-game audio are not yet tested.
- Input agent identified exact XDK 4361 D3D API addresses using signature matches
  and local disassembly, implementing first four hooks in `src/graphics.c`.
- `tools/bootstrap.py` pins checkout, applies tracked patches without overwriting
  other local changes, and installs only the pinned Capstone dependency.

## Input fingerprints
- ISO SHA-256: `1caef217dd588655a851ce5ae3cdead912bf383ae86e574e9b33aa74c12900bc`
- XBE SHA-256: `e8d7cbf225d899eb88227c11d1f40434c34e27c1b23ed946fb2c3471168d2f4d`

## Current checkpoint (boot-10)
- Real native ARM64 entry and CRT execute. Disc root, optical media query, save
  directory opens, title metadata creation and sparse HDD metadata reads now pass.
- Safe guest runtime layout starts above the XBE image, avoiding its large .data.
  Sparse 4 GB virtual reservation has 64 MB of guest RAM; it is not 4 GB resident.
- Boot-06 reached first game main at 0x00087BF0 (LLDB breakpoint). No actual
  title/menu visible yet. Boot-07 crashes on NtDuplicateObject from game main: a
  current-thread pseudo handle was being treated as a native handle pointer. Fixed
  and tested independent duplicate lifetimes/current-thread identity; boot-08 passes.
- Boot-08 creates the real native 640x480 graphics device and a guest texture-level
  surface. It then stalls in the original D3D pushbuffer routine from render-state
  initialization (FD830 ->103740). Boot-09 passes that boundary with actual native
  blend/depth/cull state hooks, then stalls on FillMode (FDDF0). Graphics agent is
  handled FillMode, passthrough texture coordinates, and enabled alpha test with
  actual fragment discard. Boot-10 passes material setup, loads actual2.7MB and
 786432-byte asset reads, then stalls at FEF20 SetRenderTarget. Graphics agent is
  adding FEF20/FF830 native default-color/depth handling. No title/menu pixels verified.
- Boot-05 aborts on a missing target inside CRT memmove 0xF5DD0: function discovery
  interpreted inline jump-table data as instructions. Native audited cdecl memmove
  bridge added; overlap/alignment/stack tests and boot-06 confirm the fix.
- Native D3D create/clear/swap, texture/VB and non-indexed draw bridges are integrated.
  Synthetic textured-quad readback passes on GL4.1 Metal; this is not game rendering.
  Programmable shaders, indexed draws, render targets and other state need work.
- Seven native XAPI controller bridges integrated. Virtual-device tests pass;
  physical Bluetooth Xbox/DualSense testing remains pending.
- Native CoreAudio output works in isolated tests. Real game sound is not integrated;
  startup creates 252 Xbox ADPCM buffers, requiring decoding and lazy voice allocation.
- Runtime details: docs/RUNTIME.md. Graphics/input ABI evidence: D3D-INTEGRATION.md,
  INPUT-INTEGRATION.md. Build max two jobs; execute from local/run to contain logs.

## Next
Run the rebuilt native game through CRT/cache startup and stop on concrete faults.
Implement the next actually reached compatibility or translation gap, preserving
original game behavior. Keep all stubs/limitations explicit. Verify real title/menu
pixels and then gameplay/input before reporting success.

## Reproducible bounded boot
`python3 tools/boot.py boot-NN` writes the LLDB report under local/reports.
Use `--break-at sub_00087BF0` to verify game-main entry. Real crashes need LLDB
`-k` commands (the helper includes them); process exit 0 is not game success.

## Work after the graphics-init checkpoint
- Audio worker entry 0x5DA30 seeded from its actual CreateThread argument; analyze
  now finds it (4628 functions total). Four-pthread memory regression verifies
  independent guest TIB/TLS/registers/stacks and synchronized heap allocation.
- Recompiler chunk boundaries now retain original function positions when native
  overrides omit bodies; stable sorted declarations avoid unrelated C rebuilds.
  `tools/test_incremental_lift.py` and diagnostic lifter test pass.
- Audio agent completed a native Xbox ADPCM decoder/lazy buffer-voice backend,
  tested 252 idle buffers and bounded64 playing voices; patch/docs handoff pending.
  It will next work on exact title-specific audio bridges in src/audio_bridge.c.
- Computer-use skill read; node_repl has `sky` imported. Use that skill/API to
  inspect the actual game window once rendering advances. No UI screenshot yet.

## Latest handoff details
- boot-10-surfaces.log breaks at FEF20: color[1E3CC8]=019E1030 from our GetBackBuffer,
  depth[1E3CCC]=0 and fallback[1E8D60]=0. Color header six DWORDs:010D0001,019E1080,
  0,00011221,271DF27F,0. Singleton+2070=color,+2074=0,+207C=color,+2080=0.
- Runtime agent finished and frozen. Graphics agent active on FEF20/FF830 in
  src/graphics.c/docs. Audio agent active on new src/audio_bridge.c, only verified
  interfaces, explicit errors for unsupported effects/streams/spatial features.
- Native backend/worker/incremental-lift checkpoint ad1a6db; prior game-main/native
  graphics checkpoint ee6b9e9. Current graphics changes await next checkpoint.
- All five tracked toolkit patches pass idempotent bootstrap (including new
  graphics shader patch). Last native build/boot10 complete, no game left running.
