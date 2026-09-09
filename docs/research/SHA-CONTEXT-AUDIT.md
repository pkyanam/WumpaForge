# Xbox SHA context layout

Source-only compatibility audit,2026-09-08. The supplied XBE imports kernel
ordinals335,336,337: XcSHAInit, XcSHAUpdate and XcSHAFinal. This is not evidence
that save/load has completed successfully, nor a demonstrated cause of the
reported graphics artifacts or crash.

## Contract and original caller

The [nxdk interface](https://github.com/XboxDev/nxdk/blob/29638d0b001f179b73c3513489af10ddc2986216/lib/xboxkrnl/xboxkrnl.h#L1537)
specifies116 bytes of SHA context storage. The independently maintained
[Cxbx kernel wrappers](https://github.com/Cxbx-Reloaded/Cxbx-Reloaded/blob/585c49a50af1255ab155099e06f24505f9c5a800/src/core/kernel/exports/EmuKrnlXc.cpp#L51)
place the92-byte software state after a24-byte prefix. Its state/count/buffer
fields consequently start at24/44/52. These sources corroborate the guest layout;
no third-party implementation body was copied for this correction.

The original game's SDK function `EE120` allocates `7C` bytes, stores two DWORD
fields, then passes allocation+8 to `EE011` at call `EE18E`/return `EE193`.
`EE011` forwards that pointer to SHA init through `F4238` at `EE05C`/`EE061`,
then SHA update through `F4232` at `EE06A`/`EE06F`. Thus the remaining context
storage is exactly `7C-8=74` bytes, or116. Original `EE19B` likewise adds8 before
updating. `EE073` finalizes and restarts the context to construct an outer hash.
The thunk slots are15AB58(init),15AB54(update),15AB5C(final). Generated C was
used for navigation; original instruction bytes supply the allocation/call facts.
No original executable bytes, keys or generated functions are tracked here.

## Correction and validation

The runtime's `XBOX_SHA_CONTEXT` previously contained only the92-byte state at
offset0. Its own consistent init/update/final routines could still produce the
right digest; that did not establish guest layout compatibility. The structure
now includes the24-byte reserved prefix and preserves the original fixed-width
state/count/buffer fields. The implementation leaves reserved bytes untouched.
No arithmetic, digest algorithm, console key, signature policy or save file is
changed. Obsolete comments about storing a host BCrypt handle were removed.

`tools/tests/sha_context.c` tests actual runtime SHA functions with explicit
116-byte storage, canaries before/after it, state/count/buffer offsets and output
canaries. Public [SHA-1 vectors](https://www.rfc-editor.org/rfc/rfc3174.html#section-7.3)
cover empty input,`abc`, the standard56-byte message and one million`a` bytes.
Two copied contexts continue with different update chunk sizes and produce the
same digest. The pre-fix fixture reports79 failed layout/prefix checks while its
digest checks pass. The corrected fixture passes under Clang/UBSan.

Run `.venv/bin/python tools/test_sha_context.py`, or the synthetic suite in
[testing instructions](../TESTING.md). Logs are ignored under
`local/reports/sha-context-{before,after}.log`. This establishes the tested storage
and hashing contracts. Context-finalization wiping, unusual pointer aliasing,
console-key interoperability and the game's complete save/load flow remain
outside this fixture; no existing user save data was modified.
