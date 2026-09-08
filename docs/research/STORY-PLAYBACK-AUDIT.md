# Original Xbox backstory path

Audited 2026-09-08 against the supplied USA XBE, XDK4361; no implementation
changes, game launches, or substitute movie. Evidence comes from original data
and `local/reports/disasm/asm/text.asm`, not inferred GameCube equivalence.
All extracted files already match their original ISO extents by SHA-256; see
[asset audit](../ASSET-AUDIT.md). The current story remains unverified on screen.

## Selector and asset sequence

`LoadCutMovie` is original `0x2C020`. Its switch table at `0x2C2A4` contains
`2C0C8, 2C14C, 2C1F6, 2C190, 2C190`. Thus **movie1 loads the combined story**;
movie2 does not load another half. The recovered GC `cut.c` has a different
movie1/movie2 split and must not be used to change Xbox control flow.

Movie1 at `2C14C` loads `ats/splash/ripples.ats`, copies
`levels/b/intro/introchars.dat` into the archive-name buffer (`8F4E00`), sets
`1BAA84=1`, and calls `LoadCutComponents` (`2BD80`) with EAX=`17BBF8` and the
character table `17BAA8` on the stack. That table contains 27 character entries,
including Cortex, Dingodile, N.Gin, N.Tropy, Tiny, Uka Uka, the hologram, Crash,
Coco, Aku Aku, masks, and scene props. Original character loading uses the DAT
archive; no new extraction or synthesized assets are needed.

Each scene descriptor is four 32-bit words: scene-name pointer, cut-name pointer,
sound ID, debris group. The table at `17BBF8` contains:

| Index | World scene (under levels/b/) | Cut animation (under levels/b/) | Sound ID |
|---|---|---|---|
| 0 | intro/space.nus | intro2/station.cut | 173 |
| 1 | intro/corridor.nus | intro2/corridor.cut | -1 |
| 2 | intro/chamber.nus | intro/inchambr.cut | -1 |
| 3 | intro/volcano.nus | intro2/surf.cut | 174 |
| 4 | intro/volcano.nus | intro2/sunny.cut | -1 |
| 5 | intro/volcano.nus | intro2/storm.cut | -1 |
| 6 | intro2/blackbox.nus | intro2/black.cut | -1 |
| 7 | intro/volcano.nus | intro2/washedup.cut | -1 |
| 8 | intro/elemen.nus | intro2/mystic.cut | -1 |
| 9 | intro/house.nus | intro2/house.cut | -1 |
| 10 | intro/control.nus | intro2/control.cut | -1 |

Debris groups are 1..11. The next descriptor is the null terminator. Original
`A9F60` probes the Xbox `.nux` version of these `.nus` scene names first; the
listed world scenes all have corresponding extracted `.nux` files. Preserve that
original choice and case-insensitive path resolution.

`2BD80` loads/fixes each world and animation, creates each instance, sets rate0.5,
assigns its audio ID, and links the preceding instance to the next. The final
callback is `2BC00`; intermediate callback `2BC40` increments the scene index.
`LoadCutMovie` waits for the original loading thread `2BF30` to finish before
returning `CutInst[0] != NULL`. A reached loader is not evidence of playback.

## Audio evidence and compatibility

`StartCutMovie` (`2C480`) starts the first instance and reads its sound ID. For
nonzero movies, `2C59F` calls `B50A0(173, NULL)` after setting selected channel
`1AA41C=4`. The original 48-byte sound descriptor table begins at `1A42F0`:

- ID173, descriptor `1A6360`, filename pointer at +24 to `1645E8`:
  `VCE_ENG/INTRO1` (resolved beneath `Crashdat/sfx`, with `.wav`).
- ID174, descriptor `1A6390`, filename pointer at +24 to `1645D8`:
  `VCE_ENG/INTRO2`.
- Audible title-logo ID172 uses adjacent descriptor `1A6330`, `VCE_ENG/LOGOS`.

Both original English story WAVs have format tag105 (`0x69`, Xbox ADPCM), mono,
44100Hz, average24806bytes/s, block36bytes,4bits/sample, extension2bytes and
64samples/block. INTRO1 data size3424392bytes; INTRO2 size5654160bytes. This is the
already supported native packet/decode/mix path used by the title logo, not a new
codec. These headers alone do not establish successful story audio submission or
that sound has been heard. Nominal decoded sample durations are approximately
138.046s and227.933s, so complete original backstory playback exceeds a180s diagnostic.

The original `.cut` header float at +8 (the duration field read by original
2C4EB) gives scene lengths110,200,3818,438,501,211,58,471,2181,1766,1195.
At rate0.5 per60Hz update, their sum10949 is nominally364.967s; the first
three scenes are137.6s and the remaining eight227.367s. Scene boundary, audio
hold, fade, loading and menu interaction can add wall time. Allow at least480s
for a bounded full-story attempt; treat it as a diagnostic limit, not an exact
completion deadline. The first station scene is nominally3.67s and corridor6.67s,
then the chamber scene127.27s, so a late capture will naturally show the chamber.

At scene3, callback `2BC40` stops channel4, services sound while its original key
status remains1 (`2BC61..2BC73`), then calls `B50A0(174,NULL)` at `2BC8E` and sets
audio hold. Other scene IDs are−1: they retain the existing stream, not stop it.
Normal native stream flushing, release, recreation, packet completion and high
contiguous-memory source addresses must remain functional across this transition.
No fake ready/completed values are appropriate. Backend constraints and verified
packet ABI are in [AUDIO-INTEGRATION.md](../AUDIO-INTEGRATION.md).

## States, breakpoints and verification

| Original address | Meaning |
|---|---|
| 17BE64 | signed current movie selector; story=1 |
| 8F4DB4 | signed next movie; PlayCutMovie resets it to−1 |
| 8F4DFC | current scene index0..10 |
| 1BA928 + index*4 | cut-instance pointer; +6C flags, +70 float position, +74 float rate |
| 1BA7A8 + index*4 | cutscene asset pointer |
| 23C2E0 + index*4 | world-scene pointer |
| 8F4E80 + index*4 | signed sound ID |
| 1BAA2C | audio hold gate; not proof of audible output |
| 942064 | cut_on, cleared by final nonzero-movie callback2BC35 |
| 1A097C / 23C40C | fade / signed fade rate |
| 1BAA8C / 1BAA90 | loading-thread handle / finished request |
| 427A10 / 427A14 | logical channel4 backing index / kind (2=stream) |
| 431ED8 + backing_index*44 | original stream wrapper, +24 feeding flag |

Use original `sub_0002C020` to observe loading, `sub_0002C480` for loaded instances
before starting, `sub_0002BC40` before scene transitions and `sub_0002BC00` before
completion. `sub_0002C330` is the animation update, but avoid expensive repeated
per-frame debugger stops. `tools/story_probe.py` exposes:

```text
command script import /Users/preetham/Code/wrath-of-cortex-decomp/tools/story_probe.py
script story_probe.install(lldb.debugger)
```

Install before the parent's existing bounded run. It adds only four boundary
breakpoints, automatically continues and logs only movie1. Callback snapshots
are **before** each named function executes. At any subsequent debugger stop:

```text
script story_probe.dump(lldb.debugger)
```

Or pass an ignored local report path as its second argument. This tool only reads
bounded guest memory; it calls no guest functions, writes no guest state, and
changes no selector or completion gate. It was syntax-checked, not yet validated
inside a story run. It does not own or alter `shader_probe.py`.

Verify movie1, all eleven nonzero instances after loading, then a visible station
scene and increasing animation position with fade0/rate0.5. Correlate original
sound173 with successful native stream packets and nonzero sink output; use actual
hearing to confirm audibility. Later scene3/sound174 and final scene10 completion
are separate milestones. Do not infer the entire story works from its first frame.

## Original skip controls

In `PlayCutMovie` (`2D950`), `2DB67` tests the original controller rising-edge word
at `[pad+0xD0]` with mask`0x840` (A or Start). If pressed for a nonzero movie,
`2DB73` sets the loop's quit flag and `2DB7A` writes next_movie−1. Preserve this
ordinary user path: Space/A and Enter/Start per [controls](../CONTROLS.md). No
forced selector change, automatic skip, or shortened asset should be introduced.

## Story04 desynchronization evidence and next measurement

The user heard station audio and saw that scene, then reported desynchronization
before the scene2 shader failure. Story04's boundary snapshots support an audio
lead already at scene0 end. Original stream wrapper+`1C` is the cumulative loaded
byte count (`5D606..5D610`); the three status words at+`0C` are all pending.
At station end, loaded331776bytes =9 packets; at least6 packets have therefore
finished mixing. Each36864-byte mono ADPCM packet decodes65536frames. This is
at least8.916s mixed source versus animation110/30=3.667s, a lead of at least5.25s.
At corridor end, loaded516096bytes=14 packets, with3pending, gives at least16.347s
mixed versus (110+200)/30=10.333s, lead at least6.01s. The SDL ring can delay the
mixed signal by at most64ms plus the audio-device pipeline. This is preliminary
boundary evidence, not a measured render-frame cause; story04 had no profiler.

For story05, enable `WRATH_PROFILE=1 WRATH_TRACE_AUDIO=1` and the sparse story
probe. The probe now reads swap/vblank counters, Python host monotonic time,
actual SDL submitted/played/queued/underrun counters, and the backing native
stream's voice cursor and bounded packet-queue metadata. When all three original
slots remain pending and the supported non-looped story stream is matched, it
computes mixed-source seconds from loaded ADPCM frames minus native queued frames
plus the head packet's16.16 source cursor. No guest/audio functions are invoked.
Metadata unavailable in the debug image is reported explicitly. Native callback
counts include silence; source cursor is therefore the story-specific clock.

Compare changes in mixed-source seconds, SDL callbackframes/48000, vblanks/60,
and cumulative scene animationframes/30 at matched boundaries. Pair the existing
per60-present profile lines by swaps/vblank. Sparse debugger stops affect timing;
separate any stop-related discontinuity from sustained render slowness. No audio
implementation adjustment is justified solely by the earlier codec/header audit.

### Story05 measured result

`local/reports/story-05.log` enables both existing profilers. Station scene0
profile frame3504→3684 reports animation9→99 (3.000 nominal seconds), while
vblank3861→4370 advances509/60=8.483 seconds: **5.483 seconds of additional
animation lag** across those180 rendered frames. The first3504 report straddles
loading, so its aggregate15.79FPS is not a pure station measurement. Subsequent
full station windows are20.24,22.49,21.06FPS, with38.2–42.5ms/frame attributed
to texture upload work. `audio_gate=0` and step0.5 throughout these windows.

Contemporaneous AUDIO-OUTPUT lines show nonzero output,512–1024 queued stereo
frames (10.7–21.3ms), underruns unchanged at2 and overflows0. The three pending
original packet slots and loaded-byte totals reproduce story04's lower bounds.
The corridor subsequently runs58.94–60.00FPS; this slows further divergence but
does not erase the preceding station offset. The combined evidence identifies
render throughput as a concrete cause of this audio lead. It does not support
changing audio sample rate, codec or completion status to hide the slowdown.

Story05 had imported the preceding probe version before the native counter
extension was written. Its boundary snapshots therefore lack an exact simultaneous
native source cursor; do not claim that enhanced measurement already ran. Commit
7a67c77 prepares that check for a later run or explicit debugger module reload.

### Story07: native audio and animation clocks now advance together

Story06 followed the attract demo and did not measure movie1. Story07 successfully
entered movie1 on build49 with palette invalidation and vertex-stream fixes.
The enhanced probe obtained real native queue/cursor values at both boundaries:

| Boundary | Host monotonic seconds | Vblank | SDL frames played | Source seconds mixed |
|---|---:|---:|---:|---:|
| Before StartCutMovie | 54.584527 | 3055 | 2215424 | no story stream yet |
| Station end | 58.237685 | 3274 | 2391040 | 3.514655 |
| Corridor end | 64.870954 | 3672 | 2709504 | 10.143966 |

Station start→end took3.653158 wall seconds,219vblanks (3.650s). Full station
profile windows run60FPS, with0 uploads and4.73–4.77ms/frame work. The first
aggregate18.30FPS report still includes loading and is not the station rate.

Between station and corridor boundaries, wall advances6.633270s, vblank6.633333s,
SDL playback6.634667s and the actual source cursor6.629311s. The independent
clocks agree within4ms across this interval; the previous multi-second drift is
absent. Output underruns and overflows remain0, stream volume1, rate44100Hz,
active/unpaused, three queued packets. SDL ring depth is1024frames at station
end and768 at corridor end (21.3ms and16ms).

Absolute mixed-source positions trail the simple nominal animation sums110/30
and310/30 by152ms and189ms respectively. This includes initial audio/animation
start scheduling and scene-boundary frame conventions; it is not yet a lip-sync
assessment. Ring/device latency is additional. Do not describe this as verified
perfect synchronization. The measured result establishes correct ongoing clock
rate in the first two scenes and removal of the earlier large render-induced lead.

The run then aborts at original state method1026F0(value1), scene2 animation1.5.
At the stop, native source10.223966s is still active, with0 underruns/overflows.
No audio failure or unsupported format was reached. Chamber character playback,
remaining scene timings, transition to sound174, and full completion still need
an actual run after the graphics state boundary is implemented.

### Initial offset audit: original frame origin and feeder scheduling

The simple header-length/30 comparison above overstates the residual offset.
Original `79EC4` initializes each instance's position to **1.0**, not0.0.
`7A9F3..7A9FF` adds rate0.5 per active update, and `7AA17..7AA4A` invokes the
end callback when position reaches the cut-header length. At the next-scene
handoff, `7AA92` starts the next instance (again1.0), then `7AA97..7AAA7` adds
any carried excess. The first active update also has a flag-controlled activation
pass (`7A9DF..7A9E1 → 7AB1F`) before normal incrementing. These are original
frame-number conventions, not playback seconds starting at frame0.

Using content position measured from frame1, station end is109/30=3.633333s;
station+corridor end is(109+199)/30=10.266667s. Their differences from the measured
source cursors are **118.678ms and122.700ms**. The supposed37ms offset growth
mostly came from counting an extra frame at the scene boundary; the actual
interval discrepancy is4.02ms. Including the first activation tick gives expected
boundary update counts219 and219+398, matching measured vblanks exactly; the
corresponding source lags are135.345ms and139.367ms. Neither comparison supports
a persistent audio-rate error.

There is also an explicit original **100ms startup gap** between stream readiness
and its first data submission:

1. `2C59F` requests sound173 on channel4; `2C5A7` sets the audio hold gate.
2. Original worker `5DA9D` calls stream setup `5D820`. Setup creates the media and
   DirectSound stream, marks wrapper+24 feeding at `5D9C9`, sets volume via5D670,
   then returns at5DA24. It does **not** call the packet feeder5D720.
3. Worker5DAA2 jumps to5DAFE, completes its six-stream scan, then pushes100 at
   `5DB1B` and calls the original Sleep wrapperECD37 at5DB1D.
4. On its next scan, feeding is set, so it takes5DAA7→5DADB and finally calls
   packet feeder5D720 at5DAF9. First native mixing follows real Process submission.
5. Meanwhile `2C348` checks channel4 through
   `AA590 → EC770 → B9370 → 5D4B0`. The original5D4B1..5D4B6 returns true directly
   from wrapper+24; it does not wait for the first packet to reach the hardware.
   Movie1 therefore clears hold at2C38A and resumes animation while the feeder
   is in that original100ms sleep.

The measured stable roughly119–123ms content/source difference is consistent
with this original100ms delay plus frame/worker/mixer scheduling; exact subframe
attribution was not captured. Native output adds the observed16–21ms ring queue
and hardware latency. The application's DirectSound-ready status cannot correct
this gate: the original feeding-byte fast path bypasses that status entirely.
Changing a bridge status, skipping Sleep, shifting animation frames or adjusting
sample rate would change original behavior without evidence of a compatibility
bug. No such correction was made.

The native44.1kHz→48kHz mixer increment floors a16.16 fraction; its effective
source rate is44099.8535Hz (3.32ppm low). That is approximately1.22ms over366s,
far below this startup difference. This is a precision bound, not a measurement
of final long-story sync or a justification to retime the original game.

Next useful validation is ordinary scene2 dialogue/lip movement and the
scene3 transition to sound174 after the graphics blocker is resolved. Existing
probe boundaries can show whether another similar one-time startup gap occurs.
An actual additional growing mismatch would justify further investigation;
current evidence supports retaining original timing and the native audio code.

### Story08: full original animation/audio sequence reaches completion

Build50's story08 reaches final original callback2BC00 at scene10 position1195.
Elapsed time from the before-StartCutMovie probe is365.153122s, with21884 swaps
and21896 reported vblanks. All eleven original scene boundaries are observed:

| Scene ending | Elapsed wall seconds | Mixed source seconds | Cumulative SDL underruns |
|---|---:|---:|---:|
| 0 station | 3.700811 | INTRO1 3.525322 | 1 |
| 1 corridor | 10.332879 | INTRO1 10.106633 | 8 |
| 2 chamber | 137.833779 | EOF; not estimated | 11 |
| 3 surf | 152.497486 | INTRO2 14.442619 | 13 |
| 4 sunny | 169.184038 | INTRO2 31.130563 | 13 |
| 5 storm | 176.200708 | INTRO2 38.149207 | 13 |
| 6 black | 178.101110 | INTRO2 40.047867 | 13 |
| 7 washed up | 193.766929 | INTRO2 55.711815 | 13 |
| 8 mystic | 266.433948 | INTRO2 128.303574 | 25 |
| 9 house | 325.355537 | INTRO2 187.151378 | 32 |
| 10 control | 365.153122 | EOF; not estimated | 33 |

The original stop/create/feed change from173 to174 succeeds; the second stream
uses native voice slot1, mono44100Hz, active/unpaused, volume1. Between scene3
and4 ends, source advances16.687945s while wall advances16.686552s. At scene8
end the frame1-origin content sum is128.466667s versus source128.303574s, a
163ms source lag; no multi-second drift appears in this longer run.

This run has33 cumulative SDL underrun callbacks and0 overflows. For example,
station→corridor wall6.632069s versus source6.581311s and callback6.576000s occurs
while underruns increase1→8, contributing approximately51–56ms of extra audio
lag. These brief gaps must not be described as flawless audio. Sparse debugger
stops, host scheduling and the very large repeated graphics-error log are present;
no matched measurement yet isolates their individual contribution. No audio
rate/queue/timestep change was made.

At EOF the original loaded-byte offset wraps modulo the data length and feeding
becomes0 (`5D620` onward), while remaining fixed-size native packets may still be
queued. The ordinary three-pending-packet source estimator intentionally omits
those endpoints. Final callback reach does not mean every padded file sample has
already played; original movie cleanup stops its channels normally.

Visual completeness is explicitly **not verified**: this run emits over100,000
stream1 range/allocation rejections and associated skipped indexed draws. The
next stop after the story sequence is shader_error for fixed pixel stage0
COLOROP4/ALPHAOP4 argument/result/binding. Root owns graphics correction and
actual frame validation. This result proves original story control flow and both
audio-stream lifecycles progressed to their intended final callback, without an
automatic skip or replacement movie.
