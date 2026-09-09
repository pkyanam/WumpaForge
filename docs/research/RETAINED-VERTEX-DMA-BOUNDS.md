# Retained vertex DMA bounds after Arctic Antics

Story23/build61 completed the actual Arctic Antics level and returned to the hub.
The stopped draw has a live retained attribute4 address, generation1106, owner
VB01924DF0, data022D4D80, requested size49152, baked offset44328, new stride24,
FLOAT3 elements and literal indices0..325. The requested physical span is
022DFAA8 through022E192C exclusive. It crosses the VB's requested end022E0D80 by
2988 bytes, while remaining entirely within the title's64MiB guest RAM.

## Original hardware-facing contract

Original104160 initializes NV097 context DMA bindings. Its packet at10418C
starts method190; DWORDs at1041A8 and1041AB supply handle3 to methods19C and1A0,
DMA_VERTEX_A and DMA_VERTEX_B. Both are shared device contexts, not a separate
context for each D3D vertex-buffer allocation.

Original108F40 writes per-attribute FORMAT/stride and OFFSET methods. Indexed
address construction109091..1090A7 adds resource Data, declaration offset,
stream offset and original baseVertex*stride. A NULL stream branches around
that address write at10908F, retaining the physical address already programmed.
There is no resource-size operand in those packets.

Original100D70 allocates exactly the requested bytes through
MmAllocateContiguousMemoryEx at100D93, with PAGE_READWRITE|PAGE_WRITECOMBINE404,
alignment0 and highest acceptable physical address07FFFFFF. At100DB0 it masks
the resulting Data pointer to27 bits. The separate D3D resource header is12 bytes.
Here49152 is exactly12 Xbox4KiB pages: page rounding cannot explain the2988-byte
overrun. The native bridge's requested-size metadata is not an NV2A limit.

## Independent primary implementation evidence

Pinned xemu revision `fdfb5a8f481b2f870c57080e74ec8d3a31a47053`:

- [GL vertex fetch](https://github.com/xemu-project/xemu/blob/fdfb5a8f481b2f870c57080e74ec8d3a31a47053/hw/xbox/nv2a/pgraph/gl/vertex.c):
  pgraph_gl_bind_vertex_attributes maps selected DMA context, validates initial
  attribute offset against its length, computes physical start plus indexed
  stride, and references one VRAM-sized GL buffer. update_memory_buffer checks
  the resulting range against the VRAM region. It has no D3D resource-size input.
- [NV2A DMA and memory](https://github.com/xemu-project/xemu/blob/fdfb5a8f481b2f870c57080e74ec8d3a31a47053/hw/xbox/nv2a/nv2a.c):
  nv_dma_load derives address/limit from the DMA object; nv_dma_map resolves its
  physical address into VRAM. nv2a_init_memory makes VRAM the Xbox RAM region.
  The implementation retains TODOs around DMA classes/targets and full-limit
  validation, so it is not evidence to accept arbitrary addresses or wrapping.
- [Attribute methods](https://github.com/xemu-project/xemu/blob/fdfb5a8f481b2f870c57080e74ec8d3a31a47053/hw/xbox/nv2a/pgraph/pgraph.c):
  SET_VERTEX_DATA_ARRAY_OFFSET stores DMA selection from bit31 and offset from
  bits0..30. FORMAT carries type, count and stride. No allocation length is set.

Reference files are ignored under local/reports/research/xemu-vertex-dma.c and
xemu-dma-*. This was a read-only audit; no reference code was copied into runtime.

## Narrow implementation recommendation and limits

For the reached retained-address path, keep verified history, live resource owner,
allocation generation, type, format and stride checks. Replace the per-resource
end restriction with a checked physical guest-RAM interval. Compute start/end in
64-bit arithmetic, require each complete element within the64MiB backing, and
never apply a modulo mask that could wrap an overflow. Keep the protected low
CPU-mapping range rejected: macOS null-page protection may cover16KiB, so the
existing mapped title area beginning10000 is a conservative host-readable floor.
The reached interval is far above that area.

Read the actual adjacent guest bytes. Do not allocate invented padding, clamp
indices, reset the original base, substitute attributes, or synthesize zero data.
Tests should place distinguishable real bytes beyond the originating resource's
requested size, verify GPU output changes when those bytes change, and reject
RAM-end crossing/overflow, expired owner generation and absent address history.

This corrects a compatibility check that was stricter than the GPU contract.
It does not establish that the game's tangent producer is correct, nor that
adjacent bytes match their contents under the original Xbox allocator. The port
uses its own bounded guest allocation layout. Live validation must still inspect
hub reflection quality. Hardware may retain physical addresses even after an
allocation is released, but that broader case is neither reached nor authorized
by this bounded recommendation; the existing generation guard remains.
