# Title-specific DirectSound integration — 2026-09-08

Investigation only: no bridge source changes or game launches were made for this
pass. The native SDL/CoreAudio output and APU software mixer work in focused
checks, but **the existing DirectSound API cannot be used directly for this
Xbox title**. Its normal startup creates Xbox ADPCM buffers, and its stream API
returns success with a NULL object.

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

## Existing implementation audit

`third_party/xboxrecomp/src/audio/dsound_device.c` provides:

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
shutdown with the existing lightweight audio tests. No such title-specific
DirectSound bridge has been implemented or tested by this investigation.

Local evidence: `local/reports/scan_audio_signatures.py`,
`audio-signature-matches.json`, `audio-signatures-xref-resolved.json`, downloaded
`xbsdb/DSound/*.inl`, `cxbx-XbDSoundTypes.h`, and the original generated `.text` /
DSOUND disassemblies. The refined signature report still contains candidates for
ambiguous wrappers whose callee signature was unavailable: use the confirmed
addresses above, not every report entry as a binding.
