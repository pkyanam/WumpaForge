# Guest EBP across native function boundaries

The source audit found a concrete frame-state error, independently of the reported
story/demo artifacts. Before this correction, standard-frame generated functions
such as original `EE011` and `EE073` declared an uninitialized local `ebp`, then
executed the original `PUSH EBP; MOV EBP,ESP`. Their saved guest frame DWORD was
therefore host-local garbage. Their eventual `LEAVE` restored that garbage.
Ordinary returns also left the two TLS EBP mirrors holding a nested callee's frame;
a frameless relay could consequently pass a dead frame to its next helper.

`tools/test_frame_state.py` translates small original instruction sequences and
compiles them to native C with UBSan and deterministic Clang automatic-variable
initialization. The five baseline cases all failed. Saved EBP was `AAAAAAAA`
instead of `7770`, and a framed indirect call inherited `7FF4` instead of its
current `7FFC` frame. An explicit `MOV EBP,4567` register argument was also lost.
The baseline is recorded locally in `local/reports/frame-state-before.log`.

The bounded correction initializes every local EBP from the incoming register,
publishes that actual local before direct and indirect calls, and publishes it
again at RET after the original POP/LEAVE epilog. External direct, indirect,
manual, conditional and discovered fallthrough tails publish both mirrors before
transferring control. Functions with no local EBP retain the register returned by
their callee. There is no blanket call-wrapper restoration and no synthesized
frame unwind: original instructions still control all stack movement.

The existing SEH exchange remains explicit. The original detected prolog `F5D38`
stores the incoming EBP at `[ESP+10h]` before establishing its frame; epilog
`F5D71` restores EBP with LEAVE before RET. Their caller still reads back the
changed frame. The existing `fpo_leaf` inheritance from `g_seh_ebp` remains because
native inline-thread entry uses that mirror. Original setjmp `F68DC` saves EBP at
jmp_buf offset zero, and the existing native longjmp restores both TLS mirrors
before its caller reloads local EBP. These paths were reviewed without altering
the native nonlocal-jump implementation.

Validation: all 11 native fixture cases pass, including saved guest-frame bytes,
frameless relay, framed indirect calls, an explicit register argument, every tail
category above, and deliberate SEH frame exchange across a nested call. Each case
also checks ESP, caller stack sentinels and unchanged EBX/ESI/EDI. The eight related
flag, arithmetic, lifter and SAHF/remainder suites pass. Re-translation of 4,546
original functions against `383c2f1` retains exactly 47 fallback conditions, with
no function gaining one. The inventory's eight textually changed tail lines only
add the EBP publication to the branch body; their predicates and locations are
unchanged. No generated game sources were written by these tests.

The cumulative lifter patch was applied to fresh upstream files and all eight
resulting files compared byte-for-byte with the working dependency. Root owns
full native game rebuilding and subsequent integration validation.

Remaining limit: a non-SEH callee that intentionally returns a changed EBP does
publish it into TLS, but an already-local-EBP caller does not automatically read
it back. Such register-return conventions need their own original-code evidence;
blind readback/restoration could change established special-call behavior. This
fix neither establishes the cause of the user's intermittent distortion nor
claims whole-game ABI correctness.
