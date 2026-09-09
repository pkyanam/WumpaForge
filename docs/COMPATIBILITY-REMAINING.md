# Remaining compatibility priorities — 2026-09-08

Read-only audit of normal runs and existing stopped-game captures, after depth
fix656f4c4. No process attach, game launch, UI action or runtime edit was performed.

## Observed run boundaries

`local/reports/play-24.log` ends with an explicit window-close request at22359
lines, last profile frame66900. It contains five offscreen SetRenderTarget
rejections, already addressed by656f4c4, and no unresolved compiled-call marker.
The `play-25.log` snapshot through frame13916/13420 lines contains no target
rejection, unresolved compiled call, or kernel unbridged-invocation warning.
Its last profile is Level37/Demo0 at60.03FPS; that single hub window does not
establish sustained snow performance or complete rendering. Logs alone do not
establish audio quality, underrun counts or absence of faults outside this path.

## First implementable feature: actual spatial sound

The current run reaches these public ABI boundaries and returns E_NOTIMPL:

| Operation | Original SDK address | Logged guest return |
| --- | --- | --- |
| Buffer minimum distance |136D61|B8D7A|
| Buffer source position |136D85|B8FC5|
| Listener position |1374E1|B96C9|
| Listener orientation |137497|B96F8|
| Listener rolloff |137516|B9B8B|
| Commit deferred spatial settings |136D09|B9898|

Evidence: `play-25.log` lines524,528,2351–2353,5262;
`src/audio_bridge.c` bindings and original functionsB8D20/B8F70/B96B0/B96D0/B9890
in ignored generated source `recomp_0020.c`. AtB8D7A a negative result branches
toB8D93, skipping the subsequent SetMaxDistance136D3D call that would set50.0
(returnB8D8D). Minimum distance is3.0/deferred. The outer64-buffer constructor
loop atB9AB1 ignores this helper result and continues; this is an incomplete
audio setup, not evidence of an initialization crash.

Original argument order is `(buffer, distance, apply)` for min/max; source
position is `(buffer, x, y, z, 0)` with immediate application. Listener position
is `(device, x, y, z, 1)` and orientation is `(device, frontX, frontY, frontZ,
topX, topY, topZ, 1)`, both deferred. Commit takes only the device. Startup rolloff
is `(device, 1.0f, 0)`; the separate B9700 path can change that factor. These
are guest DWORD/float slots, not host-width fields. Source-position helperB8F70
copies coordinates into its wrapper+1C/+20/+24 before returning the SDK HRESULT.

A bounded implementation must retain source/listener state, implement immediate
versus deferred application, and actually apply documented distance attenuation
and stereo positioning to the native mixer. The backend's current distance and
position methods are also no-op successes; routing the bridge to them would
conceal the gap. Validate original apply/rolloff/channel contracts before coding,
then test directional PCM output, distances3/50, deferred commit, source motion
and release/reuse. Keep DSP image/HRTF/I3DL2 effects explicitly incomplete until
they have real implementations; those separately reachB9A40/B9A54/B9B7D/B9473.

## Highest crash priority: capture the intermittent audio worker fault

`story-13.log` line2409 reports target0; its faulting thread20 backtrace at2653–
2655 reaches original worker5DA30, generated `recomp_0009.c:6241`. Original5DABB
calls stream vtable+0C (GetStatus1363D6), return5DABE. The stopped report lacks
the wrapper/object/register values needed to distinguish stale ownership from
an overwritten pointer. Healthy subsequent captures do not resolve this fault.

At a natural recurrence, root should retain the faulting thread and run the
existing `tools/audio_probe.py` dump: wrapper431ED8+4, object DWORD0, vtable16B70C,
method+0C, native stream identity, guest TLS and stack. See
[the full fault audit](research/AUDIO-WORKER-NULL-CALL.md). Do not add a null-call
bypass or speculative lifetime change without that evidence.

## Lower priority markers that do not currently identify a crash

- FVF0 is rejected four times in play24 and six times in this play25 snapshot,
  immediately around resource teardown/loading. Original B6800 explicitly calls
  SetVertexShader102940 with0 (returnB680B) before releasing shader objects.
  This is a concrete static candidate caller; current runtime lines omit the
  caller, so the match remains unverified. Capture that caller and subsequent
  draw/state before changing accepted FVF behavior.
- Kernel91 (IoDismountVolumeByName) and8 (DbgPrint) are unbridged imports at startup.
  Neither normal log contains the dispatcher's warning for an actual invocation.
  Original154910 calls91 only on its error-cleanup branch, return154915. These
  registrations alone do not establish an active gameplay fault.
- Repeated missing cubemap DDS/xloading.nux probes have original absent-file
  handling documented in [ASSET-AUDIT.md](ASSET-AUDIT.md). Do not synthesize assets
  or group all failed-open lines into a crash diagnosis. Other optional-extension
  probes still need their individual caller/ISO checks if they become relevant.
