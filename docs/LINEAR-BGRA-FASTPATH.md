# Linear BGRA8 texture and surface fast path

Story17 attributed approximately3.9ms/frame to two uploads of a640×480
format0x12 screen texture, within roughly16ms/frame of upload/copy/resolve work.
The existing format conversion functions already establish that LIN_A8R8G8B8
format0x12 is byte-identical to native BGRA/UNSIGNED_INT_8_8_8_8_REV pixels.
The old path nevertheless decoded and encoded each pixel through dynamic format
selection. This change removes that redundant work without changing ownership.

## Implementation

`upload_texture_images` still compares exact guest bytes and captures the same
bounded shadow. For format0x12 it uploads directly from that captured byte image
(or the existing guest-pointer fallback when the shadow budget is full), using
GL_UNPACK_ROW_LENGTH=guest_pitch/4. It no longer allocates or fills a second
width×height RGBA staging array for this format. All saved unpack state and PBO
binding are restored. Other formats retain their existing decoder and mip paths;
the existing linear-resource creation rule still permits one level only.

CPU surface reads, render-target resolves, and CopyRects into format0x12 use
row memcpy with explicit source and destination pitches. Only visible pixels
are copied; pitch padding remains untouched. Source pixels are still captured
before CopyRects destination writes, preserving overlapping-copy behavior.
Existing rectangle validation, GPU/CPU source selection, target switching,
parent invalidation, and guest CPU visibility are retained.

BGRA8 GPU readback and target preparation reverse row order using a fixed1KiB
scratch array and chunked memcpy instead of swapping each pixel individually.
Odd widths, widths larger than the scratch chunk, and odd heights retain the
same top-down guest/bottom-up GL relationship. No additional full-surface
allocation, deferred copy, GPU-authoritative resource model, or RAM budget
increase is introduced. Other surface formats keep their previous conversion.

## Native GPU validation

`tools/test_linear_bgra.inc` constructs actual synthetic guest texture/surface
objects through the native bridges. Its259×5 texture crosses the row-flip chunk
boundary, has an odd center row, and includes64-byte row alignment padding.
Per-pixel colors and nonopaque alpha verify native texture upload bytes. It tests:

- Foreign unpack row length, alignment, skips, byte swap and PBO restoration.
- Direct retained guest-pointer mutations after a clean upload.
- Unchanged-byte upload reuse and full-shadow-budget fallback.
- CopyRects with nonzero source/destination origins and untouched surroundings.
- Actual GPU target preparation, readback and resolve with distinct top/bottom
  pixels, full alpha, and unchanged guest row padding.
- Foreign pack row length, alignment and skips restored after readback.
- Upload of resolved pixels, subsequent reuse, and normal release lifetimes.

The complete native GPU suite passed first try, including all earlier swizzled,
mip, cube, palette, fog, reflection, vertex, index, copy and lifetime regressions.
One compiler job; ignored reports `local/reports/linear-bgra-build.log` and
`linear-bgra-smoke.log`. No full game was launched by this task. Production
sources were frozen before parent notification; measured live hub FPS remains
the next validation, rather than inferred from component correctness.
