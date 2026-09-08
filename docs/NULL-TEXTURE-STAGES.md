# Original fixed-stage null texture behavior

Story08 completed all11 backstory scenes, then stopped during the original
post-movie render at swap24305, movie=-1/cut_on=0, fade82. The shader snapshot has
programmed vertex handle30498833 (69 instructions), no programmed pixel shader,
and stage0 COLOROP=ALPHAOP=4 (MODULATE), ARG1=2 (TEXTURE), ARG2=0 (DIFFUSE),
RESULTARG=1 (CURRENT). All operations and arguments are supported. The only
condition that can reject this exact state in the previous fixed lowering is an
absent texture binding (dimension0). The old probe did not store texture handles;
this is a deduction from its captured state and the exact rejection predicate.
The root is adding explicit binding diagnostics for future captures.

## Supplied original SDK evidence

`local/reports/disasm/asm/D3D.asm`,10A910 argument encoding:

-10A91D..10A929 reads the bound texture pointer at10CB88+4*(stage&3).
-10A92D returns an all-ones sentinel for a null pointer; a real texture returns
 NV2A register8+stage.
-10AD7E..10ADA8 detects the sentinel in the combined input word. It replaces
 the entire RGB or alpha input equation with diffuse for the initial stage
 (04200000/14200000), or CURRENT for subsequent stages (0C200000/1C200000).
-10ADAE..10ADBC still writes the previously selected destination/output mapping.
 RGB and alpha are evaluated independently. The next-stage loop continues at
10ADE7..10AE15; null binding does not disable the remainder of the cascade.

The native fixed lowering now preserves this behavior for its supported
operations2..10. It validates real argument/source flags before emitting the null
sentinel, and handles only arguments used by the selected operation. No fake
white texture, forced success or game-logic bypass is introduced. Output scaling
and RESULTARG remain active, just as in the original SDK. Bound→unbound→rebound
state generates the correct pixel definition and native shader cache variant.

The primary [Microsoft texture-operation definition](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dtextureop)
corroborates the MODULATE operation and argument arithmetic. The Xbox-specific
null-binding behavior above is established by the supplied retail SDK itself;
PC documentation alone is not used to infer that behavior.

The actual stop stack is shader_draw→draw_indexed_vertices→3B010→A8190→A8240→
9FFD0→A0990→A8140→87BF0. This is after the backstory; reaching it does not establish
playable level navigation or correct completion of the transition.

## Tests

`tools/test_mixed_shader_bridge.inc` uses the real native guest SetTexture and
DrawVerticesUP bridge path with an actual programmed vertex shader.
It verifies stage0 unbinding preserves diffuse before a later bound ADD stage,
stage1 unbinding preserves CURRENT, both unbound preserve diffuse, and rebinding
restores textured output. Distinct RGB/alpha inputs prevent a white dummy texture
or unconditional cascade termination from passing.

`tools/test_nv2a_pixel.c` also verifies absent-texture MODULATE2X retains output
scaling while independent alpha still selects texture factor, and invalid
argument flags remain an error. Both fixtures use synthetic data only.

Standalone pixel compilation/readback passed in
`local/reports/null-texture-pixel-build.log` and
`local/reports/null-texture-pixel-test.log`. Combined native graphics smoke also passed in
`local/reports/null-texture-build.log` and `local/reports/null-texture-smoke.log`,
including the concurrent bound-secondary-stream exact-zero proof fixture. The
compilations were sequential, each using one compiler job. No game was launched.
