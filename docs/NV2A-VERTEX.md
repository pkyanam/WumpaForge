# Native NV2A vertex-program compiler

`src/nv2a_vertex.c` converts a bounded array of four-DWORD NV2A instructions into
GLSL 4.10 source without a GL context or CPU execution of shader instructions.
The native OpenGL driver compiles and runs the resulting GPU shader. The caller
owns source/error buffers; errors clear partial source and identify the exact
instruction and unsupported field/opcode. Metadata reports input/output masks,
physical constant-register use and consumed instruction count.

## Evidence and provenance

The existing toolkit's `d3d8_vsh.c` decoder is unsuitable: it extracts MAC/ILU
opcodes from word 0, whereas the verified NV2A layout puts them in word 1.
Sources inspected and retained under ignored `local/reports/`:

- xemu revision `fdfb5a8f481b2f870c57080e74ec8d3a31a47053`:
  [vsh-prog.c](https://github.com/xemu-project/xemu/blob/fdfb5a8f481b2f870c57080e74ec8d3a31a47053/hw/xbox/nv2a/pgraph/glsl/vsh-prog.c)
  provides instruction fields, paired issue rules, output-register mapping,
  scalar/fog behavior and NV2A screen-space output treatment;
  [vsh.c](https://github.com/xemu-project/xemu/blob/fdfb5a8f481b2f870c57080e74ec8d3a31a47053/hw/xbox/nv2a/pgraph/glsl/vsh.c)
  supplies initial register values, W clamping, 1/16-pixel truncation and final
  OpenGL depth conversion;
  [common.c](https://github.com/xemu-project/xemu/blob/fdfb5a8f481b2f870c57080e74ec8d3a31a47053/hw/xbox/nv2a/pgraph/glsl/common.c)
  confirms integer depth ranges 0..65535 and 0..16777215 for Z16/Z24S8.
- envytools revision `f102b82381f3f11cee113d16374c87091db039d9`:
  [nv20_3d.xml](https://github.com/envytools/envytools/blob/f102b82381f3f11cee113d16374c87091db039d9/rnndb/graph/nv20_3d.xml)
  confirms transform-program upload method 0x0B00 and constant-upload methods
  0x0B80/0x1EA4. Its generic NV20 vertex format table is incomplete for Xbox;
  do not infer Xbox D3DCOLOR semantics solely from that table.
- Cxbx revision `96aabe72e238674a22a61695fb6f5295259edf62`:
  [XbD3D8Types.h](https://github.com/Cxbx-Reloaded/Cxbx-Reloaded-legacy/blob/96aabe72e238674a22a61695fb6f5295259edf62/src/core/hle/D3D8/XbD3D8Types.h)
  corroborates the shader object/input-slot layout, declaration formats,
  maximum 136 instructions and signed-to-physical constant correction +96.

The new compiler is authored here, with these reference implementations used
for format and execution semantics. Its source conservatively retains the
xemu shader translator's `GPL-2.0-only OR GPL-3.0-only` licensing and attribution
to espes, Jannik Vogel, Matt Borgerson and the listed Cxbx/Dxbx antecedents.
No emulator executable, CPU interpreter, QEMU library or source dependency is
linked. Envytools/Cxbx were read as corroborating references; their source files
were not copied into the build. The downloaded references and game shader dump
remain ignored diagnostic material. A future distributed source package must
include the chosen GPL text and comply with its source-distribution terms.

## Instruction contract

Word 0 is not an opcode word. Key fields are:

| Field | Word | Bits |
| --- | --- | --- |
| ILU opcode | 1 | 27:25 |
| MAC opcode | 1 | 24:21 |
| Constant index | 1 | 20:13 |
| Vertex input index | 1 | 12:9 |
| A negate/swizzle | 1 | 8 / 7:0 |
| A temporary/mux | 2 | 31:28 / 27:26 |
| B negate/swizzle/temp/mux | 2 | 25 / 24:17 / 16:13 / 12:11 |
| C negate/swizzle | 2 | 10 / 9:2 |
| C temporary | 2 and 3 | word2 bits1:0 are high; word3 bits31:30 low |
| C mux | 3 | 29:28 |
| MAC mask / shared temporary | 3 | 27:24 / 23:20 |
| ILU mask / output mask | 3 | 19:16 / 15:12 |
| Output bank/address/mux | 3 | 11 / 10:3 / 2 |
| Relative addressing / FINAL | 3 | 1 / 0 |

Register mux 1/2/3 selects temporary/input/constant. Masks use bit3=x through
bit0=w. Paired MAC/ILU source values are captured before writes; paired ILU writes
R1, and paired MAC writes targeting R1 are suppressed. R12 aliases oPos. Output
registers are 0=position, 3/4=front colors, 5=fog, 6=point size, 9..12=texcoords.
Fog uses the most significant masked source component as its scalar value.

Supported MAC instructions: NOP, MOV, MUL, ADD, MAD, DP3, DPH, DP4, DST, MIN,
MAX, SLT, SGE, ARL. Physical constant reads may use A0-relative addressing;
see [ARL and relative constants](NV2A-VERTEX-RELATIVE.md) for its bounds and tests.
Supported ILU: NOP, MOV, RCP, RCC, RSQ. EXP/LOG/LIT, writable constant/state shaders and backface-color outputs are
explicitly rejected. These can be implemented when a real program requires
them. GPU floating-point results are not claimed bit-identical for every NaN,
denormal or precision corner case. Multiplication preserves observed zero times
infinity/NaN => zero behavior; RCC uses the documented signed 2^-64..2^64 clamp.

GLSL inputs are `layout(location=N) in vec4 vN` for N=0..15. Uniforms are
`vec4 u_vconstants[192]`, `vec4 u_nv2a_viewport` (x,y,width,height) and
`vec2 u_nv2a_depth` (integer-range viewport minimum and maximum). Varyings agreed
with the pixel compiler are vec4 vD0/vD1/vT0/vT1/vT2/vT3 and float vFog.

NV2A oPos is already in screen coordinates. The compiler reverses the supplied
native viewport/depth range, flips screen Y for the native top-left convention,
and multiplies NDC xyz by oPos.w before writing gl_Position. It preserves that
W, rather than replacing it with a reciprocal. W initially equals 1. The caller
must provide a positive viewport size and nonzero depth span; the generator
does not guess these runtime values. Default D24S8 depth is (0,16777215).

## Actual startup object and declaration

`local/reports/boot14-shader-objects.bin` contains guest 0x02606000..0x02608000.
The observed vertex handle is 0x02606701; subtract its low tag bit to obtain
object 0x02606700. Original `CreateVertexShader` at 0x00102440 allocates and fills:

| Offset | Meaning | Observed value |
| --- | --- | --- |
| 0x00 | Reference count | 1 |
| 0x04 | Flags | 0x10 (has program) |
| 0x08 | Instruction count | 6 |
| 0x0C | Program/constants packet DWORD count | 25 |
| 0x10 | Four dimensionality bytes | 0 |
| 0x14 | Sixteen 16-byte attribute slots | below |
| 0x114 | First GPU upload packet header | 0x00600B00 |
| 0x118 | First actual four-DWORD instruction | six instructions follow |

The 0x00600B00 header requests 24 DWORDs at method 0x0B00, matching six
instructions. Larger objects can contain multiple upload packets plus constant
packets; callers must parse packet headers, not treat every DWORD after 0x118
as instruction data. CreateVertexShader writes a zero packet terminator after
the recorded packet count. This object has no embedded constant packets.

Each slot starts at object+0x14+16*slot and contains DWORD stream index, DWORD
byte offset, DWORD format, byte tessellation type, byte tessellation source,
two padding bytes. The original declaration parser 0x001021A0 writes those first
three members at 0x00102203..0x0010222A; initialization at 0x001024D0 sets every
format to 0x02 (NONE) before parsing.

| Slot | Stream | Byte offset | Format | Native interpretation |
| --- | --- | --- | --- | --- |
| v0 | 0 | 0 | 0x32 | float3, implicit w=1 |
| v1 | 0 | 12 | 0x12 | float1, implicit y=z=0,w=1 |
| v2 | 0 | 16 | 0x40 | packed BGRA unsigned bytes normalized to RGBA |
| v3 | 0 | 20 | 0x22 | float2, implicit z=0,w=1 |
| v4..v15 | 0 | 0 | 0x02 | absent |

The minimum stride is 28 bytes; use the actual stream binding's stride. Shader
input numbers are arbitrary: this program reads v0 position and v3 UV, so v3
must not be treated as a fixed-function diffuse-color semantic. All observed
tessellation flags/sources are zero. Nonzero tessellation needs explicit handling.

The actual six instructions use MAC MOV, four MADs, ADD, and one paired ILU
MOV. They read physical constants 112/113/119/120 (SDK indices16/17/23/24), write
oPos.xyz and oT0..oT3.xy, and leave initial oPos.w=1. The generated native shader
is retained in `local/reports/boot14-vertex.glsl`.

## Constant setter ABI

The real entry is 0x00102AA0: three stack arguments `(signedIndex, dataPointer,
vectorCount)`, stdcall `ret 12`. It adds 96 to the signed SDK index, copies
`vectorCount*16` bytes into `device+0xB98+physicalIndex*16` unless device flags
at +8 include 0x10 (pure device), then builds the GPU constant upload. The game
wrapper 0x0003AD50 and SDK wrapper 0x00101705 call it directly. Address 0x00102A70
is a `ret 4` inside SetVertexShader, not a fast constant setter. No independently
called fast variant is evidenced by the current supplied-XBE disassembly.

`local/reports/boot14-d3d-device.bin` has zero cache values at the relevant physical
indices. Capture constants through the actual setter hook; a pure-device cache
snapshot cannot be the sole source. Preserve signed-index interpretation and
validate the corrected range before uploading to the native shader.

## Validation

`tools/tests/nv2a_vertex.c` uses synthetic instructions and native GPU transform
feedback to check numeric paired MOV/MAD/ADD results, swizzles/masks, R12 alias,
constant upload, viewport/depth conversion and W preservation. It also rejects
malformed opcodes, missing FINAL and undersized buffers. ARL and relative reads
have additional numeric GPU regressions documented in the linked followup.
The optional ignored real object fixture compiled successfully on this M3's
OpenGL 4.1 context. Output: `local/reports/nv2a-vertex-test.log`.

```sh
clang -std=c11 -O0 -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 tools/tests/nv2a_vertex.c src/nv2a_vertex.c src/nv2a_vertex_input.c -L/opt/homebrew/lib -lSDL2 -lepoxy -o build/nv2a-vertex-test
build/nv2a-vertex-test local/reports/boot14-shader-objects.bin
```

These tests establish generator behavior and native GPU compilation, not a
complete rendered-game milestone; parent integration supplies that check.

## Bounded object extraction helper

`src/nv2a_vertex_input.{h,c}` now implements the object/packet parsing described
above independently of GL and guest execution. `nv2a_vertex_object_decode`
accepts untagged object bytes plus their available length and returns a
`Nv2aVertexObject` with sixteen decoded slots, contiguous `words[136][4]`, and
physical embedded constant words/presence masks. The caller applies those
constants when loading the object and supplies actual stream strides/bounds.
It must still implement or reject each declared attribute kind explicitly.

The parser follows the actual 4361 packet writers: `0x00102130` emits at most
32 DWORDs (eight instructions) per 0x0B00 packet; `0x00102340` emits a one-DWORD
0x1EA4 physical constant selector and at most 32 DWORDs per 0x0B80 data packet.
It supports consecutive data packets with automatic constant-index advance and
later packets overwriting earlier constants. Packet headers are stripped before
passing microcode to the GLSL generator. No constant payload is silently skipped.

Length checks include the terminator at `0x114 + packet_dwords*4`. The declared
instruction count must match extracted program data exactly. Counts above 136
instructions or 4096 packet DWORDs are rejected; the latter bound exceeds a
maximum program plus all 192 physical constants uploaded individually. Invalid
selectors, payload lengths, methods/subchannels/flags, constant ranges, stream
indices, overflowing offsets, unknown formats and tessellation are errors. State
or read/write shader object flags are rejected. Partial result structures are
not exposed on failure. The microcode generator separately validates opcodes
and FINAL termination.

The format helper decodes float1..4, normalized/unscaled signed short1..4,
normalized byte1..4, BGRA8, packed signed 11/11/10, float2H and NONE. Packed
11/11/10 requires unpacking; float2H stores x,y,w as three floats and requires
expansion to x,y,0,w. Returning these distinct kinds does not claim that the
native draw path already supports their conversions.

`tools/tests/nv2a_vertex_input.c` passed deliberately unaligned input, synthetic
1/8/9/136-instruction programs, split and repeated constants through physical
c191, declaration byte sizes, overflow/truncation/malformed cases, and the
ignored actual startup object. Output: `local/reports/nv2a-vertex-input-test.log`.

```sh
clang -std=c11 -O0 -Wall -Wextra tools/tests/nv2a_vertex_input.c src/nv2a_vertex_input.c -o build/nv2a-vertex-input-test
build/nv2a-vertex-input-test local/reports/boot14-shader-objects.bin
```

## Fog uniform contract

The programmable epilogue consumes oFog.x as distance. Callers set
`u_nv2a_fog_mode` (0 disabled,1 linear/pass-through,2 exp,3 exp2) and
`u_nv2a_fog_params` (bias,slope). Factors remain unclamped until fragment
interpolation. See NATIVE-FOG.md for original SDK coefficients, special values,
licensing references and native GPU tests.
