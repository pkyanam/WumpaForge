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
Expression JIT is disabled using Apple LLDB's `SetAllowJIT(False)`; only the
fixed simple variable names are evaluated, with no call expressions. Missing/optimized values are independent errors; original
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
and exact bounded object/vtable reads. The installed Apple LLDB also passed import and capture against an empty target,
recording unavailable fields without aborting. Real stopped-game validation is
pending the parent's coordinated next capture.

The first startup-only run survived its15-second watchdog. Its probe initially
failed because Apple LLDB does not expose upstream SetExecutionPolicy. The option
setup now uses the verified host API and sits inside per-field error handling.
No audio failure was reproduced by that run.

## Healthy native capture

The second15-second startup run also survived and successfully saved
`local/reports/audio-startup-02.json` using the installed Apple LLDB. The original
wrapper0 sound pointer is `010FF430`, matching native `s_streams[0].guest` with a
nonzero native object pointer. Its first DWORD is `16B70C`; all seven captured
vtable methods exactly match the XBE, including GetStatus `1363D6` at offset0C.
Wrapper0 is feeding logos audio172 from the real contiguous address80000000;
all three packet status words are PENDING8000000A. The other five wrappers have
null object pointers, matching their inactive state. Capture errors are empty.

This stop selected `xbox_watchdog_thread`, not the guest audio worker, so its
zero-valued guest TLS registers describe that watchdog thread. They do not
establish worker register correctness. At a recurrence of the unresolved call,
the selected faulting worker's TLS and object state must be compared instead.
Two short runs without the fault establish successful capture and healthy
startup examples, not resolution of the intermittent story13 failure. Continue
the primary gameplay work and retain this probe for a naturally recurring stop.
