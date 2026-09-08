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

Story08 renders the chamber but first rejects v6/stream1 on a newly linked
88-instruction program (144 indexed vertices, stream0 stride56). This differs
from the earlier supported69-instruction shader. The existing exact zero-use
proof currently bypasses fetches only when the secondary handle is absent; a
stale bound buffer could therefore still reject even if its input is irrelevant.
That is a hypothesis until the next live range/constant capture.

Secondary-fetch and indexed-draw rejection messages are capped at16 each per
process. The secondary diagnostic reports the resource handle/type/data/size,
stream and declaration offsets, stride, effective index range, required byte end,
current c122, and exact dead-input proof result. `WRATH_BREAK_STREAM_ERROR=1`
stops at the first failed gather through `shader_error`, allowing the existing
shader probe to capture the actual state. This diagnostic does not alter bounds,
fetches, constants, shader code or game progression.
