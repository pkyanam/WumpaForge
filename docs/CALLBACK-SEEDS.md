# Evidenced callback entries added to AOT discovery

## Boot30: emission callback0x31E20

After native audio streaming released the intro hold, the parent observed original
animation advance from300 to347.5. The next actual dispatch failure was0x31E20,
from `34820 → 34C40 → 34CA0 → 79060 → 7A310 → 7AB90 → 7AC40 → 7ACC0 → 2D950`.
This is a missing original callback entry, not an alias for another implementation.

Read-only inspection of the supplied extracted `local/assets/default.xbe`, using
its section virtual/raw mappings from `local/reports/default_analysis.json`, gives
DWORD at guest0x199C1C =0x31E20. The adjacent table values start0x32730,0x32600,
0x32890,0x329E0,0x32740,0x31FD0,0x321B0,0x323E0,0x33A90,0x33E20. Only the observed
missing entry0x31E20 was added to `config/seed-functions.json` in this change.

Original registration and call path (`local/reports/disasm/asm/text.asm`):

- 348D0 sign-extends byte[EBP+0x20],348D4 reads the function pointer from
  `0x199C1C + type*4`, and348DF stores it at `[EBX+0x4A8]`.
- 34A80 pushes EBP,34A81 pushes EBX, and34A82 calls `[EBX+0x4A8]`;34A88 removes
  eight bytes. This establishes two cdecl arguments in order `(pool, definition)`.
- 31E20 starts after the previous function's RET31E1D plus two alignment NOPs.
  It reserves12 local bytes, reads the first stack argument into EBX at31E24 and
  the second into ESI at31E6F. It accesses a ring of32-byte records through the
  first argument and properties through the second. “Emission callback” and
  argument labels describe those uses; original symbols are unavailable.
- 31FBA returns the selected record pointer from EDI in EAX, restores saved
  registers/local storage and returns at31FC2, followed by alignment NOPs.
- Existing thunk32730 consists of an unconditional jump to31E20, providing
  independent executable-target evidence; it does not replace the callback body.

No original instructions were skipped and no native return value was fabricated.
The parent owns the next analysis, lift, build and game validation. This audit
performed only XBE/source reads and the seed/document edits.
