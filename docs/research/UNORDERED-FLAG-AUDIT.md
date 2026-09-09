# Unordered floating-point flags: bounded source audit

2026-09-08, after build67. No production change or gameplay test. Original
instructions, generated C, and one temporary emitted-C fixture were inspected.

## Established scope

Raw disassembly of the already-generated discovered functions contains13 SAHF
instructions and no FCOMI/FUCOMI/FCOMIP/FUCOMIP or
COMISS/UCOMISS/COMISD/UCOMISD. This is a bounded inventory, not proof about every
possible undiscovered function. Four SAHF candidates occur in decoded data or
instructions after unconditional exits in69B80,A8240,142110; they are excluded
from actionable evidence. F88ED has two CRT-shaped SAHF sequences but no located
caller or absolute pointer reference, so it is not used as reachability evidence.

These genuine paths have original caller evidence:

| Code | Original producer/consumer | Evidence |
| --- | --- | --- |
| F488D | F4891 FCOMP; F4896 FNSTSW AX; F4898 SAHF; F4899 JNE. Second compare/SAHF/JNE at F48A7/F48AC/F48AD. | F46C8 calls at F4822/returnF4827 and F486A/returnF486F. |
| F78AE | F7953 FCOMP; F795A FNSTSW AX; F795C SAHF; F795D JAE. Second FCOMP/SAHF/JBE at F7981/F798A/F798B. | Calls F780B/returnF7810 from F77E4, F784F/returnF7854 from F7822. |
| F8A91 | F8AF2 FCOMP; F8AF9 FNSTSW AX; F8AFB SAHF; MOV EAX,4; F8B01 JAE. Second FCOMP/SAHF/MOV EAX,3/JBE at F8B1B/F8B24/F8B25/F8B2A. | Genuine tail target from F46C8. |

These are static paths. No observed gameplay NaN or live execution of the faulty
branches is claimed. The preceding CRT classification/error paths may constrain
which inputs reach each branch.

## Discriminating counterexample

For a masked-invalid x87 comparison with a NaN operand, the status condition
bits are C3/C2/C0=111. FNSTSW AX followed by SAHF consequently sets ZF/PF/CF=111.
The current model correctly represents the comparison as `g_fp_cmp=2`, but
SAHF emits only a comment and its branch conditions compare that enum with zero.
JNE and JAE become true, while JBE and JE become false: each contradicts the
actual flag bits. Ordered inputs can conceal this error.

The temporary fixture uses actual Lifter emissions and condition construction,
compiles native C under UBSan, and tests eight unsigned/equality/parity branches
against explicit flag tables for less/equal/greater, NaNs in either operand,
equal infinities and signed zero. It reports **168 checks,44 mismatches**:
SAHF16, FCOMI16, COMISS12, all on the two NaN cases. FCOMI/SSE results establish
generic lifter gaps; no genuine game instruction sites were found for them.
It is an explicitly failing local diagnostic, not an ordinary regression test
added to the passing suite. Reproduce with
`.venv/bin/python local/reports/run_unordered_audit.py`; output is
`local/reports/unordered-emitted-cpu.log`.

Intel documents the compare flag table and SAHF bit transfer in its
[instruction-set manual](https://cdrdv2-public.intel.com/868137/325462-089-sdm-vol-1-2abcd-3abcd-4.pdf).
Correcting the predicates does not implement invalid-operation exception
production/delivery, signaling-NaN distinctions, or complete host/guest floating
control compatibility.

## SAHF and FPREM must be considered together

SAHF is an integer transfer from AH, not a floating comparison. A faithful
implementation snapshots AH bits7,6,4,2,0 into SF/ZF/AF/PF/CF and preserves OF.
Merely changing `g_fp_cmp==2` predicates still fails for status produced by
non-comparison x87 operations, or arbitrary AH values. Reading AH only when
branching also fails after intervening MOV: the original F8A91 sequences above
explicitly overwrite EAX after SAHF. The snapshot must occur at SAHF itself.
Existing tracked carry consumers must see the copied CF, while signed conditions
requiring preserved OF need a proven prior OF state rather than an invented one.

The sole original SAHF→JP sequence is F4346 FPREM; F4349 FNSTSW AX; F434C SAHF;
F434D JP F4346. This tests the remainder operation's C2 completion bit, not NaN.
Although F4344 is labeled a tail alias, it has real indirect caller evidence:

- Descriptor1B3530 stores F4344 at1B3540. F4330/F433A load that descriptor into
  EDX and tail-call F785F/F77E4, which call classification helper F7ACB.
- F7ACB performs FXAM and dispatches via descriptor+10h+class offset. Normal
  finite values use lookup1B3624[8]=0 for both operands, selecting F4344.
- Original calls to F433A occur at1A5C3/return1A5C8, B0E6C/returnB0E71,
  E7EC0/returnE7EC5, E7EF6/returnE7EFB, E8825/returnE882A and E88B1/returnE88B6.

FXAM of a normal finite value leaves C2=1. Current FPREM computes the complete
binary64 `fmod` result but does not update any x87 status bits. For example,
normal operands5.5 and2 produce remainder1.5 while leaving C2 set. Today the
emitted JP is constant false. Correcting SAHF to read AH without correcting
FPREM completion status could loop forever on this ordinary finite path.

A subsequent bounded repair should first validate FPREM completion/status
semantics, then SAHF's actual AH snapshot and relevant branches as a unit.
FPREM also defines quotient bits in C0/C1/C3; clearing all status to simulate
completion would fabricate or discard state. Extended precision, exceptional
inputs and quotient-bit preservation must be stated accurately. FCOMI/SSE
predicate work can remain separate because no genuine sites were found here.

Original inventory/contexts are recorded in
`local/reports/unordered-original-inventory.json` and
`local/reports/unordered-original-contexts.txt`. Generated references are in
`local/reports/unordered-generated-sites.json`. No game assets, generated code,
or known-failing default test was committed.
