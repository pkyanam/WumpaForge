# Native viewport methods and vertex constants

Boot35/39/41 captured a69-instruction skinned vertex program whose last two
instructions multiply oPos by physical c58, divide by W using paired RCC, then
add physical c59. Both vectors were zero in the native snapshots. This collapsed
the resulting position to one point despite the preceding skinning arithmetic.
Boot42 could display New Game/Load Game text while the large logo was absent.
The hypothesis has a direct state-contract explanation, independent of scene
timing or omitted assets.

## Hardware alias and retail4361 evidence

NV2A viewport scale/offset are aliases of physical vertex constant registers58
and59. Pinned xemu revisionfdfb5a8f481b2f870c57080e74ec8d3a31a47053 implements
[SET_VIEWPORT_SCALE/OFFSET](https://github.com/xemu-project/xemu/blob/fdfb5a8f481b2f870c57080e74ec8d3a31a47053/hw/xbox/nv2a/pgraph/pgraph.c)
by writing that same constant bank. The register names
`NV_IGRAPH_XF_XFCTX_VPSCL=0x3A` and `VPOFF=0x3B` are in
[nv2a_regs.h](https://github.com/xemu-project/xemu/blob/fdfb5a8f481b2f870c57080e74ec8d3a31a47053/hw/xbox/nv2a/nv2a_regs.h).
No copied emulator method implementation is linked; this bridge applies the
SDK's actual equations to the native shader uniforms.

Original local disassembly `local/reports/disasm/asm/D3D.asm` establishes:

- FF860 clamps/stores the24-byte viewport at device+9D0 and calls FE2B0 atFF9A8.
- FE2B0 builds NV097 viewport-offset method0xA20 and scale method0xAF0.
  FE33B..FE3FB derive the programmable-vertex case from viewport, raster factors
  at device+458/+45C, depth scale+450, and screen offset+9E8/+9EC.
- 1001D0 adds float at XBE10B000 to its X/Y offset arguments. The original
  initializer104330 calls it with0,0. That float is0x3F080000 =17/32 =0.53125.
  This is the retail offset; it is not a replacement half-pixel guess.
- FEF20 refreshes the viewport after a target change. A nonnull depth attachment
  calls105E00 to update device+450; a null depth keeps the previous scale.
  The105E00 jump table maps format0x2A (LIN_D24S8) to float16777215.
- Shader selection102940 first calls102610 at1029BF, which copies the object's
  upload packets, including embedded constants. It then calls102670 at1029C6;
  this calls FE2B0 at1026C0. Viewport aliases therefore overwrite any embedded
  c58/c59 in that order. A later explicit SetVertexShaderConstant write may
  overwrite the alias until another viewport/shader-selection operation.

For the supported single-sample D24S8 native targets:

```text
c58 = (Width/2, -Height/2, (MaxZ-MinZ)*16777215, 0)
c59 = (X+Width/2+0.53125, Y+Height/2+0.53125, MinZ*16777215, 0)
```

The helper retains the original raster-factor and screen-offset inputs in
guest device fields and reproduces FE2B0's conditional0.5 screen-offset
adjustment when device bit0x8000 and state at10F02C are both set. W-buffering
and non-D24 depth formats remain outside the currently accepted native path.

## Integration

`graphics.c` initializes unit raster factors, D24 depth scale and the retail
screen offset on device creation. It updates the native bank after creation,
successful SetViewport, and render-target viewport reset. `shader_bridge.inc`
updates it after applying embedded shader constants, preserving the observed
selection order. It normalizes generated shader output using the same retained
depth scale. There is no per-draw overwrite that could erase an explicit
constant write. Hardware alias updates do not fabricate writes to the SDK's
separate CPU constant cache at device+B98.

The shader compiler's existing output conversion remains unchanged: it receives
the screen-space coordinates produced by these original instructions, applies
the established1/16-pixel truncation, and reverses the native viewport/depth
mapping to OpenGL clip coordinates.

## Validation

New `tools/test_viewport_constants.inc` is part of the combined graphics smoke.
It uses a synthetic MOV followed by the same MUL/RCC/MAD structure as the
captured epilogue. Native GPU readback verifies:

- Correct scale/offset vectors, including17/32 bias and zero W components.
- Viewport `(20,30,200,120)`, depth range0.25..0.75 and input W=2 render the
  expected quad; its center depth is0.375 and an outside sample stays black.
- Explicit c58 writes survive until another shader selection restores aliases.
- Switching to a64×32 texture target and back refreshes dimensions and preserves
  D24 depth scale without a bound depth attachment.

All earlier texture/index/immediate/mixed-shader smoke checks also pass. Command:

```sh
clang -std=c11 -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-missing-field-initializers -Wno-deprecated-declarations -DWRATH_GRAPHICS_SMOKE_TEST -Isrc -Itools -Ithird_party/xboxrecomp/src -Ithird_party/xboxrecomp/src/platform -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 src/graphics.c src/nv2a_vertex.c src/nv2a_vertex_input.c src/nv2a_pixel.c third_party/xboxrecomp/src/d3d/d3d8_gl.c -L/opt/homebrew/lib -lSDL2 -lepoxy -framework OpenGL -o build/input/test_viewport_constants
build/input/test_viewport_constants > local/reports/viewport-constants-smoke.log 2>&1
```

Exit0. This component result establishes projection and update behavior; parent
owns the next actual logo captures and shared graphics.c integration commit.
