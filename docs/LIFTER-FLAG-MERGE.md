# Compatible comparison flags at control-flow joins

Boot28's original wave-file parser at0x13849F rejected valid RIFF/WAVE input.
At0x138552 it compares `[EBP-4]` with0x45564157 (WAVE), then jumps to0x138567.
The alternate predecessor compares `[EBP-4]` with ECX at0x138564 and falls into
the same JE. Neither predecessor clobbers the comparison flags. Original branch
0x138567 goes to0x13856E on equality; the lifted C instead tested `_flags`, a
local initialized0 and never assigned in that function. It consequently reached
E_FAIL at0x138569 on the valid WAVE path. Source evidence is in
`local/reports/disasm/asm/DSOUND.asm` and old generated `recomp_0041.c`.

The translator's predecessor merge required exact equality of both mnemonic
and operand lists. That discarded two compatible CMP states just because their
right operands differed. The lifter already snapshots unsigned and signed
operand values into `_fa/_fb/_fas/_fbs` when each CMP/TEST executes. At the join,
these locals contain values from the predecessor actually taken.

`tools/recomp/lifter.py:merge_flag_states` now accepts differing operand identities
only for the same snapshot operation (CMP or TEST) and the same operand width.
Width must agree because sign-flag conditions still use a width-specific cast.
`translator.py` calls this helper after confirming every predecessor's output
state is available. Existing identical-state behavior is preserved. Missing,
mixed-operation, differing-width or unsupported-setter states do not acquire an
invented comparison. No game address, constant or forced parser success is part
of the implementation.

The actual translator/native regression is `tools/test_flag_merge.py`. It
constructs small synthetic x86 CFG byte fixtures, disassembles/lifts through the
real function translator, compiles one native C executable with Clang−O2 and
undefined-behavior sanitizer, then executes both predecessor paths and branch
outcomes. Coverage includes the stack-memory WAVE comparison shape, register
comparisons whose sources change before the join, signed32 ordering, TEST masks,
and exhaustive8-bit sign-result combinations. Separate checks reject incompatible
or unknown states, including an actual translator CMP/TEST mixed join. Generated
C for the supported joins contains no `_flags` branch.

Validation passed:

```sh
.venv/bin/python tools/test_flag_merge.py
.venv/bin/python tools/test_lifter.py
.venv/bin/python third_party/xboxrecomp/tools/recomp/test_carry_flag.py
# From third_party/xboxrecomp:
../../.venv/bin/python -m unittest tools.recomp.test_lifter_carry tools.recomp.test_lifter_bit_scan
```

Logs: `local/reports/flag-merge-test.log`, `lifter-diagnostics-test.log`,
`flag-merge-carry-test.log`, `flag-merge-upstream-tests.log`.

## Remaining risk and inventory

This bounded fix does not solve unknown EFLAGS, uncomputed back edges, mixed
operation joins, interprocedural flag propagation, or all x86 partial-flag rules.
The old unsupported `_flags` fallback still exists. It can silently produce the
wrong branch, so unresolved sites should be treated as correctness gaps; the old
translator comment claiming unknown fallback could never be wrong was corrected.
A full flag-dataflow redesign or broad materialized-EFLAGS implementation was
outside this patch. Same-width CMP/TEST joins elsewhere in the game will change
behavior when re-lifted and need bounded runtime validation.

Before the parent's next game lift, read-only inventory found70 fallback
conditional-jump sites across62 functions: JE26, JNE13, JL10, JGE4, JP4, JB3,
JNP3, JS3, and one each JLE/JNO/JAE/JBE. The per-function/block locations are
saved in `local/reports/flag-fallbacks-before-merge.json`. The WAVE join is covered
by the new merge rule; the full reduction must be measured after re-lifting.
A quick safe post-lift count is:

```sh
python3 - <<'PY'
from pathlib import Path
import re
n=sum(len(re.findall(r'if \(_flags /\* j\w+\b', p.read_text()))
      for p in Path('local/generated').glob('recomp_*.c'))
print(n)
PY
```

No full game analysis, lifting, build or launch was performed for this fix.
The parent coordinates the next lift and validates stream creation/progression.

Parent integration: lift29 corrects the actual0x138567 branch to
`CMP_EQ(_fa,_fb)`. Unknown conditional-jump fallbacks decrease from70 to57;
nine generated translation units changed. Remaining57 are unresolved correctness
gaps, not claims of correct EFLAGS behavior. Native runtime validation follows.
