# Native programmable and fixed fog

Story06 reached the real Level7 snow draw with fog enabled and stopped at the
previous explicit fog guard. The vertex program has69 instructions. The native
programmed-vertex pipeline now evaluates fog from the original program's oFog.x,
using the retail SDK's cached parameters, and supplies the factor to the existing
NV2A pixel-combiner generator. This applies to programmed vertices paired with
either an original pixel shader or the translated fixed texture-stage equations.
No game timing, scene logic or hardware wait bits are changed.

## Original evidence

The supplied XBE's `local/reports/disasm/asm/D3D.asm`, function108000, converts
these original render-state cache slots into NV2A methods29C/2A0/2A4/9C0:

| State | Cache address | Meaning |
| --- | --- | --- |
|82|10EF60|enable|
|83|10EF64|table mode:0 NONE,1 EXP,2 EXP2,3 LINEAR|
|84|10EF68|start float|
|85|10EF6C|end float|
|86|10EF70|density float|
|87|10EF74|range fog selection for fixed-function distance generation|
|119|10EFF4|ARGB fog color; native FDA80 bridge already preserves it|

Original game A42A0 sets table mode3 when enabling fog and mode0 when disabling.
A4280 sets start/end; 9D3F0 supplies these parameters and color. Thus LINEAR is
concretely used by this title, rather than inferred from a generic API default.

For LINEAR, the original108072..1080AD computes reciprocal1/(end-start), except
exactly equal endpoints use8192 from XBE10C0FC. Its emitted bias is1+end*reciprocal
and slope is-negative reciprocal. EXP/EXP2 use bias1.5 and density multiplied by
the exact single-precision constants at169E98/169E94 respectively:
-0.0901680737733841 and-0.21233002841472626. NONE with enable set emits bias1,slope1,
which passes the shader distance through as the factor. Native coefficients are
read each draw; changing values does not require shader recompilation.

## NV2A semantics and provenance

The pinned primary implementation
[xemu vsh.c](https://github.com/xemu-project/xemu/blob/fdfb5a8f481b2f870c57080e74ec8d3a31a47053/hw/xbox/nv2a/pgraph/glsl/vsh.c)
confirms that programmable vertices use oFog.x directly as the fog distance. It
also specifies the coefficient bias adjustment, EXP exponent multiplier16,
EXP2 squared multiplier32, special Inf/NaN behavior and interpolation ordering.
The emitted source implements those semantics:

- LINEAR: bias+distance*slope-1.
- EXP: bias+exp2(distance*slope*16)-1.5.
- EXP2: bias+exp2(-distance²*slope²*32)-1.5.
- Infinite distance or NaN result yields1 for LINEAR/EXP and0 for EXP2.
- Factors are bounded only to finite float range in the vertex shader, interpolated
  by the GPU, then clamped0..1 by the pixel shader. Premature vertex clamping gives
  incorrect fog gradients and is explicitly covered by the readback fixture.

The original pixel definition continues to decide its final-combiner expression.
Only its zero/default final expression gains the SDK's default fog blend when
fog is enabled. Explicit final combiners retain their own fog-register behavior;
alpha is not replaced by the fog factor. Disabled fog supplies factor1.

`src/nv2a_vertex.c` retains its existing GPL-2.0-only OR GPL-3.0-only reference
attribution; see NV2A-VERTEX.md. No reference source or proprietary shader data is
added to Git. The new GLSL uniform contract is integer u_nv2a_fog_mode (0 disabled,
1 linear/pass-through,2 exp,3 exp2) plus vec2 u_nv2a_fog_params (bias,slope).

## Explicit boundary

Fixed XYZ vertices now use the original fixed distance contract, described below.
Enabled XYZRHW remains explicit: original102850 uploads a separate SDK vertex
microprogram for transformed coordinates. Its fog distance cannot be assumed to
be the untransformed world/view contract. Fixed-vertex/programmed-pixel pairing
also retains its existing explicit boundary. NV2A ABS fog modes are not emitted
by the audited original SDK; unknown D3D table modes still fail explicitly.

## Validation

`tools/test_native_fog.inc` constructs a synthetic3-instruction original vertex
object (MOV position,constant diffuse,oFog), original pixel definition and guest
vertices. Full native GL4.1 readback passes modes0..3, near/mid/far colors, live
start/end updates, equal-endpoint coefficients, alpha preservation, disabled fog,
Inf/NaN semantics and interpolation before clamping. It uses no game assets.

One compiler job built the complete graphics smoke, including the concurrent
SetShaderConstantMode0/1 fixture and all prior multistream, palette, render-target,
resource-lifetime and shader tests. All passed. Logs are ignored:
`local/reports/native-fog-build.log` and `local/reports/native-fog-smoke.log`.
Command: the graphics smoke command in RESOURCE-REGISTRY.md, with output
`build/input/test_native_fog`. No game launch or full AOT build was performed by
this component task; actual Level7 fog still needs the parent's integration run.

## Reached fixed XYZ fog: story20 / Arctic Antics

Build58/story20 actually loaded Level7/Demo0, `levels/a/snow_m/snow`. The native
fixed pipeline stopped before the first snow frame with enable1, tableLINEAR3,
start1, end35, range0 and live physical c57=(0,0,1,0). This is separate from the
earlier programmed-vertex Level7 fog implementation.

Original108031..10804E computes fog generation mode as `1 + !rangeEnabled`.
Original108053..108068 overrides it to0 when table mode is NONE. The resulting
NV2A modes are0 SPEC_ALPHA,1 RADIAL,2 PLANAR. The original coefficient path is
shared with programmed fog. The [pinned fixed vertex reference](https://github.com/xemu-project/xemu/blob/fdfb5a8f481b2f870c57080e74ec8d3a31a47053/hw/xbox/nv2a/pgraph/glsl/vsh-ff.c)
confirms the three distances: clamped input specular alpha, length of view-space
XYZ, or the signed plane dot product plus plane offset. This reference is LGPL2
or later; this implementation uses the documented equations and register
semantics, with independently written native shader integration.

The title bridge passes coefficients, generation mode, RGB fog color and actual
physical c57 to `xbox_D3D8GLSetFixedFog`. The backend computes world*view separately
from world*view*projection, so projection/viewport/depth normalization cannot
change eye distance. Its fixed vertex shader computes the original factor before
interpolation without prematurely clamping0..1. The fragment shader clamps the
interpolated factor and blends RGB with fog color; original alpha and alpha test
remain unchanged. Specular alpha is fetched from its actual FVF field even when
specular lighting is disabled. No new guest shader, synthetic fog data, hardware
wait bypass or game logic change is used.

`tools/test_fixed_fog.inc` adds asset-free GPU readback for planar versus radial,
specular-alpha/tableNONE, LINEAR/EXP/EXP2, separate world and view translations,
projection independence, negative signed eye distance, live plane scaling and
offset, unchanged alpha and fog disable. The full combined renderer smoke passed
on its first run, including all existing programmable fog, shader, resource,
retained vertex address and native fence tests. Logs:
`local/reports/fixed-fog-build.log` and `fixed-fog-smoke.log`; binary
`build/input/test_fixed_fog`. Cumulative graphics patch reverse-apply validation
and `git diff --check` passed, including the existing CMake Apple OpenGL linkage.
Production was frozen before parent build59. Actual snow playability remains the
parent's integration/visual test, not something the component fixture proves.
