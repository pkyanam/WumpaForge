# Programmed vertex with fixed pixel stages

Boot31 advanced through the actual Universal globe animation, then reached the
previously explicit mixed-stage rejection. Boot32/33 establish the pairing:
vertex handle30464065 (47 original NV2A instructions), pixel handle0, indexed
26-vertex draw through3B010→A5D40→A8240. Full evidence is ignored in
`local/reports/boot-33-pairing-full.log`, especially its `[PAIRING-STATE]` JSON.
Fog and specular are both off. Textures0/1 are actual DXT5 2D resources.

The captured pixel equations are stage0 COLOROP4/ALPHAOP4 with TEXTURE and DIFFUSE
arguments; stage1 COLOROP7 with TEXTURE and CURRENT; stage2/3 disabled. This is
texture0×diffuse followed by adding texture1 RGB. It is not equivalent to either
a texture copy or the previously active programmed pixel shader.

## Important captured invalid state

Stage1 ALPHAOP was0. This is **not** treated as a preserve-alpha alias:

- Original4361 builder10A9B0 reads alpha operation at10ADCF.
- 10AA54/10AA57 dispatch through table10AED4 indexed by operation−1.
- Operation0 selects DWORD10AED0, whose actual XBE bytes are0x00498D00 (padding),
  not an operation handler. A native implementation must not invent its meaning.
- Valid ALPHAOP1 enters10AA5E. For a later stage,10AA9D sets the output word to0;
  10ADBC stores that zero destination mask. This leaves preceding alpha intact.

Parent task owns correcting any demonstrated device-default initialization gap
in graphics.c. The new lowering explicitly rejects operation0. A later stage's
valid ALPHAOP1 preserves current alpha; stage0's initial current is diffuse.

Cache indices are Xbox-specific:12 COLOROP,13/14/15 COLORARG0/1/2,
16 ALPHAOP,17/18/19 ALPHAARG0/1/2,20 RESULTARG,21 TEXTURETRANSFORMFLAGS,
28 TEXCOORDINDEX,11 ALPHAKILL. In boot33 index20=1 means CURRENT, while
transform flags and alpha-kill are0. The cache is four32-DWORD blocks beginning
at10EC18, independently corroborated by original FE170/10A9B0.

## Implementation

`nv2a_pixel_fixed_definition` in `src/nv2a_pixel.c` lowers actual fixed texture
stages to the existing register-combiner definition. A real initial combiner
copies diffuse into CURRENT, then each enabled stage implements separate RGB
and alpha operations with simultaneous writes. The existing validated GLSL
generator supplies input mapping, arithmetic, output saturation, default final
combiner and final alpha testing. This retains the original vertex program,
declaration, constants and emitted varyings.

Supported bounded operations are disable, select argument1/2, modulation,
modulation×2/×4, add, signed add, signed add×2 and subtract. Arguments include
diffuse/current/texture/texture factor/specular/temporary, complement and alpha
replication. Existing undefined-register validation catches reads of an
uninitialized temporary. Bound resource dimensions choose PROJECT2D, PROJECT3D
or cube sampling, following original107CA0's header dispatch; the current native
resource binder still rejects volume textures. A selected texture argument with
no resource, unsupported operations, transform flags or key/sign/alpha-kill
states produce explicit errors.

`shader_bridge.inc` now creates a local active pixel definition for each draw
when SetPixelShader(0) selects fixed stages. The same definition supplies cache
identity and actual uniforms. Texture-factor changes therefore update uniforms
without recompiling, and old programmed pixel constants cannot leak into a fixed
draw. Fixed vertex with programmed pixel remains explicitly unsupported. Both
fully fixed stages retain the existing backend path; this change is bounded to
the actual reached mixed pairing.

Equations are independently grounded in
[Microsoft's texture-operation contract](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dtextureop)
and the
[NVIDIA register-combiner specification](https://registry.khronos.org/OpenGL/extensions/NV/NV_register_combiners.txt).
Xbox enum/cache reconstruction is corroborated by
[Cxbx's public definitions](https://github.com/Cxbx-Reloaded/Cxbx-Reloaded-legacy/blob/96aabe72e238674a22a61695fb6f5295259edf62/src/core/hle/D3D8/XbD3D8Types.h).
Actual4361 instruction addresses above resolve SDK-specific details. No upstream
implementation was copied for this lowering.

## Validation

The extended `tools/test_nv2a_pixel.c` passes actual GLSL compilation/linking and
GPU readbacks for the reached modulation/add/preserved-alpha equations, disabled
stages, independent arguments, alpha replication/complement, texture factor,
and invalid ALPHAOP0/unbound-resource/alpha-kill rejection. All preceding pixel
generator tests also pass. Output: `local/reports/fixed-pixel-smoke.log`.

```sh
clang -std=c11 -O1 -Wall -Wextra -Werror -Wno-missing-field-initializers -I/opt/homebrew/include $(/opt/homebrew/bin/sdl2-config --cflags) tools/test_nv2a_pixel.c src/nv2a_pixel.c $(/opt/homebrew/bin/sdl2-config --libs) -L/opt/homebrew/lib -lepoxy -o build/input/test_fixed_pixel
build/input/test_fixed_pixel > local/reports/fixed-pixel-smoke.log 2>&1
```

`tools/test_mixed_shader_bridge.inc` adds a full bridge fixture: genuine NV2A
vertex instructions/declaration, SetPixelShader(0), two actual native textures,
fixed equations/alpha and texture-factor cache reuse. Include it after existing
test helpers and `test_shader_bridge`, before smoke main; call it after
`test_shader_bridge()`. The combined native graphics smoke passed with this
fixture, including prior indexed/immediate/framebuffer tests (exit0):
`local/reports/mixed-bridge-smoke.log`. To respect root ownership of graphics.c,
validation used an ignored copy `local/reports/graphics-mixed-smoke.c` with only
the include/call inserted, compiled with the usual graphics smoke command plus
`-Isrc -Itools`. Parent still owns adding those two lines to the tracked harness.
No game build or game launch was performed for this task.
