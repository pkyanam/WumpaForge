# Snow renderer performance: story23/build61

The live game reached Arctic Antics, initially the original attract/demo route.
No user input, debugger attach, pause, extra game launch, or game timing change
was performed by this audit. Parent owned the live run and window/filter testing.

For the30 measured60-frame windows ending at swaps6139..7879, the median is
42.35FPS (range34.12..54.86), work wall23.304ms, work CPU18.328ms, Present0.311ms,
upload1.781ms, and397.8 draws/frame. CopyRects costs1.761ms and resolves3.758ms.
Readback4.354ms is nested in those operations and must not be added again.
Present itself is small; these measurements do not support lowering upscale
quality as the principal snow performance fix. Changing demo geometry and user
window actions prevent treating the range as a controlled filter comparison.

The seven128x128 target resolves each frame still round-trip through original
swizzled format6 memory. Actual changed uploads total roughly105MiB/60 frames,
with each128 texture upload about0.225ms, one uploaded twice/frame. The direct
linear BGRA8 path is already present. Future byte-identical Morton conversion
work could reduce this CPU cost, while GPU authority/coherence changes would
need a separate audit before dropping any guest-visible reads or writes.

## Measured registry scan

A single bounded OS sample used `sample 64628 2 10` (two seconds, 10ms sampling),
without attaching or stopping the debugger.
`local/reports/story23-snow-sample.txt` contains167 main-thread samples. Collapsed
self samples include draw_vertices_data_fetch21, indexed bridge15, shader_draw11,
memmove10, memcmp9. Synchronous GPU readback is another visible cost. These are
small statistical counts, not exact millisecond attribution.

Read-only ARM64 disassembly confirms the frequently sampled PCs10000B7CC and
10000B7D8 are the resource registry loop's handle load and index increment. This
particular instance is inlined into vertex_array_commit: it scans allocated
pages separately for declared input attributes, often requesting the same bound
vertex buffer repeatedly. Other draw, index, texture and shader paths call the
same resource lookup. The source-line table alone misleadingly points at the
nearby texture loop; actual instructions establish the cause.

Evidence files are ignored `story23-resource-scan-arm64.txt` and
`story23-snow-sample-command.log`. No further samples were taken.

## Bounded exact lookup cache

A512-entry direct-mapped pointer cache adds4KiB on ARM64. Resource pages already
never move or disappear. A hit returns the pointer only if its current handle
exactly equals the requested nonzero handle. Release clears that field; recycling
a slot must match its new handle. Reusing the same guest handle in the same slot
correctly identifies the current allocation. This lookup does not replace the
separate retained-vertex generation check, add references, or resurrect released
resources. Collisions and stale pointers fall back to the original bounded page
scan. Unknown handles are not cached, so subsequent registration remains visible.

No per-lookup timing or production counters were added. Component-only counters
prove10000 repeated warm lookups require zero scans. The actual resource API test
also covers deliberate hash collisions, unknown/NULL handles, release invalidation,
slot reuse,2046 real index resources, stable pointers across page growth, and
bounded exhaustion. Full combined GPU smoke passed on the first run:
`local/reports/resource-cache-build.log`, `resource-cache-smoke.log`.

Production was frozen before parent build62. This change removes measured work;
only the next live run can establish FPS improvement or a remaining60FPS gap.
