# Actual secondary vertex streams

Story-04 reaches the original space-station story and chamber scene. Its shader
has69 instructions and consumes v6 as FLOAT3 from stream1, offset0; primary stride
is56. Live c122=(1,0,0,1), so the earlier exact zero-weight proof correctly refuses
to discard the input. Story-05 confirms the concrete missing binding:

```
[wrath streams] bind1 stream1 handle0x15C9450 stride12 result0x8876086C caller0x3A6D1 swaps4104
```

`local/reports/story-05.log` and `story-04-shader.json` are the original execution
evidence. This is an actual nonnull vertex buffer rejected by the old stream0-only
bridge, not an unspecified retained-address case.

## Original Xbox4361 contracts

All addresses below come from the supplied XBE's D3D.asm/text.asm disassembly.

- Game A5F50 computes three-float deformation vectors per vertex, stores them at
  A6065/A606B/A6075, finishes the buffer at A60A1, and binds stream1 through
  A60B0 →3A6A0 →102580. The cached-buffer branch at A60C0 uses the same binding.
- Original SetStreamSource102580 takes(stream,buffer,stride), ret12. It addresses
  one of16 records at10F280+12*stream: stride,+4 byte offset,+8 guest buffer handle.
  It increments/decrements the resource binding count by0x80000, retains the
  byte offset, and marks dirty40 for unchanged stride or70 for changed stride.
- Original108F40 emits array formats using each declaration slot's own stream
  stride at108FD2–108FE4. Address generation10902B–109031 is buffer.Data plus
  declaration offset plus stream byte offset. The nonzero base path at109091–
  1090A7 additionally adds baseVertex times that stream's stride.
- Game3B010 binds the index buffer/base at3B08C and passes a literal uint16 index
  pointer to DrawIndexedVertices101BC0 at3B0AC. Every active vertex attribute uses
  the same index, base vertex and primitive order.

The native SetStreamSource now preserves all16 binding records and lifetimes.
Only stream0 is forwarded to the toolkit fixed-pipeline binding: that toolkit
has one stored vertex buffer, so forwarding stream1 there would overwrite it.
Programmable secondary streams instead feed actual native GL attribute buffers.

## Native fetch implementation

`src/vertex_fetch.inc` carries the original first vertex, optional little-endian
uint16 indices, and quad conversion order. `draw_vertices_data_fetch` propagates
this numbering from DrawVertices and DrawIndexedVertices to the shader bridge.
For each consumed secondary attribute, the bridge gathers the exact bytes from
its resource using:

```
resource.Data + streamByteOffset + declarationOffset
              + (baseVertex + originalIndex) * streamStride
```

Those bytes are uploaded to native GL; the original translated NV2A vertex shader
executes on the GPU unchanged. This expands data layout only; it does not evaluate
vertex shader math on the CPU or substitute geometry. Quad triangulation uses
0,1,2,0,2,3 consistently across all streams. Non-indexed draws propagate startVertex.
Stream0 also now respects the original stream byte-offset field.

Bounds are validated with64-bit arithmetic before gathering. Per-draw secondary
attribute uploads are capped at64MiB, and each temporary gather is released after
GL copies it. Zero stride repeats the same actual buffer element. A necessary
unbound stream still fails explicitly; the earlier exact dead-input proof remains
available only when live constants prove its value irrelevant. Unsupported packed
attribute conversions/tessellation remain explicit existing limitations.

## GPU regression

`tools/test_multistream.inc` creates independent real vertex buffers with distinct
strides, stream offsets and declaration offsets. FLOAT3 v6 supplies varying RGB to
a native GPU shader, so absent/wrong secondary values change measured pixels.
It checks reordered uint16 indices, nonzero base, non-indexed start, quad conversion,
zero stride, index/offset overflow, invalid stream16, repeated binding, and one
resource surviving until all its stream bindings are released.

The full combined native OpenGL smoke passes, including all previous indexed,
immediate, texture, mixed-shader, viewport, fog and resource-page tests. Evidence:
`local/reports/multistream-smoke.log`. Build with one compiler job:

```
clang -std=c11 -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-missing-field-initializers -Wno-deprecated-declarations -DWRATH_GRAPHICS_SMOKE_TEST -Isrc -Itools -Ithird_party/xboxrecomp/src -Ithird_party/xboxrecomp/src/platform -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 src/graphics.c src/nv2a_vertex.c src/nv2a_vertex_input.c src/nv2a_pixel.c third_party/xboxrecomp/src/d3d/d3d8_gl.c -L/opt/homebrew/lib -lSDL2 -lepoxy -framework OpenGL -o build/input/test_multistream
build/input/test_multistream > local/reports/multistream-smoke.log 2>&1
```

Parent owns the next game build/launch and confirmation that the actual character
scene advances. No additional SDK exclusion addresses are required:102580,
101B20 and101BC0 were already native bridge boundaries.

## Story08 range diagnostics

Story08 renders the chamber but rejects v6/stream1 (144 indexed vertices,
stream0 stride56). A nearby link message suggested88 instructions; the exact
Story09 stop instead confirms the69-instruction shader. Its retained secondary
buffer is1280 bytes with stride160; indices0..47 require7532 bytes. Captured
c122=(0,0,0,0) makes every use of v6 an exact zero multiplication. The strict
dead-input proof returns1 for the entire actual program and live constant bank.

The same proof now runs before secondary fetches regardless of whether the
retained handle is absent or bound. This fixes irrelevant stale-buffer rejection
without relaxing any required fetch bounds. The proof is recomputed on every
draw; GLSL and constant values are unchanged. Nonzero, near-zero, unsupported
arithmetic and other live uses still require valid real data.

Secondary-fetch and indexed-draw rejection messages are capped at16 each per
process. The secondary diagnostic reports the resource handle/type/data/size,
stream and declaration offsets, stride, effective index range, required byte end,
current c122, and exact dead-input proof result. `WRATH_BREAK_STREAM_ERROR=1`
stops at the first failed gather through `shader_error`, allowing the existing
shader probe to capture the actual state. This diagnostic does not alter bounds,
constants or shader code.

Validation passed in `local/reports/null-texture-smoke.log`: the GPU fixture uses
the observed1280-byte/stride160/index47 range and confirms expected pixels at
zero weight, strict rejection at1e-30 and1, and identical pixels after changing
back to−0. `local/reports/story09-dead-input-gpu.log` additionally exercises the
actual69 instructions and192 captured vectors on the native GPU. Transform
feedback compares all29 output components for two finite synthetic v6 inputs:
all match with captured zero weight and differ after live c122.x=1. Fixture data
are generated from the ignored capture, not embedded in committed test code.

To reproduce the captured-program component test after the combined smoke:

```sh
python3 - <<'PY'
import json, struct
j = json.load(open('local/reports/story-09-shader.json'))
open('local/reports/story09-vertex-words.bin', 'wb').write(
    struct.pack('<276I', *(w for row in j['words'] for w in row)))
open('local/reports/story09-vertex-constants.bin', 'wb').write(
    struct.pack('<768I', *j['vertex_constants_bits']))
PY
clang -std=c11 -O0 -Wall -Wextra -Werror -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 tools/tests/nv2a_vertex.c src/nv2a_vertex.c src/nv2a_vertex_input.c -L/opt/homebrew/lib -lSDL2 -lepoxy -o build/nv2a-vertex-test
build/nv2a-vertex-test local/reports/boot14-shader-objects.bin local/reports/story09-vertex-words.bin local/reports/story09-vertex-constants.bin > local/reports/story09-dead-input-gpu.log 2>&1
```
