# Opt-in original-index shader submission

`WRATH_INDEXED_SHADER=1` enables the Android-only candidate in
`title-zzzzzzzzz-indexed-shader.patch`. Default remains the existing expansion.
Root owns the corresponding device marker and physical validation.

The original `101BC0` index validation still runs before selection. For triangle
lists using an actual vertex shader and no nonempty secondary/tessellation slots,
selection compares contiguous stream0 bytes plus uint16 indices against the old
expanded vertex bytes. Only equal-or-smaller uploads qualify. Original source
begins at stream offset plus base vertex times stride; raw indices remain unchanged.
The shader submits those indices with `glDrawElements`. Existing shader RPC passes
the additional `VertexFetch` field synchronously, preserving pointer lifetime.

No resource contents are cached. Every selected draw uploads current vertex/index
bytes, including writes through retained guest pointers. The shader VAO's prior
EBO binding and supported primitive-restart enables are restored. Fixed-index
restart is queried only with GL 4.3+ or exact ARB_ES3_compatibility support;
the immutable context capability is cached once. The actual Shield requests GL 4.1. Index 65535 remains
an ordinary Xbox index. Quads, secondary and retained streams, fixed rendering,
sparse ranges, disabled mode and uncertain declarations keep the established path.
The original physical vertex-array commit and resource/index bounds remain intact.

Host command: `python3 android/shield2019/tests/test_indexed_shader.py`.
It replays the actual patch under the ignored build directory and passes 10,000
original-expansion equivalence cases under ASan/UBSan plus opt-in/fallback checks.

The graphics component fixture is included in the patch and executes after the
existing actual shader setup. It requires three explicit optimized hits, nonblank
full-region pixel equality with baseline, nonzero base/offset, vertex/index alias
mutation, restart-state preservation, subsequent nonindexed drawing, bounds and
release checks. Its source is also retained at `tests/indexed_shader_gpu.inc`.
Physical execution is pending; this document makes no GPU or performance claim.

Compare the same hub scene with the mode off/on after correctness passes. Record
selected draw coverage, CPU work, frame intervals and actual upload bytes. This
candidate removes primary CPU expansion and permits indexed GPU vertex reuse;
it does not remove `FFEA0` binding work or solve all secondary-stream gathers.
