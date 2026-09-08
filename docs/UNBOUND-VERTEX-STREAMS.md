# Reached unbound vertex input

Boot40's opt-in `WRATH_TRACE_STREAMS=1` records no SetStreamSource calls for
streams above0 before the failure. The actual69-instruction shader declares
v6 as FLOAT3, stream1, offset0; its live input mask is0x7F. Stream1 has zero
stride, offset and handle. Stream0 is bound with stride56. The exact trace is
`local/reports/boot-40-profile.log`, lines2619..2635. The declaration fixture is
`local/reports/boot-39-shader.json`.

There is therefore no missing buffer binding to recover at this draw. Original
SDK102580 does support multiple streams, storing stride/offset/handle records
at10F280+12*stream. The game's B7040 contains actual stream1 binds at
B758D/B7FA2/B80C7/B8461 with stride24; these paths were not reached before this
failure. Full native multistream support remains future work when required.

## Why a universal zero default is incorrect

Original108F40 writes an attribute's declaration format and its stream stride
at108FC1..108FE4. At109020..109029 (zero base) and109086..10908F (nonzero base)
it skips the address update for a null stream handle. That preserves the previous
GPU address; it does not prescribe a default attribute value. Pinned xemu
[gl/vertex.c](https://github.com/xemu-project/xemu/blob/fdfb5a8f481b2f870c57080e74ec8d3a31a47053/hw/xbox/nv2a/pgraph/gl/vertex.c)
also treats stride0 as reading the first element at the retained address and
supplying it as a constant attribute. Native compatibility does not yet retain
all those hardware array-address semantics. We do not substitute a general
zero vector for an unbound stream.

## Exact bounded proof

The captured shader reads v6 only at slot3:

```glsl
r4.xyz = (nv_mul(c122.xxxx, v6.xyzz) + r4.xyzz).xyz;
```

Captured c122 is all exact zero. Our established NV2A `nv_mul` contract returns
zero when either factor is zero, including the other factor being NaN/infinity.
Thus this input is immaterial to the result at the reached draw.

`nv2a_vertex_input_is_dead` conservatively examines every instruction read of
the specified input. It permits only MUL/MAD A or B whose other operand is a
direct constant swizzled entirely from exact-zero components. It rejects any
ILU read, addend/C read, other arithmetic operation, relative constant, unknown
opcode, missing FINAL or nonzero selected constant. Signed zero qualifies;
near-zero and NaN do not. It performs no vertex arithmetic or CPU execution of
shader instructions. It is a narrow proof of input independence.

`shader_bridge.inc` invokes the proof against live constants on every draw
before omitting an unbound secondary-stream fetch. The complete generated GLSL
continues to run unchanged on the native GPU. A later constant update can make
the input necessary again, which retains the existing explicit stream error.
There is no shader/program cache entry that remembers a stale zero weight.

## Validation

- `tools/tests/nv2a_vertex_input.c`: both multiply operand orders, exact/signed
  zero, near-zero, nonzero, NaN, swizzle selection, relative addressing, ILU,
  MAD addend, later MOV use and missing FINAL. Prior object/parser tests pass.
- `tools/tests/nv2a_vertex.c`: native GPU transform feedback runs the unchanged
  MAD with zero/default, differing finite and NaN/infinity input values. All
  produce identical output at zero weight; nonzero weight produces the expected
  changed result. Existing arithmetic, ARL, matrix-selection and actual shader
  compilation checks still pass.
- Optional actual69-instruction/constants fixtures prove v6 dead with captured
  c122.x=0 and necessary with c122.x=1e-30 or1.

Logs: `local/reports/nv2a-dead-input-proof.log` and
`local/reports/nv2a-dead-input-gpu.log`, both exit0. No game was launched for these
component tests. Parent owns the next actual boot and shared graphics.c commit.

```sh
python3 - <<'PY'
import json, struct
x = json.load(open('local/reports/boot35-shader.json'))
with open('local/reports/boot35-vertex-constants.bin', 'wb') as f:
    f.write(struct.pack('<768I', *x['vertex_constants_bits']))
PY
clang -std=c11 -O0 -Wall -Wextra -Werror tools/tests/nv2a_vertex_input.c src/nv2a_vertex_input.c -o build/nv2a-vertex-input-test
build/nv2a-vertex-input-test local/reports/boot14-shader-objects.bin > local/reports/nv2a-dead-input-proof.log 2>&1
clang -std=c11 -O0 -Wall -Wextra -Werror -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 tools/tests/nv2a_vertex.c src/nv2a_vertex.c src/nv2a_vertex_input.c -L/opt/homebrew/lib -lSDL2 -lepoxy -o build/nv2a-vertex-test
build/nv2a-vertex-test local/reports/boot14-shader-objects.bin local/reports/boot35-vertex-words.bin local/reports/boot35-vertex-constants.bin > local/reports/nv2a-dead-input-gpu.log 2>&1
```

## Later reached bound stream

Story-05 actually attempts stream1 binding with a real buffer and live nonzero
weight. The old stream0-only rejection was the cause; this is now implemented
with genuine indexed secondary fetches. See [MULTISTREAM-VERTICES.md](MULTISTREAM-VERTICES.md).
The exact dead-input proof above remains restricted to truly unbound, irrelevant
inputs; it is not used to replace the new live deformation stream.
