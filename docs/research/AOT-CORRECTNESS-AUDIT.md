# AOT arithmetic and thread-state audit — 2026-09-08

This is a source/CPU-component audit. No game launch, debugger attach, UI action
or gameplay validation was performed. It does not establish the cause of the
reported intermittent demo distortion.

## Corrected guest rounding and NaN tests

The lifter emitted host `rint` for FRNDINT although FLDCW changes only the guest
control word. For example, guest round-down of-2.5 produced-2 with host nearest,
instead of-3. FTST reported NaN as equal instead of unordered. The emitted-code
fixture in `tools/test_x87_status_rounding.py` reproduced61 mismatches before the
fix and zero afterward, across all four host modes and all four guest modes.
It also covers signed zero, ties, infinities, NaNs and unchanged stack depth.
Existing FIST/FISTP and actual D3DX color-packing tests still pass. Both CPU
executables use UBSan and float-cast-overflow instrumentation.

FRNDINT now selects the guest RC explicitly, with nearest-even independent of
host state. FTST uses the same unordered-aware comparator as FCOM. These changes
do not implement x87 extended precision, arithmetic exception delivery or full
status flags. Reference: Intel's [instruction-set manual](https://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-software-developer-vol-2a-manual.pdf).

The existing generated source contains nine FRNDINT sites in F488D(2), F84A0,
F88AA, F88ED(2), F89A8,105A40 and13DF84. Five FTST sites occur in F885C,F88AA(2)
and105A40(2). SDK105A40 is called by10531B/return105320;13DF84 is in DSOUND.
These are static references, not evidence that a particular live frame executes
the faulty input cases. Reports: `local/reports/x87-status-before.log`,
`x87-status-fixed.log`, `x87-fist-regression.log`, `x87-lifter-regression.log`.

## CRT classification and byte arithmetic repaired

The three FXAM instructions at `F7A91`, `F7AF8`, and `F7B16` previously emitted
comments. The following FNSTSW therefore exposed stale comparison bits. The new
thread-local status word separates x87 condition codes from the comparison
snapshot used for EFLAGS, includes TOP, and classifies sign, zero, finite normal,
infinity, NaN, and an empty physical register. A binary64 subnormal is normal in
the x87 extended exponent range after FLD, so FXAM correctly classifies it as
normal in the current binary64 execution model. Push/pop, FST/FSTP register
stores, FXCH, comparisons, FNINIT, and EMMS maintain the modeled state. FCOMI
changes its EFLAGS comparison snapshot without replacing x87 condition codes.

The same original classifiers contain these three byte sequences:

| Original addresses | Original operations |
| --- | --- |
| `F7AAE`, `F7AB0`, `F7AB2` | SHL CL,1; SAR CL,1; ROL CL,1 |
| `F7B27`, `F7B29`, `F7B2B` | SHL CH,1; SAR CH,1; ROL CH,1 |
| `F7B34`, `F7B36`, `F7B38` | SHL CL,1; SAR CL,1; ROL CL,1 |

The old SAR8 expression sign-extended a zero-extended byte as int32: `80h >> 1`
produced `40h` instead of `C0h`. ROL8 used ROL32 and truncated, turning `80h`
into `00h` instead of `01h`. For FNSTSW AH=`40h`, the whole original sequence
must deliver classifier index 1; the previous code delivered 0. These indexes
feed the table at `1B3624` in CRT functions `F7A64` and `F7ACB`.

SAR and ROL/ROR now use their actual 8/16/32-bit widths and the x86 five-bit
count mask. Carry preserves its prior value for masked count zero. Rotation
carry updates for nonzero masked counts, including a full-width byte/word
rotation; overflow is defined for masked count one. Immediate one-bit overflow
branches use a result snapshot even if MOV overwrites the destination. Signed
branches after byte/word shifts also use the operand's sign bit. This is not a
complete EFLAGS implementation: variable zero-count branch-state merges and
flags preserved across partial flag writers still need a broader audit.

`tools/test_shift_rotate_widths.py` passes under UBSan: every byte value, counts
0–63, both initial carry values, representative 16/32-bit values, both rotation
directions, defined overflow, sign branches, and the three original classifier
sequences. It additionally compiles complete translated functions containing
ROL/ROR, a destination-overwriting MOV, and JO/JB, checking every byte value.
`tools/test_x87_classification.py` passes against the production TLS definitions:
class/sign/empty/TOP, stack operations, comparison-status separation, exception
clear, initialization, and two native pthreads repeatedly checking distinct
state. Existing FIST, FRNDINT/FTST, flag-merge, lifter diagnostic, and incremental
translation tests also pass. Logs are `local/reports/shift-rotate-widths.log`,
`x87-classification.log`, and `x87-*-regression.log`. No game/UI test was run for
this audit and these counterexamples do not prove the reported demo's cause.

Reference: Intel's [instruction-set manual](https://cdrdv2-public.intel.com/774492/325383-sdm-vol-2abcd.pdf),
including FXAM, FNSTSW, FNCLEX, SAR and ROL/ROR; the [manual update](https://cdrdv2-public.intel.com/671294/252046-sdm-change-document.pdf)
spells out the masked-count rotation flag rules.

## Explicit remaining boundaries

FNCLEX at `F428D` follows calls from initialization helper `F4283` to `F424B`
and `F6EDD`; `F85FC` follows FNSTSW in helper `F85F7`, called at `F8011` with
return `F8016`. It now clears status bits 0–7 and B while preserving TOP,
conditions, tags and control. The runtime still does not generate arithmetic
exception bits or deliver unmasked x87 exceptions; clearing modeled state does
not supply that missing exception model.

FNSAVE at `F47D8 [ecx+8]` and FRSTOR at `F47E4 [ecx+8]` surround the call to
`F8B95` at `F47DB`, in `F46C8`'s CRT special-value/error branch. The surrounding
stack reservation is `74h` bytes. The original 108-byte environment/register
image includes state absent from the binary64 model. Both now emit the existing
address-bearing unsupported-instruction diagnostic, with explicit regression
assertions, instead of silently dropping the operation. The inspected live
reports do not establish whether this special branch is reached; a future
execution can therefore stop here. No fabricated 80-bit save image was added.
Original disassembly is recorded locally in `x87-original-boundaries.txt`.

The current x87 model does not provide 80-bit execution precision, true extended
subnormals/unsupported encodings, full tag classes, stack-fault arithmetic,
precision/rounding exception flags, or MMX/x87 value aliasing. Direct FCOMI/SAHF
unordered EFLAGS branches and SSE compare parity remain separate pre-existing
semantic gaps requiring their own counterexamples and bounded changes.

All guest integer, x87, XMM and MMX register definitions inspected in
`xbox_memory_layout.c` use RECOMP_TLS. Generated function bodies contain no
static temporaries or host `long` tokens. Existing worker tests check separate
guest registers/TIB/TLS/stacks/heap ownership. Indirect-call trace counters/ring
entries remain shared volatile diagnostics: concurrent updates can lose trace
ordering, but bounded indices do not feed the actual local dispatch target or
transform arithmetic. Shared guest memory remains a separate concurrency-model
limitation, not a demonstrated demo race.

## Build66 logical/double-shift follow-up

The compiler warning at generated `recomp_0005.c:8393` exposed an unmasked
`SHR [EAX],89h` at discovered function `41B20`. Original bytes show this is a
misaligned `imm_ref_target` seed: `41B1F: 83 C0 28` is ADD EAX,28h and
`41B22: 89 41 14` is MOV [ECX+14h],EAX. Starting at41B20 instead decodes operand
bytes as SHR. The database records no direct callers; neighboring41B0C/14/1C/24
are also immediate-reference discoveries. The NV2A SetPalette method value is
also41B20. No discovered function boundary was removed on that evidence alone.

Independent emitted CPU fixtures prove the helper issue: raw C shifts did not
mask the count and promoted byte/word operands to signed int. UBSan stopped on
an emitted shift exponent136 for count89h. Logical SHL/SHR now mask the count to
five bits, truncate operands/results to8/16/32 bits, preserve masked-zero helper
flags, and produce defined carry and one-bit overflow without C shift UB.
Snapshots retain ZF/SF/PF/CF and immediate one-bit OF branch inputs after MOV;
immediate masked-zero shifts preserve the preceding flag state and snapshots.
Undefined SHL/SHR carry for masked counts at least the operand width is assigned
zero, without treating it as a defined architectural result.

There is also a directly called, valid original path: CRT64 helpers F4600,
F61A0 and F64A0 use SHLD/SHRD for counts below32, including zero. The old
expressions shifted the other DWORD by32 at zero. The CPU fixture translates
these original functions into temporary files, reproduces that UBSan error,
and now passes all256 CL values for left, logical-right and arithmetic-right
64-bit results. No original game bytes or generated functions are tracked.

| Helper | Original call/return evidence |
| --- | --- |
| F4600, left | 669C2/669C7;66E67/66E6C;70FB3/70FB8;7116B/71170;87430/87435 |
| F61A0, logical right | EE2A7/EE2AC |
| F64A0, arithmetic right | 1560D3/1560D8 |

SHLD/SHRD now mask counts, retain the destination for zero, and handle16/32-bit
operands, defined carry and one-bit overflow. The16-bit result and flags are
undefined above count16; the helper deterministically retains the destination
and flags there. No global EFLAGS redesign was attempted. In particular,
variable zero-count merges with preceding flag writers and variable-count OF
branch reconstruction remain limitations; helper preservation alone does not
claim those control-flow cases are fixed.

`tools/test_logical_shift_widths.py` passes UBSan for all byte values/counts,
representative16/32-bit values, and complete translated JO/JB/JE/JS/JP functions
with an intervening destination-overwriting MOV and another masked-zero shift.
`tools/test_double_shifts.py` passes the actual CRT64 functions and independent
bit-step SHLD/SHRD16/32 result/defined-flag cases. Existing rotate, flag-merge,
incremental translation and unsupported-instruction tests pass. Reports:
`logical-shifts-before.log`, `logical-shifts-fixed.log`, `double-shifts-before.log`,
`double-shifts-fixed.log`, `logical-double-shift-original.txt` under local/reports.

The inventory of already-generated functions contains260 variable32-bit logical
shifts,41 variable byte shifts and5 variable word shifts. Its sole immediate
count at least32 is the false-positive41B20 seed. This inventory includes
existing discovery aliases/false positives and supplies no runtime frequency or
claim that an out-of-range count occurred during the reported demo. Full details
are in `local/reports/logical-shift-inventory.json`.
