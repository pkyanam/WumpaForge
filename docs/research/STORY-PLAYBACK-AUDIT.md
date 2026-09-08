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
