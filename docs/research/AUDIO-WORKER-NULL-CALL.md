# Intermittent original audio worker null call

Story13/build54 stopped near title-intro startup, after native stream creation
`137AA4` returned S_OK for output `431EDC`. The stack identifies original
`5DABB`, compiled at `recomp_0009.c:6241`: the worker reads its stream pointer
from `[esi]`, the vtable from `[stream]`, then calls `[vtable+0C]` (GetStatus),
return address `5DABE`. The unresolved target was zero. No packet or volume
bridge call was logged before that stop.

This is insufficient to identify a native stream lifetime bug. Constructor
initializes the guest object/vtable before publishing its pointer; creation and
release hold the same bridge mutex. Final release zeros the object, and original
`5D420` normally releases then zeros its wrapper pointers. The guest heap
allocator also serializes its allocations. The immutable original vtable at
`16B70C` contains seven nonzero methods in the XBE, with `1363D6` at offset0C.

Unchanged story14 audio startup succeeded: original packets decoded and reached
the real output callback with nonzero samples, zero reported underruns and
overflows before its later graphics stop. No speculative audio fix was made.
The original stopped story13 capture lacks its audio object/register values;
the failure remains open until reproduced with these values.

## Read-only next-stop capture

Parent owns all launches/attaches. At `recomp_icall_not_code_log`, run:

```
command script import tools/audio_probe.py
script audio_probe.dump(lldb.debugger, 'local/reports/audio-stop.json')
```

The probe uses the selected stopped frame's simple TLS variable expressions
(`g_esi`, `g_eax`, `g_ecx`, `g_esp` and other GPRs), then bounded memory reads.
Expression execution policy is Never, so failed debugger materialization cannot
run target code. Missing/optimized values are independent errors; original
wrapper/vtable captures still proceed. No guest or native audio methods run.

It captures all six 68-byte original wrappers at `431ED8`, each wrapper's file
and sound objects, their vtables, original `16B70C`, guest stack, selected ESI
object, and native `s_streams` guest/native identities. This distinguishes an
incorrect wrapper pointer, zeroed/released object, overwritten vtable, and a
native metadata mismatch. A healthy first stream has wrapper+4 pointing to a
registered native bridge object, whose DWORD0 is16B70C and method+0C is1363D6.

Validation: `python3 -m py_compile tools/audio_probe.py` and
`python3 tools/test_audio_probe.py` passed. The latter is a mocked debugger
fixture covering TLS fallback, unavailable values, independent memory failure,
and exact bounded object/vtable reads. Real stopped-game probe validation is
pending the parent's coordinated next capture.
