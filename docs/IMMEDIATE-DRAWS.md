# Native immediate drawing

Boot25 stops at SDK `101EC0` through game wrapper `3AE20` and caller `9F480`.
The runtime dump `local/reports/boot-25-begin.log` confirms primitive8 (quads),
FVF142 (XYZ, diffuse, one texture coordinate), and both program handles zero.
This SDK entry is **Begin**, not DrawIndexedVerticesUP: its sole stack argument
is written to NV097 method17FC and it returns with ret4. The original functions
are in `local/reports/disasm/asm/D3D.asm`; game calls are in `text.asm`.

| Address | Actual ABI | Relevant instructions |
| --- | --- | --- |
| `101EC0` | Begin(type), ret4 | Calls108EC0 to commit state, writes17FC=type at101EDC, sets device+8 bit800 at101EEA. |
| `101F00` | End(), ret0 | Writes17FC=0 at101F13, clears device+8 bits1800 at101F34. |
| `101E60` | SetVertexData4f(register,x,y,z,w), ret20 | Register−1 maps method1518 at101E7C; ordinary registers map1A00+16*register at101E83. Four DWORDs retain original float bits. |
| `101E20` | SetVertexData2f(register,x,y), ret12 | Emits1880+8*register at101E37; two float DWORDs. There is no special−1 alias in this entry. |

`9F480` sets FVF142 at9F48B, starts quads at9F559, sets diffuse register3
at9F6CC, texture register9 through2f, and emits four position register−1 values
with w1 before End at9F840. Wrappers are3AE20/30/40/60. These concrete calls,
not a PC D3D API assumption, determine the bridge.

`src/immediate_bridge.inc` retains sixteen current float4 attributes. Updating
position register0 or its special−1 alias snapshots one complete vertex; other
attributes persist without emitting. Begin/End retains the observed guest device
flags. End submits actual collected vertices through the common native draw
path, including quad triangulation. A bounded64MiB accumulator is freed after
each batch. Errors are latched so End cannot silently submit a truncated batch.

The position-emission behavior is independently documented in
[xemu's NV097 method handlers](https://github.com/xemu-project/xemu/blob/master/hw/xbox/nv2a/pgraph/pgraph.c):
SET_VERTEX4F finishes on the fourth position component, and generic4f finishes
when register0's fourth component arrives. Generic2f stores x,y with defaults
z0,w1 and finishes when register0's second component arrives. The bridge calls
native rendering directly; it does not execute or interpret GPU command streams.

The fixed path packs the active FVF from captured attributes: position0,
normal2, diffuse3, specular4, texture coordinates9..12. Actual game colors are
byte channels divided by255; packing returns those exact diffuse bytes. XYZ
positions with homogeneous w other than1 and unsupported layouts fail explicitly.
The programmed path supplies all attributes as float4 values using a temporary
native input declaration while retaining original shader instructions/constants;
the original declaration is restored after drawing.

`tools/test_immediate_bridge.inc` adds focused GPU checks to the graphics smoke:
FVF142 textured quads,2f UV updates,4f color updates, position−1/0 emission,
attribute snapshot persistence, guest Begin/End flags, and programmed float
register inputs with declaration restoration. The test is synthetic native API
validation, not actual title/menu evidence. The combined smoke passed after the
coordinated backend transform update, including the dedicated immediate GPU
PASS line. Actual process output is saved in
`local/reports/graphics-immediate-smoke.log` (exit0). The first attempted fixture
used pixel register3 instead of diffuse register4; correcting that fixture made
both fixed and programmed assertions pass, with no production change needed.

```
clang -std=c11 -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-missing-field-initializers -Wno-deprecated-declarations -DWRATH_GRAPHICS_SMOKE_TEST -Ithird_party/xboxrecomp/src -Ithird_party/xboxrecomp/src/platform $(/opt/homebrew/bin/sdl2-config --cflags) -I/opt/homebrew/include src/graphics.c src/nv2a_vertex.c src/nv2a_vertex_input.c src/nv2a_pixel.c third_party/xboxrecomp/src/d3d/d3d8_gl.c $(/opt/homebrew/bin/sdl2-config --libs) -L/opt/homebrew/lib -lepoxy -framework OpenGL -o build/input/test_graphics_immediate
build/input/test_graphics_immediate > local/reports/graphics-immediate-smoke.log 2>&1
```
