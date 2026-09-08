# Native indexed drawing

Boot21's loading worker holds the game's graphics critical section while executing
`101BC0` through the original GPU pushbuffer routines. Main then waits for that
critical section. `local/reports/boot-21-stall.log` records both stacks. The native
indexed path replaces those identified SDK operations with host GPU drawing.

The original XDK4361 routines establish these interfaces:

- `100D00`: CreateIndexBuffer(length, usage, format, pool, out), ret20. A contiguous
  length+12 allocation has Common `01010001`, Data=header+12 and Lock=0. This title
  uses INDEX16 (`2C`); the native bridge explicitly rejects other formats.
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
