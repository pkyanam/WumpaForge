# Native dependent dot/reflection texture chain

Story14's hub shader uses texture modes[1,17,17,12] (packed64621hex): a normal-map
PROJECT2D sample, two DOTPRODUCT stages, then DOT_RFLCT_SPEC. Dot mapping111hex
selects D3D signed RGB conversion for all three rows; input_texture0 routes each
row to t0. Only stages0 and3 have actual textures. The captured stage0 normal map
is128×128 RGBA8 with8 levels; stage3 is128×128 DXT3 cube with8 levels.

## Original shader provenance

The supplied XBE static pixel definition at1AE550/file19F0F0 matches all60 captured
DWORDs. Its paired31-instruction vertex program uniquely matches file19EEFC
(VA1AE35C); registration atB6F69/B6F6E passes PS1AE550 and VS1AE358, shader id1B.
The captured combiner words C4CB0000/D4DB1010 multiply d0.rgb/a by t3.rgb/a.
No combiner reads the intermediate DOTPRODUCT stages' undefined color values.

## Implemented equations

Primary NVIDIA [NV_texture_shader specification](https://registry.khronos.org/OpenGL/extensions/NV/NV_texture_shader.txt)
defines chained dot stages and reflected cube lookup: each row uses interpolated
texture XYZ and a mapped prior texture result; the three row W values form the
eye vector. The pinned primary
[xemu NV2A implementation](https://github.com/xemu-project/xemu/blob/fdfb5a8f481b2f870c57080e74ec8d3a31a47053/hw/xbox/nv2a/pgraph/glsl/psh.c)
corroborates the Xbox stage layout, source-selection bits, D3D signed mapping,
and reflection equation. Original SDK102D08 onward combines dot_mapping and
input_texture when programming the hardware shader-control word.

The new native GLSL generator performs:

- Signed mapping1: mapped=(255*t0.rgb−128)/127. Mapping0 preserves RGB.
- dot1=dot(vT1.xyz,mapped1), dot2=dot(vT2.xyz,mapped2), and corresponding dot3.
- N=(dot1,dot2,dot3), E=(vT1.w,vT2.w,vT3.w).
- R=2*N*dot(N,E)/dot(N,N)−E; sample the actual stage3 cube with R.

No texture-coordinate projection, normal normalization, epsilon or arbitrary
reflection vector is inserted. The signed conversion maps byte128 exactly to0,
255 to1,1 to−1, and0 to−128/127; it is not interchangeable with2*x−1. Intermediate
DOTPRODUCT stages produce no sampler requirement. Their RGBA results are not
accepted as combiner sources; attempts fail explicitly because this task verifies
their scalar dependent result only. Degenerate zero normals are left to the
original floating-point/cube lookup behavior, rather than assigned an invented
fallback face.

The implementation supports DOTPRODUCT only at stages1/2 and DOT_RFLCT_SPEC only
at3 after two DOTPRODUCT stages. Inputs must name an earlier defined color stage.
Mappings other than0/1, other dependent/reflection modes, incomplete chains and
undefined intermediate-color reads remain explicit errors. The actual captured
shader fits these verified boundaries. Texture-mode adjustment now also handles
the newly supported reflection sampler: an unbound flagged reflection stage
becomes NONE; the non-sampling DOTPRODUCT stages remain active.

`src/nv2a_pixel.c` remains independently authored from field definitions and
mathematical equations; no upstream implementation text is copied. Existing
vertex-generator licensing/provenance remains unchanged. Supplied shader data
and captures remain ignored, with synthetic fixtures in Git.

## Native GPU validation

`tools/test_dot_reflection.inc` constructs synthetic original vertex/pixel objects,
a1×1 normal texture and a six-color cube through the real guest resource bridges.
It uses the captured texture-mode chain and combiner words, with nonwhite diffuse
RGBA. Native pixel readback verifies all six reflection directions, the correct
sign, division by dot(N,N) using a nonunit normal, and exact signed-map byte128
centering amplified by256× rows. Malformed mapping, forward/undefined dependent
source and intermediate RGBA use are rejected.

The complete native GL smoke passed, including all earlier texture-mode, fog,
multistream, palette, target/copy and lifetime regressions. One compiler job;
logs `local/reports/dot-reflection-build.log` and `dot-reflection-smoke.log`.
No independent game/full AOT build was launched. Parent owns the next actual hub
run; component pixel correctness alone does not establish playable gameplay.
