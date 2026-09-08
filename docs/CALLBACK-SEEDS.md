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

## Complete original emission table (after boot31)

A bounded follow-up audit found nine other real callbacks missing from the current
function map. They are now seeded together, so later assets can select their
original implementations without requiring one discovery/build cycle per callback.
This change adds entry discovery only; it does not replace any callback logic.

The first table occupies `0x199C1C..0x199C47`: eleven DWORD pointers. The next
independently addressed table begins at `0x199C48`, selected from the distinct
`definition+0x21` byte at `0x333EF/0x333F9` and `0x348E5/0x348E9`, and stored at
`pool+0x4AC`. The first table uses `definition+0x20` and `pool+0x4A8` at
`0x333D5/0x333DF/0x333E6` and `0x348D0/0x348D4/0x348DF`. These distinct accesses
establish the boundary rather than merely scanning a run of executable addresses.

Original editor code beginning at `0x57590` independently offers first-table
indices 0, 6, 7, 8, 9, 10. The index values are passed to `0x5C490` at
`0x57638`, `0x57669`, `0x5769D`, `0x576CE`, `0x57702`, `0x57733`, with callback
`0x52DB0`. That callback writes the selected index into `definition+0x20` at
`0x52E07`, then reloads the first table at `0x52E1C`. String arguments in the
supplied XBE label these selections Normal, Radial, Radial Rotor, Spheroid,
BounceY, and BounceXZ. This confirms the final entry (index 10) is intentional.
Indices 1–5 are initialized table entries even though this particular editor menu
does not expose them. The consumers sign-extend their index bytes and do not
clamp them: 0–10 is the established table extent, not a claimed runtime bounds
check. Corrupted indices remain a separate issue.

All callbacks share the original two-argument cdecl dispatch at `0x34A82`, described
above. Each new entry has an independent aligned prologue and reads both stack
arguments after accounting for local storage/saved registers. Each restores its
frame, returns the selected record pointer in EAX, and executes a plain RET.
Addresses below refer to original instructions, not generated C.

| Index | Table DWORD | Callback | First/second argument loads | Final RET | Map before batch |
| --- | --- | --- | --- | --- | --- |
| 0 | `0x199C1C` | `0x31E20` | `0x31E24` / `0x31E6F` | `0x31FC2` | Already seeded |
| 1 | `0x199C20` | `0x32730` | Tail jump to `0x31E20` | In target | Already discovered |
| 2 | `0x199C24` | `0x32600` | `0x32601` / `0x3264B` | `0x3272E` | Missing; now seeded |
| 3 | `0x199C28` | `0x32890` | `0x32891` / `0x328DB` | `0x329D1` | Missing; now seeded |
| 4 | `0x199C2C` | `0x329E0` | `0x329E1` / `0x32A2B` | `0x32AC9` | Missing; now seeded |
| 5 | `0x199C30` | `0x32740` | `0x32741` / `0x32785` | `0x32887` | Missing; now seeded |
| 6 | `0x199C34` | `0x31FD0` | `0x31FD4` / `0x32025` | `0x321AC` | Missing; now seeded |
| 7 | `0x199C38` | `0x321B0` | `0x321B7` / `0x321FF` | `0x323DE` | Missing; now seeded |
| 8 | `0x199C3C` | `0x323E0` | `0x323E4` / `0x3242F` | `0x325FB` | Missing; now seeded |
| 9 | `0x199C40` | `0x33A90` | `0x33A94` / `0x33AE6` | `0x33E11` | Missing; now seeded |
| 10 | `0x199C44` | `0x33E20` | `0x33E24` / `0x33E8E` | `0x34217` | Missing; now seeded |

`0x321B0` has an additional balanced return at `0x323C5`; its second return is
`0x323DE`. Entry `0x32740` follows the `0x32730` unconditional tail thunk and
alignment padding; other entries follow preceding return epilogues and padding.
The second table contains null at index 0 followed by `0x32AD0`, `0x32B20`,
`0x32B40`, `0x32B60`, `0x32B80`, `0x32AF0`. All six non-null addresses already
exist in the current function map, so no second-table seeds are added. Its next
DWORD, `0x199C64 = 1`, is independently used as a scalar gate by `0x34CD0` and
`0x352C0`, rather than being another callback.

Validation used section-mapped original XBE DWORDs, original disassembly entry and
epilogue reads, and exact-address membership in
`local/reports/disasm/functions.json`. A bounded Python check verified all nine
slots, absent map entries, unique seed addresses, and original RET opcodes.
No game analysis, lifting, compilation, or launch was performed for this batch;
parent validation of the newly compiled callbacks is still required.
