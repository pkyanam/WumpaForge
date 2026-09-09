# Xbox pixel shader translation

`src/nv2a_pixel.{h,c}` generates GLSL410 from a240-byte Xbox
D3DPIXELSHADERDEF. It is independent of GL context creation, allocation, game memory
and the root graphics integration. Errors leave the source empty and give a bounded
diagnostic. No fixed shader or unrelated output is substituted for unsupported state.

## Evidence and provenance

The wire layout comes from
[Cxbx XbD3D8Types.h](https://github.com/Cxbx-Reloaded/Cxbx-Reloaded-legacy/blob/96aabe72e238674a22a61695fb6f5295259edf62/src/core/hle/D3D8/XbD3D8Types.h#L396),
with bit fields and XDK software mappings cross-checked against
[XbPixelShader.h](https://github.com/Cxbx-Reloaded/Cxbx-Reloaded-legacy/blob/96aabe72e238674a22a61695fb6f5295259edf62/src/core/hle/D3D8/XbPixelShader.h).
The general and final combiner equations, simultaneous stage writes, input maps,
output bias/scale/clamp and final input restrictions follow NVIDIA's published
[NV_register_combiners specification](https://registry.khronos.org/OpenGL/extensions/NV/NV_register_combiners.txt)
and [per-stage constant extension](https://registry.khronos.org/OpenGL/extensions/NV/NV_register_combiners2.txt).
Xbox-specific texture/register encodings were cross-checked with
[xemu psh_regs.h](https://github.com/xemu-project/xemu/blob/fdfb5a8f481b2f870c57080e74ec8d3a31a47053/hw/xbox/nv2a/pgraph/psh_regs.h)
and its [GLSL generator](https://github.com/xemu-project/xemu/blob/fdfb5a8f481b2f870c57080e74ec8d3a31a47053/hw/xbox/nv2a/pgraph/glsl/psh.c).

This is newly written source implementing the documented fields/equations; no
upstream generator implementation was copied. Downloaded references remain ignored
under `local/reports/pixel-reference`. Cxbx source carries GPL2-or-later notices;
xemu psh.c GPL2-or3, psh_regs.h LGPL2-or-later. NVIDIA's specification carries its
own copyright/IP notice. These sources are references, not vendored components of
this implementation. Existing toolkit HLSL combiner code was inspected as context
and not used as the authority for correctness.

## Wire layout and actual captured shader

The pixel handle is a guest object with refcount+0, owned flag+4, definition pointer+8.
Original SetPixelShader at102BB0 dereferences handle+8. Boot14's handle2606890
points to260689C. Its240-byte definition begins at file offset0x89C in the ignored
`local/reports/boot14-shader-objects.bin` capture.

| Byte offset | Field |
| --- | --- |
| 00 | alpha inputs[8] |
| 20,24 | final ABCD, EFG |
| 28,48 | constant0[8], constant1[8] |
| 68 | alpha outputs[8] |
| 88 | RGB inputs[8] |
| A8,AC,B0 | compare mode, final C0, final C1 |
| B4 | RGB outputs[8] |
| D4,D8,DC,E0 | combiner count, texture modes, dot mapping, input texture |
| E4,E8,EC | C0 mapping, C1 mapping, final constant mapping/global flags |

The real definition uses8 combiner stages, all4 PROJECT2D textures, control11108,
texture modes8421, and no advanced dot mapping. Its final words are both zero.
The shader definition is game data and is not copied into tracked fixtures.

Zero final words require the title SDK's default final combiner. This is confirmed
by102D20..102D4D omitting a final-combiner packet, and108140..1081CA emitting these
values while updating fog state:

| Fog | Specular | Final ABCD | Final EFG |
| --- | --- | --- | --- |
| off | off | 0000000C | 00001C80 |
| off | on | 0000000E | 00001C80 |
| on | off | 130C0300 | 00001C80 |
| on | on | 130E0300 | 00001C80 |

Fog enabled is read from guest10EF60, specular from10EF8C in this4361 SDK. Newer
SDK render-state enum indices differ. The generator options supply these values.
The resulting output is R0 (optionally clamped R0+specular), with optional fog
interpolation; alpha comes from R0.a. The actual captured vertex program does not
produce diffuse colors, but the captured pixel program reads textures and constants,
so no diffuse input is fabricated to make this shader produce color.

## Supported behavior and integration contract

The generator supports0..8 general stages; independent RGB and alpha input mappings;
AB/CD multiplication and RGB dot products; sum/MSB-or-LSB mux; legal bias/scales;
clamping; writable color/texture/temp registers; blue-to-alpha without conflicting
alpha writes; per-stage or shared constants; final E*F and color sum; and explicit
or SDK-default final stages. RGB/alpha expressions are evaluated before either
portion writes outputs. Reads of undefined temporary components are rejected.

Texture modes NONE, PROJECT2D, PROJECT3D, CUBEMAP, PASSTHRU and CLIPPLANE generate
real corresponding operations. Linear2D texture coordinates are normalized by
texture size when the rectangle mask is set. PROJECT modes use projective W;
CLIPPLANE honors each component's compare bit. Metadata reports the sampled texture
mask and sampler dimension2/3, or4 for cubemap. Bump/dependent/dot texture families,
unknown bits, invalid output registers, duplicate destinations, ambiguous
blue-to-alpha collisions and dot-product plus sum/mux destinations fail explicitly.
Advanced texture dot mapping/input fields are unused for the supported modes.

Varyings agreed with the vertex generator: `vec4 vD0,vD1,vT0,vT1,vT2,vT3` and
`float vFog`. Output is `fragColor`. Uniforms are:

- `tex0..tex3`, with sampler types reported by metadata.
- `vec4 u_psconstants[18]`: RGBA, index2*stage for C0 and2*stage+1 for C1;
  final constants use16/17. Returned initial values unpack ARGB DWORDs.
- `vec3 u_fogcolor` and the interpolated fog varying.
- `int u_alpha_enable,u_alpha_func`, `float u_alpha_ref`, using the existing
  native backend names. Function1..8 follow D3DCMP; reference is normalized.
  Alpha comparisons round to the hardware8-bit domain.

The XDK final_constants bit100 requests texture-mode adjustment when the bound
resource type differs. Parent integration must supply appropriately adjusted modes
for cube/volume bindings, or reject them until implemented. The current captured
shader uses ordinary2D textures. Do not bind a2D object to a generated3D/cube sampler.
Shader cache identity should omit constant values, which are uploaded as uniforms,
but include instruction fields, options and actual sampler type decisions.

Finite float operations follow the published combiners, but exact NV2A fixed-point
rounding across every intermediate and unusual signed/mux case is not claimed.
Texture borders, shadow comparisons, color key, convolution and special depth modes
are outside this bounded generator and must be implemented or rejected by the
renderer when encountered. No game launch or game frame correctness is claimed here.

## SetPixelShaderConstant

Original102D80 is stdcall(index, floatRGBA*, count), ret12, and performs:

1. Clamp each component to0..1. Compute255*x+0.5 with x87 arithmetic, store float,
   then CVTTSS2SI in105A20. Pack AARRGGBB and save device+3EC+index*4.
2. Compare the index to every4-bit nibble in C0/C1 mappings (all8 stages, regardless
   active count), calling FD860 for render-state10+stage or18+stage on matches.
3. Compare the index to the low2 nibbles in final_constants, updating slots43/44.
4. For index0, additionally send raw, unquantized RGB to hardware method181C for
   the constant eye vector used by currently unsupported texture reflection modes.

FD860 writes the native GPU register and global render-state cache10EE18+state*4.
The immutable guest shader definition is never changed. Binding a shader again
copies its original packed constants to the GPU; the device API constant store
persists independently.

`nv2a_pixel_set_constants` updates an ACTIVE definition-shaped state copy using
those mappings. `nv2a_pixel_pack_constant` supplies the packed color for the parent
store/cache. Do not write the active copy back into the immutable guest definition.
NaN conversion has low byte0 (matching CVTTSS2SI's invalid integer); infinities clamp.
The helper supports all16 mapped API indices0..15 and validates the whole range
before modification. This is independent of the eight combiner stages. Original
102F15 stores at device+3EC+index*4; 102F30,102F5C and102F8A mask each mapping with
0xF before comparing it directly to the API index. There is no sentinel exclusion:
mapping0xF matches index15, whose packed device slot is+428 (last byte+42B).
These exact operations establish behavior throughout the16-slot mapped interval;
the retail routine has no bounds check, so16 is our checked compatibility boundary,
not an assertion that the original rejects larger indices. Values above15 cannot
match any mapping. Index16 with count0 is accepted as an empty update.

Boot17 independently reached a real call from guest3AD90 (return3ADC2) with index8,
data92B9F0,count1, shader2607310, after four native shader programs linked and drew;
see ignored `local/reports/boot17-constant-fault.log`. The previous0..7 helper bound
was incorrect and caused this stop. Focused tests now cover indices8 and15 in both
banks and final slots, a7..8 update, retained unmapped slots, empty updates and
transactional rejection of15..16. The parent host API bank must contain16 DWORDs;
the shader uniforms remain18 RGBA values (eight C0/C1 pairs and two final constants).

## Focused verification

Production source compiles C11 with `-Wall -Wextra -Werror`. The standalone test
creates one hidden16x16 GL4.1 context on the native Metal OpenGL driver. It compiles,
links and reads back synthetic pixels for parallel RGB/alpha operation, all8 input
modifiers, six output mappings, dot/mux, final E*F, default fog/specular, constant
banks, blue-to-alpha and8-bit alpha testing. It checks malformed definitions,
undefined registers, conflicting writes and source-buffer exhaustion. Sampling modes
3D/cube/pass/clip compile/link, but their sampling results are not all readback-tested.

```sh
clang -std=c11 -O1 -g $(pkg-config --cflags sdl2 epoxy) tools/test_nv2a_pixel.c src/nv2a_pixel.c $(pkg-config --libs sdl2 epoxy) -o build/test_nv2a_pixel
./build/test_nv2a_pixel
```

To also load the actual ignored definition, compile/link it and verify its8-stage
arithmetic against hand-calculated synthetic texture colors/constants:

```sh
./build/test_nv2a_pixel local/reports/boot14-shader-objects.bin 0x89c
```

Both passed on2026-09-08. The actual definition produces a6811-byte shader saved as
`local/reports/boot14-pixel.glsl`; the test compares output to(1,.85,1,0) for its
specified synthetic inputs. This checks the real shader program's arithmetic,
not the game's actual textures or final game frame. Constant-helper tests cover
mapped/unmapped slots, immutable-source preservation, ARGB packing/clamping,
NaN/infinity and invalid-range rollback. No full game build was run by this subtask.

## Dependent normal-map reflection

The reached DOTPRODUCT/DOT_RFLCT_SPEC chain is now translated to native GPU
dot products and reflected cube sampling. See DOT-REFLECTION-SHADER.md for
exact source provenance, bounded mapping/stage support and six-face readback
validation. Other advanced texture modes remain explicit errors.

Reached address mode5 now uses native projected2D mirror-once sampling with
original LOD gradients and independent live U/V sampler uniforms. M3 lacks the
three native mirror-clamp extensions. See [mirror-once evidence and GPU tests](MIRROR-ONCE-TEXTURES.md).
