# Title-specific DirectSound integration — 2026-09-08

The ABI findings below began as a read-only investigation. Native SDL/CoreAudio
output, PCM16 and Xbox ADPCM buffers now pass focused tests; see [ADPCM.md](ADPCM.md).
The backend supports 252 idle buffer objects with a separate 64-active-voice budget.
Streams return an explicit unsupported error. The title-specific guest bridge is
now included in the native target and manual dispatch: it marshals guest
descriptors, objects and calling conventions into the native DirectSound API.
No in-game sound has been verified.

## Confirmed public functions

Fixed-byte signatures were read from
[XbSymbolDatabase 20eced544726f5558c5a408458f38a086cc4e543](https://github.com/Cxbx-Reloaded/XbSymbolDatabase/tree/20eced544726f5558c5a408458f38a086cc4e543).
The database's
[DSound registration](https://github.com/Cxbx-Reloaded/XbSymbolDatabase/blob/20eced544726f5558c5a408458f38a086cc4e543/src/OOVPADatabase/DSound_OOVPA.c#L843)
selects 4134 signatures for these 4361 SDK routines. Candidate bytes were checked
against local XBE function starts, then inspected alongside actual callers and
callee cross-references. Ambiguous sparse wrappers were not accepted by name
alone.

| Public function | Address | Guest ABI and evidence |
| --- | --- | --- |
| DirectSoundCreate | `0x00137A06` | stdcall `(guid, ppDevice, unused)`, HRESULT, `ret 12` at `0x137A4A`; unique 4134 signature (9 bytes); game call `0xB99DC` |
| DirectSoundCreateBuffer | `0x00137A4D` | stdcall `(desc, ppBuffer)`, HRESULT, `ret 8` at `0x137AA1`; wrapper call `0x137A7B` targets unique CDirectSound_CreateSoundBuffer `0x1377B9`; game call `0xB8D57` |
| DirectSoundCreateStream | `0x00137AA4` | stdcall `(desc, ppStream)`, HRESULT, `ret 8` at `0x137AF8`; wrapper call `0x137AD2` targets unique CDirectSound_CreateSoundStream `0x13757A`; game call `0x5D8E2` |
| DirectSoundDoWork | `0x00136702` | stdcall `()`, void, `ret` at `0x13672A`; unique 4134 signature (11 bytes), calls `0x135D69`; game call `0xB9203` |

The CreateBuffer/CreateStream sparse signatures each initially match **both**
wrappers. Their callee targets resolve the identity: `0x1377B9` creates a buffer,
`0x13757A` creates a stream. Both internal methods are stdcall
`(this, desc, ppObject, unused)`, `ret 16`; their 14 fixed-byte signatures are
unique. The game uses the two-argument public constructors, so public replacements
can avoid the internal hardware setup without binding both internal methods.

## Actual startup sequence and required behavior

Game sound initialization is `sub_000B99D0` (`0xB99D0–0xB9B93`). It:

1. Calls DirectSoundCreate with a NULL GUID/outer object and a real output slot.
   A negative HRESULT takes the initializer's failure path.
2. Loads `sfx\\dsstdfx.bin` (literal at guest `0x16521C`) and calls
   IDirectSound_DownloadEffectsImage `0x136605` at `0xB9A3B`. This five-argument
   stdcall wrapper adjusts the public self pointer by -8 and calls uniquely
   identified CDirectSound_DownloadEffectsImage `0x135D05`; it returns with
   `ret 20`. Arguments are `(device, image, imageBytes, locations, ppImageDesc)`.
   The output descriptor is saved at game sound-manager+8. Its return is not
   checked here, but that does **not** establish that a NULL/invalid descriptor
   is safe for all later users.
3. Calls DirectSoundUseFullHRTF `0x135C14` at `0xB9A4F` (void, no arguments).
   Full/light public patterns overlap, but its call at `0x135C26` targets
   `CHrtfSource_SetAlgorithm_FullHrtf` `0x135B47`, a unique 4242 match. This
   selects original DSP/HRTF function pointers; it is not native spatial audio.
4. Builds a WAVEFORMATEX with **tag `0x69` (Xbox ADPCM), mono, 22050 Hz,
   average byte rate 12403, block alignment 36, 4 bits/sample, cbSize 2,
   samples/block 64**. The field stores are at `0xB9A59–0xB9A8A`.
5. Creates **252 empty buffer objects**: 64 with flags `0x40010` (LOCDEFER +
   CTRL3D), then 188 with `0x40000` (LOCDEFER). Both loops call game helper
   `0xB8D20`, which constructs an actual 24-byte DSBUFFERDESC, zero buffer bytes,
   the supplied format pointer, and calls DirectSoundCreateBuffer.
6. For each 3D buffer, the helper calls SetMinDistance `0x136D61` with 3.0/deferred,
   then SetMaxDistance `0x136D3D` with 50.0/immediate. Both are stdcall
   `(buffer, float, apply)` / `ret 12`; each wrapper's call reaches its respective
   uniquely resolved CDirectSoundBuffer/Voice min/max chain.
7. Starts a guest worker with entry `0x5DA30`. The optional listener branch at
   `0xB9B0A` calls IDirectSound_SetI3DL2Listener `0x13753A` (target `0x137066`) and
   IDirectSound_SetRolloffFactor `0x137516` (target `0x136EFC`), each stdcall with
   three arguments / `ret 12`.

These are the immediate initialization boundaries. Replacing only
DirectSoundCreate would leave public wrappers subtracting -8/-0x1C from a host
pointer or opaque token and entering original hardware code. Every substituted
object needs a guest-resident 32-bit representation and matching method bridges;
never store a native 64-bit pointer into a game output slot.

The first useful implementation should create cheap buffer metadata for those
252 objects and allocate a bounded mixer voice only when a buffer plays. The
existing 64-voice limit is an active-voice budget; it cannot be reserved once per
created object. Decode Xbox ADPCM for real playback and map compressed byte
positions/regions to PCM frames deliberately. Avoid decoding or retaining the
whole disc; the game supplies buffer contents later through SetBufferData
(`0x13755A`, game helper `0xB8DA0`; 3-argument stdcall / `ret 12`).

## Initial implementation audit (superseded for buffers)

Before the changes in [ADPCM.md](ADPCM.md),
`third_party/xboxrecomp/src/audio/dsound_device.c` provided:

- `xbox_DirectSoundCreate`, returning a host singleton device. It does **not**
  initialize `mcpx_apu_init_standalone`; an integration must create the actual
  APU/mixer producer once and shut it down after users stop.
- Buffer data copying plus actual 16-bit PCM mixer Play/Stop/current-position,
  state, volume and frequency support. This portion is a credible reusable base
  for native PCM output through the now-working SDL backend.
- Eager `apu_mixer_alloc_voice()` in every CreateSoundBuffer, with success still
  returned when allocation fails. This would consume all 64 slots before the
  title finishes its 252-object initialization.
- Mixer registration in SetBufferData **only when bits_per_sample == 16**.
  The title's 4-bit ADPCM would therefore produce no audio. The stored format
  does not preserve/validate wFormatTag, and success does not prove playback.
- CreateSoundStream writes NULL to the result and returns S_OK. This is unusable:
  the game dereferences the resulting stream's vtable in its setup path.
- SetPlayRegion and 3D/listener/mixbin methods that report S_OK while doing no
  audio work. These need actual retained state/behavior or an explicit unsupported
  result; their current success cannot be treated as completed compatibility.

The APU has an existing ADPCM block decoder in `src/apu/apu_state.h`, used by VP
in `apu_vp.c`. It can inform a bounded decoder implementation, but test it first:
its helper produces 65 samples from a mono 36-byte block while the VP consumes 64
and `ADPCM_SAMPLES_PER_BLOCK` carries a TODO. Preserve the title's declared
64-sample Xbox framing rather than assuming ordinary IMA WAV block semantics.

## Guest descriptors and streaming

Cross-check layouts against
[Cxbx XbDSoundTypes.h](https://github.com/Cxbx-Reloaded/Cxbx-Reloaded-legacy/blob/96aabe72e238674a22a61695fb6f5295259edf62/src/core/hle/DSOUND/XbDSoundTypes.h)
and the local caller stores. All pointers/handles below are **32-bit guest VAs**.

| Structure | Required guest layout |
| --- | --- |
| DSBUFFERDESC | 24 bytes: size+0, flags+4, bufferBytes+8, formatPtr+12, mixBins+16, inputMixBin+20 |
| DSSTREAMDESC | 24 bytes: flags+0, maxPackets+4, formatPtr+8, callback+12, context+16, mixBins+20 |
| WAVEFORMATEX | tag WORD+0, channels WORD+2, rate DWORD+4, average DWORD+8, block WORD+12, bits WORD+14, cbSize WORD+16; this ADPCM format adds samples/block WORD+18 |
| XMEDIAPACKET | 24 bytes: buffer+0, maxBytes+4, completedBytesPtr+8, statusPtr+12, event/context+16, timestampPtr+20 |
| XMEDIAINFO | 16 bytes: flags+0, inputSize+4, outputSize+8, maxLookahead+12 |

The local native DSBUFFERDESC inserts a `dwReserved` at +12 and stores pointers
at host width. A raw memcpy/cast is wrong even before ARM64 pointer size enters
the picture. Marshal named fields to a separate native descriptor/format.

Game streaming setup `0x5D820` calls XWaveFileCreateMediaObject `0x13672B`
(stdcall `(filename, ppFormat, ppMediaObject)`, `ret 12`) and explicitly checks
format tag `0x69`. It then creates an output stream with three attached packets,
a NULL callback/context and the returned format. When its debug flag is set,
it immediately calls GetInfo and checks output stream flags == 5, output size ==
0, and that the input granularity divides `0x9000`. It also checks the paired file
media object's flags == 1 and compatible chunk size. Success with NULL or zero
input granularity cannot satisfy this code.

The stream constructor installs the seven-entry vtable at guest `0x16B70C`:

| Slot/offset | Function address | Method / guest stdcall arguments |
| --- | --- | --- |
| 0 / +0 | `0x136240` | AddRef(stream), ret 4 |
| 1 / +4 | `0x136287` | Release(stream), ret 4 |
| 2 / +8 | `0x1362D5` | GetInfo(stream, info), ret 8 |
| 3 / +12 | `0x1363D6` | GetStatus(stream, status), ret 8 |
| 4 / +16 | `0x136427` | Process(stream, inputPacket, outputPacket), ret 12 |
| 5 / +20 | `0x13633C` | Discontinuity(stream), ret 4 |
| 6 / +24 | `0x136389` | Flush(stream), ret 4 |

A native stream bridge must own a bounded packet queue, report pending/completed
sizes/status when data actually advances, and release or flush it consistently.
It cannot mark every packet complete without playing it. The file media object
may remain recompiled if its existing kernel file calls work; it is separate
from the missing output-stream engine. This task did not audit all file-media
methods or implement streaming.

## Recommended first bindings and validation

For initialization, start with public device creation, effect-image handling,
HRTF selection, buffer creation/min/max properties and the optional listener
methods listed above. Keep effects/HRTF limitations explicit: the current native
pipeline bypasses DSP effects. Add actual buffer data/Play/Stop/status/position
bridges before claiming title sound, then implement the three-packet ADPCM stream
path when the title reaches music/stream setup. `DirectSoundDoWork` should service
real pending native completions as needed, not simply report a completed engine.

Use exact `sub_*` exports plus manual lookup for bindings: the current generated
dispatch table references excluded/manual symbols directly. Validate descriptor
packing, hundreds of idle objects without voice exhaustion, 64-sample ADPCM
framing, real playback cursor advancement, bounded packet completion, and object
shutdown with the existing lightweight audio tests. `src/audio_bridge.c` is
independently tested as described below and included in the game target/dispatch.

Local evidence: `local/reports/scan_audio_signatures.py`,
`audio-signature-matches.json`, `audio-signatures-xref-resolved.json`, downloaded
`xbsdb/DSound/*.inl`, `cxbx-XbDSoundTypes.h`, and the original generated `.text` /
DSOUND disassemblies. The refined signature report still contains candidates for
ambiguous wrappers whose callee signature was unavailable: use the confirmed
addresses above, not every report entry as a binding.

## Native bridge handoff

`src/audio_bridge.c` exports `wrath_audio_lookup`, exact `sub_*` functions, and
`wrath_audio_shutdown`. Device creation starts the real SDL/APU PCM producer and
fails if no output backend becomes active. Buffer objects contain only 32-bit guest
addresses and native-side associations: 16-byte device metadata, 32-byte buffer
metadata, at most 512 live buffers, guest blocks reused after release. Native
pointers are never copied into guest memory. These are opaque public API objects,
not original internal Xbox SDK objects or COM vtables. Recompiled internal methods
must never receive them. All public calls below must be excluded and dispatched
together when constructors are installed.

A scan of direct call/jump targets in the original game `.text` found these audio
boundaries (the separate XWaveFileCreateMediaObject stays recompiled):

| Address | Operation | Stack argument bytes | Bridge behavior |
| --- | --- | --- | --- |
| 137A06 | device create | 12 | native device, APU initialization |
| 137A4D | buffer create | 8 | field-by-field descriptor/format translation |
| 135BE8 | device release | 4 | reference/lifecycle accounting |
| 135BFE | buffer release | 4 | release actual native buffer |
| 13755A | buffer data | 12 | checked guest range, native owned copy/decode |
| 136D21 | buffer format | 8 | transactional supported format/data revalidation and live source replacement |
| 136664 | buffer play | 16 | native playback |
| 136688 | buffer stop | 4 | native stop |
| 13662C | buffer volume | 8 | native millibel volume |
| 1366DC | buffer seek | 8 | compressed-byte to PCM-frame mapping |
| 1366A0 | buffer status | 8 | actual producer state |
| 1366BC | buffer cursors | 12 | actual producer cursor, serialized DWORD outputs |
| 136702 | DoWork | 0 | reap actual completed buffer voices |
| 136605 | effects image | 20 | E_NOTIMPL and NULL descriptor |
| 137AA4 | output stream create | 8 | native bounded PCM/ADPCM packet stream |
| 135C14 | full HRTF | 0 | void API, explicit unsupported diagnostic |
| 136648 | buffer pitch | 8 | checked absolute pitch-to-native-rate conversion |
| 136D3D | buffer max distance | 12 | E_NOTIMPL |
| 136D61 | buffer min distance | 12 | E_NOTIMPL |
| 136D85 | buffer position | 20 | E_NOTIMPL |
| 136CED | headphone HRTF | 8 | E_NOTIMPL |
| 136D09 | deferred spatial commit | 4 | E_NOTIMPL |
| 137497 | listener orientation | 32 | E_NOTIMPL |
| 1374E1 | listener position | 20 | E_NOTIMPL |
| 137516 | listener rolloff | 12 | E_NOTIMPL |
| 13753A | I3DL2 listener | 12 | E_NOTIMPL |
| 1366F8 | stream volume | 8 | checked native volume |
| 1366FD | stream pause | 8 | native pause/resume; other modes unsupported |

Additional ABI evidence: `136D21` adjusts the buffer by -0x1C and calls `136B8C`,
which reaches `136A17`/`1368D0` and the wave-format copy at `138428`. Game `B8DA0`
passes its sample's format at sample+8 immediately after SetBufferData succeeds.
The native extension `xbox_DirectSoundBufferSetFormat` now validates and reconfigures
owned data atomically, as described in the implementation update below. `1366F8` tail-jumps to `1365B3`, whose
callee `135E7A` is the independently identified volume setter. `1366FD` tail-jumps
to `13649E`; both terminal functions return with `ret 8`. The other argument widths
are explicit `ret` immediates in the local DSOUND disassembly.

Do not treat this handoff as safe completion of title sound. The initializer ignores
the failed effects HRESULT and saves the NULL descriptor; later descriptor consumers
still need auditing before broad gameplay. Real stream construction, output packet
completion and spatial behavior remain unfinished. No unsupported
native spatial stubs are called through this bridge, and no effects descriptors or
stream objects are fabricated. The void HRTF call can only log the unsupported request.

Root integration requires adding the new source, all 28 manual exclusions/dispatch
bindings, and calling `wrath_audio_shutdown` after guest workers have stopped and
before SDL/platform teardown. No root build/config/generated files were changed by
this subtask. A mutex serializes bridge lifetime/lookup operations across title workers.

Focused validation: `tools/test_audio_bridge.c` uses synthetic guest RAM and
an SDL dummy output. It passed native device/APU lifecycle, 252 serialized objects,
ADPCM playback/completion, stdcall stack cleanup, unchanged adjacent output DWORDs,
compressed cursor mapping, invalid input ranges, explicit effects/stream errors and
shutdown on 2026-09-08. `src/audio_bridge.c` separately passed a C11 compile with
`-Wall -Wextra -Werror`. No title audio calls or audible in-game output were tested.

Reproduce the bridge test after the native platform library exists:

```sh
clang -std=c11 -O1 -g -Ithird_party/xboxrecomp/src -Ithird_party/xboxrecomp/src/apu -Ithird_party/xboxrecomp/src/nv2a $(pkg-config --cflags sdl2) tools/test_audio_bridge.c third_party/xboxrecomp/src/audio/dsound_device.c third_party/xboxrecomp/src/audio/xbox_adpcm.c third_party/xboxrecomp/src/apu/apu_core.c third_party/xboxrecomp/src/apu/apu_vp.c third_party/xboxrecomp/src/apu/apu_dsp.c third_party/xboxrecomp/src/apu/apu_xaudio2.c build/native/third_party/xboxrecomp/src/platform/libplatform.a $(pkg-config --libs sdl2) -o build/test_audio_bridge
./build/test_audio_bridge
```

`config/audio-functions.json` is the sorted, exact 28-address exclusion list; each
address has a matching exported symbol in the bridge. These28 entries are merged
into `config/manual-functions.json`, preserving graphics/input/runtime entries.
The root CMake target includes the bridge and `recomp_lookup_manual` consults
`wrath_audio_lookup`. Regenerate the lift before building so excluded original
DSOUND bodies do not collide with the native exports. This integration adds no
new unsupported-success paths; effects/HRTF/spatial settings and packet streams
continue to emit bounded diagnostics and return explicit errors where applicable.

The first expected audio breakpoint is `sub_00137A06`: original sound initializer
`B99D0` calls it at `B99DC`, returning to `B99E1`. Device creation starts the native
PCM producer and SDL/CoreAudio output. It is followed by effects at `136605`, HRTF
at `135C14`, and252 buffer constructions through `137A4D`. This call sequence is
from the actual local game disassembly, not an observed audio boot yet. The parent
must invoke `wrath_audio_shutdown()` after guest workers stop and before kernel,
SDL or guest-memory teardown; the shutdown must not be deferred until after
`xbox_MemoryLayoutShutdown`, since it releases guest objects.

## Follow-up read-only audit: descriptor, SetFormat, pitch

The sound manager's concrete address is `0x00427A58`: the startup wrapper at
`0xEC960` loads that ECX value and tail-jumps to `0xB99D0`. The effect-image
output slot is therefore `0x00427A60`. No absolute read or address-taking reference
to `0x427A60` appears in the disassembled game `.text`. The sound-manager method
region `0xB9100–0xBA200` has only two identified manager+8 operations: the output
address at `0xB9A2D`, and constructor zeroing at `0xBA044`. Other +8 accesses in
that region are buffer-array/sample/vector/stack fields, as shown by their base
register initialization (e.g. `B9550` uses manager+0x7D20; `B98A0` iterates the
buffer arrays). The destructor beginning `0xBA070` does not consume this field.

This is evidence that the descriptor is stored but unused by the ordinary sound
manager, not a proof against every indirect alias/control path. No NULL descriptor
dereference was found. A hardware read watchpoint on native guest address
`g_xbox_mem_offset + 0x427A60` after initialization would settle actual title-path
usage once that stage runs. Keep E_NOTIMPL for effects; there is no need to invent
a descriptor just to fill the unused slot.

SetFormat `136D21` is further confirmed by its `138428` callee switching on format
WORD+0 and selecting PCM tag1, Xbox ADPCM tag0x69 or extensible tag0xFFFE. Game
`B8DA0` passes data/bytes from sample+0/+4, then format at sample+8. SetBufferData's
HRESULT is checked, but SetFormat's result at `B8DED` is ignored; the helper then
records sample ID at sample+0x1C and returns true. Thus explicit SetFormat failure
avoids false success in the API, but does not make a changed-format sample play
correctly. The real format values must be logged at that reached call or decoded
from the sample loader; only the initialization format (mono22050 ADPCM) is proven.

Pitch is **absolute hardware sample rate relative to 48 kHz**, not a multiplier
on the original buffer rate. The title's bundled SDK proves the conversion:
`135C3C` maps rate48000 to zero; otherwise it multiplies rate by the float at
`0x16B6CC` (1/48000), then `13A340` executes `4096 * log2(ratio)` using FYL2X and
integer rounding. The inverse for native rate is `48000 * 2^(pitch/4096)`.
`13AA11` clamps the hardware value to -32767..8191 before packing it into the
register's upper WORD; spatial/submix contributions are added before that clamp.

The game's sole buffer-pitch wrapper `B9020` passes negative inputs directly. For
nonnegative inputs p it computes `trunc(max(200-p, 0) * 0.01 * basePitch)`, with
basePitch=-4608 when its stored buffer rate is22050, otherwise -512. The float at
`0x15E3B0` is0.01; `7AF40` supplies the float-to-integer conversion. At p=100 this
therefore requests approximately22008 Hz for the22050 case. The bridge now uses that absolute inverse and rejects pitch values outside
-32767..8191. The full accepted interval maps to approximately188..191968 Hz,
within the backend's100..192000 Hz range. It does not multiply22050 by the same
ratio again.

No source changes were made during that follow-up audit. Subsequent format/pitch
work and target integration are recorded below; no audio call has yet been
observed during a real game boot.


## Implemented SetFormat and SetPitch

`136D21` now marshals the format to the native `xbox_DirectSoundBufferSetFormat`
extension; no host vtable order changes. The backend validates PCM16/Xbox ADPCM,
mono/stereo, rate/block/header constraints and current owned bytes before changing
anything. It reuses encoded storage. A rate-only change reuses decoded PCM; a
codec/channel change decodes replacement PCM first. Allocation failure, malformed
input, unsupported format, or an outstanding buffer Lock returns an error with the
old buffer/source/voice unchanged. The test injects an allocation failure to verify
this transaction rather than relying only on validation errors.

For a playing buffer, `apu_mixer_set_source` swaps the source under the mixer lock,
retaining its reserved slot, active/looping state and volume. Old decoded storage
is freed only after that swap. A successful change resets frequency/original-frequency
to the new format rate, as the bundled SDK does. Rate-only changes preserve the PCM
frame cursor; changed block layouts map the current encoded byte position to a whole
new block and reset to zero if beyond the new extent. No extra active voice is
reserved. The small interval between cursor snapshot and source swap can repeat a
few frames; streaming-quality seamless format changes are not claimed.

`136648` now computes `round(48000 * exp2(pitch/4096))` and calls the actual native
frequency method. It accepts signed pitch -32767..8191, checks the resulting native
rate and rejects out-of-range values with E_INVALIDARG. Checked examples: pitch0
→48000 Hz,4096→96000,-4096→24000,-4608→22008. The accepted endpoints round to188 and
191968 Hz. Live voices update through the mixer lock.

Tests now cover empty-buffer format changes; live mono PCM→stereo ADPCM→stereo PCM;
golden decoded samples; unchanged encoded allocation and voice slot; rate-only PCM
reuse; cursor conversion; original-frequency reset; locked/malformed/OOM rollback;
and pitch sign, endpoints, absolute-rate semantics and invalid-input preservation.
Both ordinary SDL-dummy tests and the memory-sanitized backend test passed. The
sanitized run uses the actual APU/mixer with a test-only output sink to avoid SDL's
pre-main dynamic-loader failure; it does not validate audible output. See ADPCM.md
for its command. No additional manual addresses are needed for these changes.


## Boot28/29 audio hold and native packet streams (2026-09-08)

The intro freeze is now tied to concrete original instructions. `2C330` holds
animation rate zero while `[1BAA2C]` is set and either the current cut has audio
and `AA590(4) != 1`, or intro zero still has byte `[56082E] == 2`. `AA590` tails
`EC770`, which reads channel4 index/kind at `427A10/427A14`. Kind2/index0 selects
stream wrapper `427A58 + A480 = 431ED8`. `5D4B0` returns1 when wrapper byte+24
says source feeding, or its real stream GetStatus has PLAYING10000 or PAUSED20000
without STARVED40000. It does not equate READY bit1 with playing.

Boot28 showed channel0/kind2, mode255, fade0, position300 and rate0. The WAV
reader's original `138552 CMP [EBP-4],45564157; JMP138567; JE13856E` was lifted
into an unassigned `_flags` condition at the join; this rejected a correct WAVE
header with E_FAIL. The runtime agent corrected join flag recovery. Root's boot29
then read fmt/data chunks and reached the expected former stream E_NOTIMPL at
return `5D8E7`. No animation hold or sound status was forced to advance.

The native bridge now implements the seven original stream vtable methods listed
above, plus the existing public volume/pause wrappers. Root must exclude all seven
addresses during lifting as well as include them in native dispatch. Guest object
word0 points to the unchanged original seven-entry vtable at16B70C. The backend
uses host objects; no host pointer is written to guest RAM.

Each stream lazily reserves one of64 mixer voices on first nonempty packet. It
accepts up to its declared packet count, capped64, and4MiB decoded queued PCM per
voice; current game streams declare3 packets. Packets are copied/decoded once,
then freed when consumed or flushed. The game's0x9000-byte mono/stereo ADPCM
packets each decode to128KiB, so three attached packets require384KiB per stream.
The render loop carries fractional sample position across packet boundaries,
without an artificial pause between packets. An empty queue produces silence;
only submitting real data restarts it. Status exposes actual queue/paused/EOS
state, and READY indicates remaining bounded queue capacity.

Completion runs from the native APU mixer after its source frames have been mixed.
It writes completed source bytes followed by SUCCESS status using release stores.
This advances at the existing bounded output queue's producer position, which can
lead audible output by that queue's latency. It is independent of guest DoWork.
Pending remains8000000A until consumption; Flush/final Release cancels remaining
packets with E_ABORT80004004, clears their completed size, and synchronizes with
the mixer before freeing objects. The title supplies status pointers and NULL
completed-size pointers. The cancellation completed-size convention is not verified
against Xbox hardware; exact partial byte reporting outside this title remains a
limitation. Cxbx's packet manager publishes full packet size on flush, so its
cancellation-size behavior differs and should not be silently claimed equivalent.

The title uses NULL callback/context/mixbins/event/timestamp. Nonzero callback,
event, timestamp, routing/spatial flags and synchronized pause modes remain explicit
errors. Pause0/1 preserves actual queued data and source position. Discontinuity
marks the current queue as ending, so draining it finishes rather than reporting
an unexpected starvation. Shutdown first cancels all stream packets while guest
RAM is still mapped. Guest workers must be joined before teardown; a callback only
writes completion fields and frees its host context, and never runs guest code or
re-enters the mixer.

Validation: `tools/test_audio_stream.c` drives the exact public guest ABI and
production mixer deterministically. It verifies three consecutive PCM packets,
ADPCM output against the existing synthetic vgmstream golden fixture, fractional
22050Hz boundaries, queue rejection, pending/success byte counts, pause/resume,
starvation/EOS, flush and ref lifetimes. It passed with UBSan. ASan could not run
through this installed SDL2-compat/SDL3 loader (startup modal error, test process
terminated), so no ASan result is claimed. `tools/test_audio_bridge.c` additionally
passed its252-buffer regression and a real SDL dummy/APU producer stream-completion
test without polling DoWork. No in-game stream playback or audible game sound has
yet been verified at this checkpoint.

Root integration validation: full build30 and bootstrap30 passed. Boot30 reads
real ADPCM source-file packets, clears the original audio gate, and advances animation
300→315.5→347.5 at increment0.5. Actual GPU frame180 shows the original copyright
screen over stars. Execution later stops at uncompiled callback31E20, not at
stream creation. Audible output quality remains unverified; LLDB-heavy run logs
CoreAudio overload messages, so smooth playback needs a later undisturbed run.

Reproduce the deterministic test with the existing platform library:

```sh
clang -std=c11 -O1 -g -fsanitize=undefined -Ithird_party/xboxrecomp/src -Ithird_party/xboxrecomp/src/apu -Ithird_party/xboxrecomp/src/nv2a $(pkg-config --cflags sdl2) tools/test_audio_stream.c third_party/xboxrecomp/src/audio/dsound_device.c third_party/xboxrecomp/src/audio/xbox_adpcm.c third_party/xboxrecomp/src/apu/apu_vp.c third_party/xboxrecomp/src/apu/apu_dsp.c third_party/xboxrecomp/src/apu/apu_xaudio2.c build/native/third_party/xboxrecomp/src/platform/libplatform.a $(pkg-config --libs sdl2) -o build/test_audio_stream
./build/test_audio_stream
```

API constants/layouts were cross-checked with the
[Cxbx stream types](https://github.com/Cxbx-Reloaded/Cxbx-Reloaded/blob/master/src/core/hle/DSOUND/XbDSoundTypes.h),
[stream methods](https://github.com/Cxbx-Reloaded/Cxbx-Reloaded/blob/master/src/core/hle/DSOUND/DirectSound/DirectSoundStream.cpp)
and [packet manager](https://github.com/Cxbx-Reloaded/Cxbx-Reloaded/blob/master/src/core/hle/DSOUND/DirectSound/DSStream_PacketManager.cpp),
read2026-09-08; implementation is original code following the retail4361 ABI,
not a source copy of their GPL implementation. The existing ADPCM reference
revision/golden-generation provenance remains in ADPCM.md.


## Silence diagnosis and contiguous packet buffers (boots36–37)

The user's report of no audible intro sound was correct. Animation advancement
was not proof of accepted audio: the original wrapper's source-feeding flag can
clear the intro gate even when Process rejects every packet. Boot36 with
`WRATH_TRACE_AUDIO=1` proved stream creation and volume0 succeeded, while every
`136427 Process` returned E_INVALIDARG80070057 before decoding. CoreAudio's
default route was MacBook Air Speakers, mute0, volume1.0,48000Hz; system settings
were read, not changed.

Boot37 stopped at the actual call from `5D720` and captured packet `172EF70`:
`{80000000, 36864, 0, 00431EE4, 0, 0}`. The original `5D820` initialization
allocates110592 bytes through MmAllocateContiguousMemoryEx. The bridge incorrectly
validated only low RAM. Runtime `xbox_ContiguousAlloc` maps this high virtual
window separately; it must not be masked into low RAM. The corrected validator
accepts addresses within the currently allocated extent returned by
`xbox_ContiguousAllocatedBytes`, capped by the64MiB contiguous window, with
subtraction-based end bounds. Address80000000 is valid even though its physical
offset is zero. Unallocated high addresses and crossing the extent remain errors.

The deterministic UBSan test now reserves sparse virtual memory with separate
low RAM and high contiguous storage. A real guest32 packet at80000000 produces
all192 golden ADPCM samples exactly while low RAM contains different bytes.
Boundary-crossing, one-past-end, null/low invalid and wrapping addresses fail.
Only4MiB low memory and16KiB high memory are accessible; the2GiB gap remains
PROT_NONE with no committed physical memory. The SDL dummy producer lifecycle
regression also passes. Actual game output after this fix still needs a run.

Optional `WRATH_TRACE_AUDIO=1` diagnostics provide bounded packet fields and
ABI results, decoded PCM nonzero/peak counts, and approximately once-per-second
mixer submission/SDL callback energy counters. Callback instrumentation only
accumulates counts under the existing device lock; it performs no logging or
allocation. Nonzero callback samples establish that PCM reached the native output
callback, not that a user heard it or that timing/quality is correct.

## Story18 hub music and overlap/lifetime coverage

`local/reports/story-18-audio.json` captured the main thread in the original GPU
fence path. Wrapper1's active sound170 object `02D7F5F0` matches the native stream
registry, contains the correct16B70C vtable and GetStatus1363D6 method, and has
three pending packets. Wrapper0's old story guest allocation remains registered
for reuse with its native pointer zero; that is the bridge's intended released
state, not evidence of a live dangling sound object. No probe fields failed.

The log's real output callback continues to receive nonzero samples through the
rendering stall. At the last output report, submitted16267520/callback16266496
frames leave1024 queued frames, with zero overflows. There are40 cumulative
underrun callbacks; increases occurred at several earlier points, with the last
at submitted time274.736s. The count remains40 through338.907s. These counters
show occasional sink starvation during this long diagnostic run; they do not
identify its cause or establish an audio deadlock at the GPU fence. Peak32768
alone cannot distinguish loud source audio from additive clipping. There is no
new evidence justifying a runtime audio change.

The existing stream test covered one stream at a time. A new bounded synthetic
case in `tools/test_audio_stream.c` now uses two simultaneous packet streams and
a separate one-shot buffer through the actual guest bridge/native mixer. It
checks exact stereo sums, the SFX ending while music continues, release of one
stream while another remains pending, cancellation of its old packet, reuse of
its guest stream slot with the correct vtable, replacement-source samples with
no old tail, and final silence/inactive voices after all releases. A real APU
lock/condition is initialized for buffer Play, while the producer is absent and
only explicit mixer calls advance time. This isolates sample/lifetime behavior;
it does not replace the existing asynchronous producer test or claim a completed
in-game story-to-hub-to-level transition.

The documented clang command above passed under UndefinedBehaviorSanitizer with
one compiler job. Result: `local/reports/audio-transition-test.log`. It emits no
audio and uses no game assets. No implementation source changed.
