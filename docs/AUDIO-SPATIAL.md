# Native spatial sound: distance and stereo positioning

2026-09-08. The original64 sound-effect buffers are created with flags40010,
including DSBCAPS_CTRL3D10; the other188 buffers use40000. Native music streams
remain separate. This change replaces the seven reached spatial E_NOTIMPL bridge
boundaries with actual audible state and mixing behavior, without running Xbox
APU hardware or CPU emulation.

## Original interfaces and behavior

All floats and pointers below are guest DWORD slots. The bridge reads float bits
explicitly and validates native device/buffer identity before calling its backend.

| Address | Interface arguments after object | Application |
| --- | --- | --- |
|136D61 |minimum distance, apply |Original3.0, deferred |
|136D3D |maximum distance, apply |Original50.0, deferred; previously skipped after min failure |
|136D85 |source x,y,z,apply |Original immediate |
|1374E1 |listener x,y,z,apply |Original deferred |
|137497 |front xyz,top xyz,apply |Original deferred |
|137516 |rolloff,apply |Original1.0, immediate |
|136D09 |none |Commit all deferred source/listener properties |

Apply0 changes current and pending copies of that property. Apply1 changes only
its pending copy. Commit publishes the pending listener and sources together.
An immediate write cancels a previous deferred write to the same property without
prematurely applying other pending properties. Invalid apply values, nonfinite
coordinates, nonpositive distances, rolloff outside0..10, and degenerate listener
orientations return E_INVALIDARG without changing state. Top/front are normalized
and front is orthogonalized against top. Spatial creation and SetFormat require
mono data; incompatible stereo changes fail transactionally. Ordinary stereo and
non3D sound data retain their channel layout and volume.

## Audible compatibility model and limits

For minimum distance m, maximum M, listener/source distance d and rolloff r:

```
effective_distance = max(m, min(d, M))
attenuation = m / (m + (effective_distance - m) * r)
```

At the title's defaults3/50/r1, relative amplitude is1 at distance3,0.5 at6 and
0.06 at50 and beyond. Maximum distance caps attenuation rather than muting the
source. Rolloff0 disables distance attenuation. Source volume remains a separate
factor, and sample rate/cursor/packet timing are unchanged.

A normalized right vector is computed from listener top cross front, matching
left-handed +X right/+Y up/+Z front coordinates. The source direction's right
component gives pan in−1..1. A stereo constant-power compatibility mix applies
sqrt((1−pan)/2) to the left and sqrt((1+pan)/2) to the right, multiplied by distance
attenuation. Centered spatial mono is therefore approximately−3dB per channel;
non3D mono still duplicates at its previous gain. This produces actual directional
output but is explicitly a **stereo speaker approximation**, not a reproduction
of Xbox headphone HRTF, elevation/front-back filtering or hardware mixbin curves.
The continuous gain calculation also does not claim exact hardware millibel
quantization. Verify the perceived balance in the game before broader claims.

HRTF/full/headphone requests, DSP effect-image loading, I3DL2/reverb remain explicit
unsupported boundaries. Native velocity/cone/Doppler setters are also explicit
E_NOTIMPL rather than the previous no-op successes. No new success is returned for
those unimplemented effects. Spatial packet streams remain unsupported; the
reached music streams do not request spatial flags.

## Threading and cost

The guest bridge lock protects guest object identity and publication. The native
buffer/listener lock protects current/pending spatial state and buffer lifetimes.
Listener commits compute at most64 active spatial voice gains, then install the
whole batch under the existing mixer lock. The mixer cannot observe half a commit.
Immediate source changes update only that source's gain pair. Release removes the
voice under the same mixer lock; guest-slot recreation gets default spatial state.

No allocation, trigonometric operation or new lock is added inside the per-sample
loop. Gain preparation is outside mixing; gain and ordinary volume are multiplied
once per voice/frame. Mixing retains its existing one multiply per output sample.
No new thread, ring buffering, rate adjustment, packet delay or device is added.
This is a bounded-cost design, not a measured full-game performance claim.

## Tests and integration status

`tools/test_audio_spatial.c` uses real guest ABI/native methods and production
mixer code with synthetic PCM. A valid APU lock/condition is initialized without
a producer, so only explicit mixer calls advance time. It verifies exact left/
right PCM, min3/max50, rolloff0/1/2, listener translation/rotation, deferred source
and listener settings, immediate cancellation of a deferred orientation, input
validation, volume, non3D stereo preservation, invalid stereo spatial creation/
format changes, release/reuse, and concurrent atomic commits. The mixer thread
renders3000 blocks while another thread makes1000 two-source commits; every frame
must contain one of the two complete stereo states, never a partial transition.

```
clang -std=c11 -O1 -g -fsanitize=undefined -Ithird_party/xboxrecomp/src -Ithird_party/xboxrecomp/src/apu -Ithird_party/xboxrecomp/src/nv2a $(pkg-config --cflags sdl2) tools/test_audio_spatial.c third_party/xboxrecomp/src/audio/dsound_device.c third_party/xboxrecomp/src/audio/xbox_adpcm.c third_party/xboxrecomp/src/apu/apu_vp.c third_party/xboxrecomp/src/apu/apu_dsp.c third_party/xboxrecomp/src/apu/apu_xaudio2.c build/native/third_party/xboxrecomp/src/platform/libplatform.a $(pkg-config --libs sdl2) -o build/test_audio_spatial
./build/test_audio_spatial
```

UBSan passes. Existing packet/ADPCM/stream-overlap tests and the252-buffer/real
producer bridge regression also pass, with their commands in AUDIO-INTEGRATION.
Logs are `audio-spatial-test.log`, `audio-stream-spatial-regression.log`, and
`audio-bridge-spatial-regression.log` under local/reports. Tests emit no audible
sound and use no supplied game assets. Cumulative audio patch replay was checked
against the upstream base for all10 affected audio files. Root must rebuild the
full target because APUMixerVoice gained two fields. No game launch, process
attach or package mutation was performed by this agent; in-game spatial balance
and regression testing remain pending. The intermittent story13 null GetStatus
failure is unaffected and remains open.

## Primary references and provenance

Microsoft's [minimum/maximum-distance contract](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee418662(v=vs.85))
specifies amplitude halving with doubled distance, default min1/max1billion and
attenuation clamping at maximum distance. Its
[listener documentation](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/bb318698(v=vs.85))
specifies default axes, orientation adjustment, rolloff0..10 and immediate writes
superseding pending settings. The
[Wine DirectSound implementation](https://github.com/wine-mirror/wine/blob/master/dlls/dsound/sound3d.c)
provides a primary implementation cross-check for min/max clamping and rolloff
scaling the distance beyond the minimum. These are PC API contracts and a
compatibility implementation, not proof of Xbox HRTF identity. The original
Xbox call sites and flags come from the user's XBE and are recorded in
COMPATIBILITY-REMAINING.md. No Wine implementation text was copied; the new state
and gain code is independently written in the existing licensed runtime.
