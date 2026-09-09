# Mixed flag joins in original mask movement

Source audit, 2026-09-08. This is a demonstrated AOT translation defect, not a
demonstrated cause of the reported Cortex hologram or character distortion.

## Original function and effect

The supplied Xbox executable calls `62240` directly from `2B534` in `2B2D0`,
itself called by game main `87BF0`. It is a complete function ending at `62B92`,
not an overlapping tail or decoded data. The call requires a nonzero pointer
at `8F512C`, a nonzero word at that object's `+16E`, and clear bits `0E` in the
current level descriptor's `+25`. No live branch hit was collected in this audit.

The GameCube-derived recovered
[UpdateMask function](https://github.com/Open-Travelers/OpenCrashWOC/blob/a6f483f49c67402386b5294db0a8e35e91b26c2d/code/src/gamecode/game.c#L1207)
corroborates the function identity: the same mode-bit tests, wrapped angular
differences, `TurnRot(..., 0x444)`, and sine/cosine offsets of radius `0.7`
position the following mask. Xbox instruction bytes remain the authority;
the recovered C is not a replacement implementation or exact Xbox oracle.

Two original paths join at `62500: JGE 6250A` (`7D 08`):

| Incoming path | Last actual flag setter | Required signed branch |
|---|---|---|
| Mode bit8, `62498` | `624A5: CMP EAX,4000` (`3D 00 40 00 00`), then `624AA: JMP 62500` | Absolute heading difference is at least `4000` |
| Camera-relative path, `624AC` | After wrapping difference into `[-8000,8000]`, `624FE: TEST EAX,EAX` (`85 C0`) | Wrapped difference is nonnegative |

False adds `E000` to the target heading; true adds `2000`. Their low sixteen
bits differ by `4000`, a quarter turn. `62520` calls original `61430` with the
previous angle, that target, and maximum step `444`. The helper moves toward
the target over the shortest wrapped angular difference. `62525` stores the
result in the mask's `+178`; `62532` uses it to index the sine table `4785E0`
and subsequently updates position `+8C/+90/+94`. Therefore the defect can change
mask placement over time, rather than merely changing an unused temporary.

## Translator failure and discriminating reproduction

The pre-fix generated `recomp_0010.c`, `sub_00062240`, initializes `_flags=0` and
never assigns it. At `loc_00062500` it emits `if (_flags /* jge */)`.
`merge_flag_states` accepts mixed CMP/TEST predecessors only as `snapshot_zf`.
That carries JE/JNE correctly but cannot express SF/OF-based signed conditions.
Selecting either predecessor's operand interpretation unconditionally is wrong:
with EAX=1, TEST must take JGE while CMP against `4000` must not take it.

`tools/test_mixed_flag_paths.py` runs synthetic bytes through the actual
`FunctionTranslator` and compiles the emitted C with Clang/UBSan. It covers both
forward and backward predecessor placement, all four signed comparison jumps,
8/16/32-bit operands, zero/equality/threshold/sign/overflow edges, and MOV changes
to source registers after flag production. A separate synthetic function keeps
the original absolute-delta/wrapped-delta selection and target-angle additions;
an independent bounded-angle oracle checks target and smoothing consequences.
No game bytes or extracted assets are stored in the test.

Before any fix, the fixture executes **1194 checks with 651 mismatches**;
`local/reports/mixed-flag-paths-before.log` records the failing emitted native C.
This count includes the following independent arithmetic-ZF joins, not 651 game
bugs or an estimate of game-wide correctness.

## Related arithmetic joins

Original direct-call `66680` has `666D8: JNE` joining CMP16 with DEC16 at
`66726/66750`, and `667BA: JNE` joining TEST32 with DEC32 at `667F7`.
Original D3DX `11AE20` has `11B040: JNE` joining CMP at `11AF50` with
`11B03D: SUB EAX,8`. A result ZF captured on the executed predecessor is needed;
rereading the pre-decrement register or treating SUB operands as CMP is wrong.
The fixture covers each producer family, including sixteen-bit decrement wrap.
Original direct call graphs establish genuine code; this audit does not establish
whether a particular live frame executed these sites.

## Bounded correction and validation

The lifter now captures signed-less (`SF xor OF`) at each original CMP/TEST
producer in functions with a mixed signed join. CMP compares the already
sign-extended operands, avoiding subtraction overflow; TEST checks the sign of
the AND result at its original width. `snapshot_sz` capability supports JL/JGE
and JLE/JG through signed-less and ZF. Raw SF, raw OF and CF remain unproven by
that capability and receive no invented values.

Mixed DEC/SUB joins intersect capabilities down to `snapshot_zf`: the operation
computes its width-masked result once, captures equality with zero, and writes
the destination. Later MOVs cannot change that saved condition. DEC does not
modify the carry snapshot. Calls, unknown producers, variable shifts, and rotate
partial flags gain no additional provenance. A known immediate shift count of
zero continues to preserve its predecessor's flags.

The final fixture adds actual DEC16 memory writes: **1236 checks, zero
mismatches**, Clang/UBSan. Both original synthetic angle paths and independent
target/smoothing checks pass. Existing `test_flag_merge.py`, `test_lifter.py`,
`test_logical_shift_widths.py`, `test_shift_rotate_widths.py`,
`test_double_shifts.py` and `test_carry_rotates.py` pass. Merge tests additionally
reject signed predicates through DEC and joins with variable shifts, rotates,
or BT, while accepting the preserving constant-zero-shift case.

Read-only original-function translation against commit `0367fab` inspected
4546 functions with the same pipeline/options. Conditional fallbacks decreased
**51 to 47**, removing exactly `62500`, `666D8`, `667BA`, and `11B040` with no
new fallback sites or per-function increases. Entire original functions
`62240`, `66680`, and `11AE20` now have zero such fallbacks; `62500` emits
`if (!_slt)` from the actual executed CMP/TEST snapshots. The untouched original
`61430` call and position-update instructions follow it. Counts include analyzed
overlapping aliases and are not a whole-game correctness percentage.

Logs: `local/reports/mixed-final-test_mixed_flag_paths.log`,
`mixed-final-test_flag_merge.log`, `mixed-flag-test_*.log`,
`mixed-flag-impact.log`, and `mixed-original-functions-after.c`. Full game
regeneration/build and live behavior remain root-owned integration work.
