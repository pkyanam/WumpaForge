# Boot45 model read investigation

Boot45 faults in native memcpy/memmove called by compiled guest `97380` at
original `973C3` (REP MOVSD), while loading Level37/Demo0. Boot46 instead reaches
an unresolved worker entry `86BC0` with Level7/Demo1. These are different scene
loads; boot46 does not establish that the Level37 failure is nondeterministic.
No game launch, production edit, or additional compile was performed for this audit.

## Proven read and registry contract

The original assembly is in ignored `local/reports/disasm/asm/text.asm`;
generated functions are in `local/generated/recomp_0003.c`, `recomp_0014.c`,
`recomp_0015.c`, and `recomp_0017.c`.

- `A9680` loads primitive indices. At `A9714` it calls `3A400` with the
  primitive object's index handle at `EDI+14`. `A9719` passes returned EAX
  directly to `97B00` as destination, with byte length `EBX*2` and file handle ESI.
- `97B00` dispatches to `97620`, then `97380` for a virtual RAM stream.
  `97380` selects record `2594B8 + (handle-800)*32`, reads its owner's RAM base
  at owner+2C, adds current offset record+8, and copies the requested bytes.
  Original and generated REP-copy argument flow agree; no copy-instruction
  translation defect was found in this path.
- `3A140` stores an index resource pointer at `1BAC50 + zero_based_slot*24`.
  It returns slot+1. `3A400` reads `1BAC38 + one_based_handle*24`, then returns
  that resource header's Data field at +4. The apparent 24-byte discrepancy is
  correct. `3A220` releases the corresponding resource and maintains the list.
- `9A460` allocates the primitive and calls `3A140`; a creation failure returning
  zero can reach the later lookup without a guard. Never bypass this failure or
  replace the destination with a fabricated buffer.

## Highest-priority hypothesis: native resource capacity

`src/graphics.c` has a native resource table of only 1024 entries. `new_resource`
returns null when occupied; that same table holds textures, palettes, surfaces,
vertex buffers, and index buffers. Boot45 reports eleven unsupported 32x32
textures in formats B/F immediately before the invalid Release stream. Both
formats are actually supported (P8 and DXT5). The generic message also covers
resource-slot exhaustion and graphics-thread acquisition failure. Guest heap
usage remains about 37 MB out of 53.875 MiB, so guest RAM exhaustion is not shown.

Capture the native table's live count and counts by type/references/bindings at
first `new_resource` exhaustion. This can directly distinguish an artificial
native slot cap from a lifetime leak. It is not yet a proven diagnosis.

The SDK entry `arg()` implicitly acquires the graphics lock and `finish()`
releases it. Texture/VertexBuffer/IndexBuffer/Palette creation and Release all
read arguments before touching this table, so inspection does **not** establish
a registry race. An earlier intermediate report missed this implicit lock and
was withdrawn. Failed acquisition aborts inside `arg()`, further narrowing the
later generic texture-error candidates. No concurrency change is proposed.

## Exact next diagnostic fields

At the first invalid Release, record guest return `[ESP]`, eight stack DWORDs,
EAX/ECX/EDX/EBX/ESI/EDI/EBP, handle, and its first24 bytes if within guest RAM.
Root's build46 diagnostic already records these. The alternating small handles
33,1,32,1,... suggest inspecting the only adjacent pair of Release call sites:
vertex registry cleanup `39FE0`, returns `3A015` and `3A02D`. It reads pointers
at `1E8D84 + 32*(handle-1)` and `1E8D88 + 32*(handle-1)` respectively. Capture
32 bytes at `1E8D78+ESI` there (ESI is already the byte offset), plus list head
`1E3C88` and last-used `1E8CB0`. Index-pool Release instead returns `3A244`.
This caller identification is a hypothesis until a Level37 run reproduces it.

At `A9719`, capture EAX destination, primitive EDI and `[EDI+14]` index handle,
EBX index count, ESI stream handle, the24-byte index registry record, and the
pointed resource header. At failing `973C3`, capture EDI destination, ESI source,
ECX DWORD count, EAX byte request, and the32-byte virtual-file record at EDX.
These captures connect the original bad pointer to the first allocation or
lifetime failure instead of treating the final memcpy fault as the root cause.
