# ARL and relative vertex constants

Boot35 stopped at the actual 69-instruction vertex program selected after782
swaps. The ignored fixture `local/reports/boot35-shader.json` contains its words
and all192 constant vectors. MAC13 (ARL) appears at slots5,11,17,24,30,36. All
six have zero vector write masks; slot5 pairs an ILU MOV to texture output0.
The following three instructions read physical constants124,125,126 relative
to A0. Position uses DP4; normal uses DP3.

`src/nv2a_vertex.c` now generates native GPU GLSL for this reached operation.
ARL consumes only source A after its swizzle/negation and assigns `floor(A.x)`
to A0. It writes A0 even with a zero vector mask and never modifies the encoded
temporary destination. All A/B/C values are read before committing A0, so a
paired ILU observes the previous address. ARL routed to a vector output remains
an explicit unsupported error. No epsilon or approximate integer correction is
added to the floor.

Relative reads add the instruction's physical constant index to A0. There is
no second SDK +96 correction. Metadata marks all192 constants potentially used
and sets `relative_constants`. The shader bridge already uploads the full bank.
A0 is stored as an integral-valued float internally, allowing the range check
before a GLSL float-to-int conversion; every index that can reach the192-entry
bank is exactly representable. The index converts to int only after checking
0<=index<192. Values outside return zero, without wrap or clamp. This follows
the NVIDIA contract extended to this bank size; **NV2A hardware behavior for
out-of-range reads is unverified**. Nonfinite addresses also take that bounded
zero path. These exceptional cases are not evidence for Xbox numerical parity.

## Sources and limits

- NVIDIA [NV_vertex_program revision1.10](https://registry.khronos.org/OpenGL/extensions/NV/NV_vertex_program.txt),
  sections2.14.1.10.1 and2.14.1.9, specifies floor toward negative infinity,
  signed A0, relative parameter reads and zero outside its96-entry bank.
  It is the primary instruction contract, not a measurement of Xbox silicon.
- xemu [vsh-prog.c at fdfb5a8f481b2f870c57080e74ec8d3a31a47053](https://github.com/xemu-project/xemu/blob/fdfb5a8f481b2f870c57080e74ec8d3a31a47053/hw/xbox/nv2a/pgraph/glsl/vsh-prog.c)
  confirms MAC13 uses only A, writes A0 independently of vector masks, and
  delays a paired address write until its ILU has read old state. Its ARL adds
  0.001 as a normalization workaround; that heuristic was deliberately not
  adopted. The separate downloaded nv2a_vsh_cpu implementation uses the same
  bias and is not independent evidence for exact floor on Xbox hardware.

Captured c114.z is exactly3. Each address is therefore floor(3*v2.x/y/z),
repeated for position and normal. Constants124..171 contain sixteen3-row
matrices. This suggests bone indices0..15 and row addresses124..171, but the
fixture does not contain vertex input bytes/declaration: that range is an
inference until the actual v2 values are captured. The test below proves the
native path for those indices; it does not substitute test vertices for game
vertices or assert that the next game frame is correct.

## Validation

`tools/tests/nv2a_vertex.c` retains prior paired arithmetic/output regressions
and adds native GL4.1 transform-feedback checks:

- Negative/fractional/near-integer floor inputs, source swizzle and negation.
- Paired ILU constant reads use old A0 on both an initial and later ARL;
  zero/nonzero vector masks leave an existing temporary unchanged.
- Physical first/last constant entries and bounded out-of-range zero behavior.
- Sixteen matrix selections using multiply-by3, relative B-source DP4/DP3,
  and simultaneous ARL/texture output, matching the reached instruction pattern.
- The actual69-instruction fixture generates and compiles on the native GPU.

Test log: `local/reports/nv2a-vertex-arl-test.log`, exit0. Generated actual shader:
`local/reports/boot35-vertex.glsl`. No game process or full game build was run.

```sh
python3 - <<'PY'
import json, struct
x = json.load(open('local/reports/boot35-shader.json'))
with open('local/reports/boot35-vertex-words.bin', 'wb') as f:
    for words in x['words']:
        f.write(struct.pack('<4I', *words))
PY
clang -std=c11 -O0 -Wall -Wextra -Werror -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 tools/tests/nv2a_vertex.c src/nv2a_vertex.c -L/opt/homebrew/lib -lSDL2 -lepoxy -o build/nv2a-vertex-test
build/nv2a-vertex-test local/reports/boot14-shader-objects.bin local/reports/boot35-vertex-words.bin > local/reports/nv2a-vertex-arl-test.log 2>&1
```
