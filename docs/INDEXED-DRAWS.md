# Native indexed drawing

Boot21's loading worker holds the game's graphics critical section while executing
`101BC0` through the original GPU pushbuffer routines. Main then waits for that
critical section. `local/reports/boot-21-stall.log` records both stacks. The native
indexed path replaces those identified SDK operations with host GPU drawing.

The original XDK4361 routines establish these interfaces:

- `100D00`: CreateIndexBuffer(length, usage, format, pool, out), ret20. A contiguous
  length+12 allocation has Common `01010001`, Data=header+12 and Lock=0. The retail
  routine ignores usage, format and pool entirely; the game passes2C while its
  D3DX111CC7 passes65. Both produce the same16-bit index storage.
- `FFEA0`: SetIndices(buffer, baseVertex), ret8. Binding adds `80000` to Common,
  maintains resource lifetime, and writes device+38C, device+1C and `10EC0C`.
- `101BC0`: DrawIndexedVertices(type, indexCount, uint16Pointer), ret12. The third
  argument is a literal pointer, including offsets into a bound index buffer or
  ordinary guest memory. Original helper `108F40` applies baseVertex to stream
  addresses before fetching attributes.
- `100D40`: Shared palette/index-buffer Lock(handle, outData, flags), ret12.
  The palette implementation owns this shared export. Native synchronous draws
  copy guest data, so the original GPU resource-idle wait is unnecessary.

`src/index_bridge.inc` validates index and vertex ranges with widened arithmetic,
expands actual INDEX16 fetches into a bounded temporary host vertex array, then
uses the common native fixed/programmed draw path. Base vertex is applied once.
The allocation is freed after drawing; no interpreter, CPU JIT or fabricated
game geometry is involved. The current path supports stream0 and up to64MiB of
expanded data; it diagnoses unsupported layouts instead of silently skipping.

`tools/test_index_bridge.inc` is included by the graphics smoke build. It verifies
an actual GPU green quad using base1 and a literal index pointer offset, with a
poison vertex/index that would reveal an incorrect fetch. It also exercises raw
indices without a bound index buffer, buffer bounds, base overflow, Common words,
shared locking and binding lifetime. Test execution and full-game progress are
recorded in `docs/STATUS.md`; this implementation alone does not prove a menu.


## D3DX mesh constructor failure (boots43–44)

The first demo-level transition failed at original3B460, a call through a mesh
vtable+38. Boot44 captured an empty mesh registry (free_head0, used_count0,
used_headFFFFFFFF), while the original vtables169EB0/169F20 were intact. D3DX
constructor115D76 calls111CC7, which passes format65 at111D21 to100D00. Our
former2C-only validation returned INVALIDCALL;3B180 returned mesh handle0 and the
game subsequently indexed its pool with handle−1, producing a null method call.

Original100D00..100D3E reads only `[esp+4]` length and `[esp+14h]` output; it never
reads usage/format/pool. Removing the invented format restriction preserves that
contract; it does not add32-bit index fetches. The regression now draws through
an index buffer constructed with the reached D3DX65 format and arbitrary ignored
usage/pool, verifies the original2C and other ignored format values produce the
same header, then checks their release lifetime. The combined native GPU smoke passes with the reached65 format and all prior
checks (`local/reports/mesh-index-smoke.log`). This source fix awaits the next
actual game run to confirm progress beyond the mesh constructor.
