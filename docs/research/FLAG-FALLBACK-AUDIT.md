# Remaining conditional-flag fallbacks after lift29

Original read-only audit, 2026-09-08; counts below describe lift29. The later
menu fix and its validation are recorded at the end. Full per-site inventory is [FLAG-FALLBACK-INVENTORY.csv](FLAG-FALLBACK-INVENTORY.csv);
expanded assignment evidence is in `local/reports/flag-fallbacks-after-lift29.json`.
Addresses in the CSV's block column identify the nearest generated basic-block
label, not necessarily the exact branch instruction when data follows a jump.

## What remains

The safe comparison merge reduced70 emitted sites across62 functions to57 sites
across50 functions. These have48 distinct nearest block labels; duplicate generated
ownership accounts for some repeated sites. In49 functions (55 sites), `_flags`
has **only its zero initializer**. Those emitted branch predicates are always
false if reached. `sub_6E130` is the sole exception: three string-compare loops
assign `_flags=(MEM8(esi)==MEM8(edi))`. Its two unrelated fallback JE sites can
consume stale string equality, so they cannot be described simply as always false.
No x86 flags are otherwise materialized into `_flags` in these50 functions.

Raw site count is not a reachable-defect count. Thirty-one sites belong to
functions marked `tail_jump_alias` by discovery. Several apparent branches are
actually jump-table/data bytes decoded after an unconditional transfer. Do not
add flags behavior to bytes until their execution provenance is established.

## Priority1: current intro input/state handling

`sub_6E130` is called directly from the currently executed intro/cutscene loop
`2D950` at original `2DB49` when current intro selector is0. This proves the
containing path is relevant; it does **not** prove either particular branch ran
in boot28/29. The remaining branch sites are:

| Branch | Predecessor1 | Predecessor2 | Gap |
| --- | --- | --- | --- |
| `6ECA4: JE 70B5E` | `6ECA0: CMP [ESP+0x10],EAX` | `6F094: MOV EAX,[ESP+0x10]`; `6F098: TEST EAX,EAX`; `6F09A: JMP 6ECA4` | CMP/TEST mixed semantics and backward predecessor. |
| `7012A: JE 70B5E` | `70126: CMP [ESP+0x10],EDI` | `70791: MOV EAX,[ESP+0x10]`; `70795: TEST EAX,EAX`; `70797: JMP 7012A` | Same two gaps. |

These branches control whether state-changing calls are skipped. They currently
read either initialized0 or equality from unrelated prior string comparisons.
The generated alias `sub_700B5` contains a duplicate7012A with only initialized0.
The comparison-only merge correctly refused to equate CMP and TEST: their two
snapshot operands have different interpretations. Retaining whichever predecessor
happens to be emitted first would silently miscompile the other path.

Recommended next evidence: a diagnostic at either reachable fallback reporting
original branch address, function entry, guest EIP-equivalent block, GPRs/ESP,
`_fa/_fb/_fas/_fbs`, and the static predecessor flag-setter descriptions. A
controlled halt is more useful than continuing with fabricated flags. Since the
current constant-false branch can be optimized away, a debugger breakpoint on
its emitted line may not reliably establish execution; an explicit diagnostic
before it is the reliable future implementation boundary.

## Priority2: useful small extensions and SDK/math sites

| Site | Original evidence | Bounded faithful follow-up |
| --- | --- | --- |
| `F2173: JE F227C` in `F2122` | Initial `F2170` and backward predecessor `F225D` both execute exactly `CMP [EBP+0x10],ESI`; `F2260` jumps back. | An order-independent block flag-state prepass can resolve this without adding new flag arithmetic. Current one-pass address-order emission treats the later predecessor as unknown. Caller is `EDE72`; actual branch execution still needs a hit. |
| `62500: JGE 6250A` in `62240` | `624A5: CMP EAX,0x4000` jumps forward to join; other path `624FE: TEST EAX,EAX`. | Mixed CMP/TEST signed relation, not just ZF. Requires actual per-path condition semantics; do not reuse `_fas` comparison arbitrarily. Called through game state function `2B2D0`, not proven current intro hit. |
| `666D8: JNE 66798` in `66680` | Initial `CMP word[4678E8],0`; backward paths decrement that16-bit word at66726/66750. | Shared ZF representation or edge-specific predicates, with DEC width preserved. |
| `667BA: JNE 6683B` in `66680` | Initial TEST EAX,EAX; backward path `667F7: DEC dword[561290]`. | Same mixed-result ZF issue; cannot assume original EAX was refreshed after decrement. |
| `77061: JNE 7706C` in `76A80` | `7704D: TEST BL,0x8C` versus `7705C: CMP word[ESI+0x24],0x53`. | Mixed ZF with different widths. A per-path ZF bit is sufficient; operand-width equality merge is intentionally insufficient. |
| `121829: JE 12177F` in D3DX `121717` | CMP32 `[EBP+0x10],EDI` versus TEST8 AL,2 / AL,4. | Same per-path ZF case in SDK code. Static callers121ABE; runtime reachability unverified. |
| `11B040: JNE 11AE43` in D3DX `11AE20` | `11AF50: CMP [ESI+0x34],0` versus `11B03D: SUB EAX,8`. | Per-path ZF after comparison versus arithmetic result. Do not reread changed operands as though both were CMP snapshots. |
| `F52DA: JE F5349` in `F52D5` | Real call-entry flags: callerF52B8 callsF8A30, thenF52D5; F8A30 masks the double exponent and compares to0x7FF00000, returning without flag changes. F52D5's PUSH/WAIT/FNSTCW preserve those flags. | Interprocedural flags, not a CFG join. A bounded native math helper or verified carry-through of this actual helper contract is needed; local merge cannot solve it. The branch recognizes nonfinite exponent cases, so its always-false predicate is not evidence that ordinary finite camera math is already wrong. |

A low-risk next structural improvement is to determine each block's terminal
flag definition independently of code-emission order, then propagate compatible
states over the CFG until stable. Treat unknown/clobbering instructions and real
entry edges conservatively; loops do not automatically imply flags are known.
This resolves same-kind back edges such asF2173 without changing the meaning of
any setter. A separate small extension could materialize only the **ZF bit** in
functions with verified mixed-ZF joins, at each actual relevant flag-writing
instruction. That requires tests for both incoming paths, widths, preserving
instructions, and partial-flag cases; it should not turn a whole mixed state into
one guessed CMP. No such extension was implemented in this audit.

## Lower priority until execution/boundary evidence

- Eight generated owners contain a fallback associated with block31407, after
  original `3140A: JMP 2DD30`. The apparent later JL comes from decoding bytes
  past this unconditional transfer. Do not count all eight as distinct game bugs.
- `sub_3CFE0` has a fallback after `3E360: JMP 3E163`, immediately before the
  jump table whose base is3E368 (used at3D077). The allocation-loading function
  has been seen in real stacks, but that does not make its embedded table code.
- Three JS sites in `sub_E77D0` lie in its table beginningE7890, used by original
  indirect jumpE77E4. Their byte-memory ADD/JS sequences are table disassembly,
  not established executable sign tests.
- Both `sub_F7365` and `sub_F736C` contain a fallback after an unconditional
  generated transfer in blockF7459; inspect ownership and original table ranges
  before adding flags support.
- Entry aliases such as11111,20005,20018,20021,2003A,2005D,200AA,200BB,4007D,
  400B0,600B5,700B5,B20D0,C00B4,F0012,F6C61,100041..100043,101101 and10FFFF
  begin with live-flags-consuming code or partial frames. Some may be legitimate
  intra-function entry points; others may be erroneous decoded immediate targets.
  A dispatcher listing alone establishes neither. Capture actual call/jump source
  and validate original instruction boundaries before fixing their conditions.
- 417C4/B85FE/946080 show implausible decoded memory operands, dead targets, or
  earlier unsupported instructions. Their provenance needs verification first.
- Remaining ordinary forward/join sites13749,14D08,14D69,74147,15797C are recorded
  in the CSV. They deserve predecessor-semantic analysis once a runtime hit or
  relevant static path establishes priority; no guessed predicate is proposed.

## Diagnostic policy proposal

For a future diagnostic build, replace only unresolved flag-condition fallbacks
with the existing explicit unsupported-instruction/error path at the original
branch instruction address. Preserve known conditions. This converts reached
silent corruption into an actionable bounded failure; it does not implement
missing CPU behavior or claim unsupported byte streams are valid code. Keep the
static inventory distinct from runtime hits, and record source ownership/table
provenance before deciding between a lifter fix, recovered function boundary or
native SDK compatibility helper. Root owns the implementation decision and STATUS.


## Implemented: captured mixed zero flags and backward predecessors

A later focused audit reconfirmed original6ECA4 and7012A. Their CMP predecessors
compare the local atESP+10 against a register proven zero (EAX at6EC36, EDI at700AE).
The backward predecessors TEST that same local. Its final assignment at6E2DA is
debounced input bits masked with120. Both branches must skip a menu transition
when that back/cancel input is zero. Original menu switch70B7C maps menu0D to
6F094, menu0F to6EC36, menu25 to700A3;7012A also serves cancellation paths from
menus16,19,1D. This proves the semantics, not a specific in-game branch hit.

The implementation improves AOT analysis generally; it contains no title-address
or menu predicate override:

- A shared pure instruction flag-transfer function establishes terminal setters
  before code emission and propagates preserving-block state to a fixed point.
  Backward predecessors are therefore independent of source address order.
  The CFG includes known switch destinations. Unknown entry, clobber and call
  paths remain unknown; circular preservation does not create an initial value.
  CALL and POPFD no longer inherit an unverified caller flag state. This can
  reveal additional unresolved legacy paths after a full lift; no arbitrary
  interprocedural EFLAGS-return contract is assumed.
- Compatible same-kind/width snapshots retain their existing comparisons. A join
  of known CMP/TEST states with different semantics or widths meets at ZF only.
  JE/JNE and corresponding equality SETcc/CMOVcc can consume that bit; other
  mixed condition bits remain unresolved.
- Only functions containing such joins materialize a dedicated `_zf` at each
  original CMP/TEST, using its already width-masked captured operands. Later
  MOV/register/memory mutation cannot change the recorded condition. The bit
  is separate from the legacy string-loop `_flags` temporary.

`tools/test_flag_merge.py` executes generated ARM64 C under UBSan: both incoming
paths, backward TEST, the two original stack-memory CMP forms, source register
mutation after the setter, equal/non-equal and zero/nonzero inputs, byte/word
widths, and unknown/clobber rejection. Existing signed32 and exhaustive byte-SF
regressions pass. Lifter diagnostics, incremental chunks and x87 rounding tests
also pass. Translating only original6E130 to ignored
`local/reports/menu-flags-fixed.c` confirms both branches now use `_zf` and that
function has no remaining `if (_flags` conditions. No whole-game lift/build/run
was performed for this handoff; aggregate fallback counts and actual controller
menu behavior await root integration and validation.
