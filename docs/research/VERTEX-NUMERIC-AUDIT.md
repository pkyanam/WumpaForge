# Bounded vertex numerical audit

The user's observed character/demo distortion motivated a source review and
synthetic native GPU tests. No game process, UI or full game build was used.
These checks establish specific contracts, not a whole-game confidence percentage
or a demonstrated explanation for the observed distortion.

## Bone indices are floats in the captured programs

Both ignored `story-09-shader.json` (69 instructions) and `story-10-shader.json`
(88 instructions) declare primary v2 as FLOAT3, format32, offset20, normalized0.
Their primary stride56 also contains position FLOAT3 at0, weights FLOAT2 at12,
normal FLOAT3 at32, diffuse BGRA8 at44, UV FLOAT2 at48. The normalized-byte input
is diffuse v4, not the bone index. Captured c114.z is3; bone row addressing uses
that multiplier. The88 program's additional morph addresses come from FLOAT4
secondary inputs, also unnormalized.

Thus xemu's documented byte-normalization workaround for ARL is not evidence
for changing these programs. Exact floor remains in place. The16-matrix GPU
fixture now feeds the real interleaved formats through `glVertexAttribPointer`
and the declaration decoder, rather than using constant attribute setters.
All16 selections pass. The existing tests also retain negative/fractional ARL,
paired old-address reads, R12 alias and mask checks. No new defect was identified
in paired writes or the conservative dependency proof during this bounded review.

## Reproduced color-boundary discrepancy

Pinned xemu `vsh.c` converts NaN diffuse/specular components to1 before clamping
to0..1. Our plain native `clamp` instead produced0 for both signed quiet NaNs
on this Mac's GPU: all16 diffuse/specular components in the two cases differed.
The failure is retained in `local/reports/nv2a-vertex-edge-before.log`.

The minimal correction explicitly converts each NaN component to1 at the
color-output boundary. Finite arithmetic, ARL, relative addressing, dependency
analysis and RSQ remain unchanged. The same GPU fixture now passes positive and
negative quiet NaNs, signed zero, finite fractional/integer inputs and infinities.
RSQ already matched the checked reference results for signed zero, infinities
and negative finite inputs; no speculative RSQ change was made.

This is parity with a pinned reference implementation. No captured failing
game vertex has yet demonstrated NaN color, and no Xbox hardware measurement
was performed here. A color correction does not establish a geometry-warp fix.

## Evidence and validation

- [NVIDIA NV_vertex_program revision1.10](https://registry.khronos.org/OpenGL/extensions/NV/NV_vertex_program.txt)
  specifies ARL floor and instruction floating-point requirements; it is not
  an Xbox silicon measurement.
- [xemu vsh.c, pinned fdfb5a8](https://github.com/xemu-project/xemu/blob/fdfb5a8f481b2f870c57080e74ec8d3a31a47053/hw/xbox/nv2a/pgraph/glsl/vsh.c)
  defines `NaNToOne` and applies it to diffuse/specular output.
- [xemu vsh-prog.c at the same revision](https://github.com/xemu-project/xemu/blob/fdfb5a8f481b2f870c57080e74ec8d3a31a47053/hw/xbox/nv2a/pgraph/glsl/vsh-prog.c)
  documents its ARL normalization workaround and explicit RSQ edge cases.
- [nxdk hardware-test source, pinned33e7c6b](https://github.com/abaire/nxdk_pgraph_tests/blob/33e7c6b0ebf4d6e1b67d0ca475adc336fb5fbaf8/src/tests/attribute_float_tests.cpp)
  includes NaN color test cases. Its existence is not treated as a hardware
  result obtained in this workspace.

`tools/tests/nv2a_vertex.c` passes after the correction, including all previous
tests and actual69/88 program compilation plus live-constant dependency checks:
`nv2a-vertex-edge-build.log`, `nv2a-vertex-edge-test.log`, and
`nv2a-vertex-edge-88-test.log` under ignored `local/reports/`. The production
diff is a color-boundary helper and its two calls; it contains no game-specific
special case, forced shader result or tolerance added to bone indexing.
