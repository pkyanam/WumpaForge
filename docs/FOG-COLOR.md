# Original SDK fog-color boundary

Story02's180-second watchdog found the main thread in original `103370`, waiting
for the GPU cache-flush bit10000 at MMIO+100410 to clear. The native call chain
was `103330 ->103420 ->1035C0 ->103740 ->FDA80 ->3A830 ->A4270 ->9D3F0 ->87BF0`.
The newly compiled86BC0 loading worker had already exited with status0. This was
an unbridged SDK state setter reaching the old hardware push-buffer machinery,
not an unfinished worker or a new missing AOT function.

The supplied XBE's original disassembly in ignored
`local/reports/disasm/asm/D3D.asm`, functionFDA80 (FDA80..FDACC), establishes:

- one32-bit argument and `ret4`;
- a push-buffer packet for method2A8;
- swapping red/blue bytes of the ARGB argument for the hardware ABGR word;
- storing the original, unswapped argument at10EFF4 before return.

The original game wrapperA4270 invokes generic render-state3A830 with state77hex
(119decimal). That state dispatch selectsFDA80.10EFF4 is exactly
10EE18+119*4, the original render-state cache slot.

The native FDA80 manual bridge now preserves that cache update and the guest
argument cleanup, with no guest push-buffer or MMIO operation. Programmed draws
convert the cached ARGB to RGB for their existing `u_fogcolor` uniform; previously
that uniform was always black. This also matters when an explicit final combiner
uses the fog color register while distance fog is disabled. The pixel generator
correctly forbids fog as a general-combiner source.

This initial change implemented the color register only. The subsequent
programmable distance-fog implementation and remaining fixed-vertex boundary are
documented in NATIVE-FOG.md.
It does not claim linear/exponential fog, change the game timeline, clear hardware
wait bits, or skip original scene logic. Parent must lift after the new manual
function entry before the original direct calls resolve to the native bridge.

The focused `tools/test_fog_color.inc` extends the complete native GPU smoke. It
calls the actual guest FDA80 API twice with distinct nonblack colors, verifies
full ARGB cache preservation, stack cleanup, unchanged adjacent cache entries
and unchanged guest FIFO pointer, then reads matching RGB pixels from a legal
final-combiner fog source. Color alpha does not replace fog factor/fragment alpha.
The first synthetic fixture incorrectly used fog in a general combiner; the
existing decoder rejected it. Correcting the fixture to a legal final combiner
made all focused and prior graphics tests pass. Production code was unchanged
by that fixture correction. Builds were sequential with one compiler job.

Validation logs: ignored `local/reports/fog-color-build.log` and
`local/reports/fog-color-smoke.log`. The command is the graphics smoke command in
RESOURCE-REGISTRY.md with output `build/input/test_fog_color`. No game was launched
for this component task; root owns integration and the next original-game run.

Story02 also validates the resource paging change against actual retail loading:
its snapshot contains2947 live native resources in12 allocated pages (3072slots),
including2017 index buffers,493 vertex buffers,346 textures,80 palettes,9 surfaces,
1 depth surface and1 cube texture. This proves the old1024 table was too small for
a real level workload; it does not retrospectively identify every Boot45 fault.
