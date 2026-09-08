# Disabled morph streams in the original hub

Story10 reaches `levels\b\hub\hub` after skipping the original story through
the game's input path. Its88-instruction vertex shader declares v6–v15 as ten
FLOAT4 values in stream1, stride160. The retained buffer contains1600 bytes,
but this regular mesh has13 vertices and an indexed draw of45 entries including
index12. Even the first secondary attribute would require1936 bytes. The native
bounds rejection is correct for a live input; reading beyond that buffer is not
an acceptable fix.

Exact evidence is in ignored `local/reports/story-10-shader.json` and the first
rejection in `story-10.log`. Unlike the previous69-instruction shader, this
program reads each secondary input's W into A0 and uses relative constant weights.
The earlier proof of immediate multiplication by zero therefore correctly refuses.

## Original program and producer

All addresses are from the supplied Xbox4361 XBE's D3D.asm/text.asm disassembly.

- Shader slots2–21 accumulate the ten morph displacements in r2, selecting each
  weight with ARL from that input's W. Slot23 is exactly
  `r4.xyz = c122.x * r2.xyz + v0.xyz`. Captured c122 is allzero.
- The next ARL at slot25 overwrites the address dependency before bone-matrix
  relative reads. The final outputs have no dependence on the discarded morph
  calculation, including its temporary registers and address selections.
- Game A8190's non-morph branch A81AB calls B6310 and binds only stream0 at
  A81C5. B6310's B63BB–B63D0 path sets logical constant26/physical c122 tozero
  without unbinding stream1. This is the intended morph-disable mechanism.
- The earlier buffer producer A8940 reads vertex count at A8B70, computes
  `count*160`, allocates at A8B82 through39ED0→100D70, and stores the buffer in
  `(model+4C)+10`. The160-byte stride describes ten FLOAT4 influences per vertex;
  it is not an instance divisor, separate index space or constant table.
- Original108F40 address formation and102580 stream records match the native
  fetch formula. No stream offset/index or resource-size correction was warranted.

## Conservative dependency proof

`nv2a_vertex_dead_input_mask` tracks16 input-dependency bits and an exact-zero
fact per component of temporaries and outputs, plus the A0 dependency. It does
not compute positions, weights, matrix values, arithmetic results or addresses.
Direct constant loads inspect live components only for exact zero. A relative
load conservatively inherits the old A0 dependency and no known-zero fact.

Only the existing native `nv_mul` rule removes a dependency: either operand
being exactly zero produces zero even with NaN/Inf in the other operand. MUL and
the multiplication part of MAD use this rule. Dot products, DST and other
operations conservatively retain dependencies; there is no algebraic cancellation
or near-zero tolerance. MOV/masks can propagate or overwrite component facts.

Sources are snapshotted before all writes. The proof mirrors the GLSL compiler's
output-first/ILU/MAC write ordering, R12=oPos alias, paired ILU R1 destination and
MAC R1 suppression, ARL update, and fog's first-enabled-component selection.
Unknown opcodes, invalid registers, missing FINAL and writable-constant/state
outputs fail closed. Final output dependencies determine the complement mask.

The existing direct proof remains the fast path. If it fails, the shader bridge
computes the complete transitive mask lazily once per draw for all secondary
attributes. Constants are read afresh on each draw; no cache can survive a changed
weight. Required fetches retain the same bounds and resource checks. GLSL, game
constants and game progression are unchanged.

## Validation

- `transitive-input-proof.log`: CPU cases cover exactzero, nearzero/NaN,
  observable writes, partial masks, R12, paired old A0, address overwrite,
  paired R1 suppression, fog component selection and unsupported state/opcodes.
- `transitive-input-smoke.log`: the full native graphics suite passes. A retained
  too-small secondary buffer feeds a MOV→MAD chain that defeats the direct proof.
  Zero weight renders measured RGB, tiny1e-30 and1 both reject the required
  out-of-bounds fetch, and−0 restores the identical pixels.
- `story10-dead-input-gpu.log`: actual88 instructions and all192 captured vectors
  run on the native GPU. For every v6–v15, the fixture changes XYZ and W (ARL
  index2→4), then compares all29 transform-feedback components. They are identical
  at captured zero weight and differ at live weight1. The CPU mask marks allten
  inputs necessary for tiny1e-30 as well. Test vectors are synthetic; the original
  shader/constants are loaded from ignored captures rather than committed assets.

Both root and mac_runtime independently reviewed the proof against the generator
and found no concrete soundness mismatch. The parent owns the next actual hub run.

Generate `story10-vertex-words.bin`/`story10-vertex-constants.bin` using the JSON
extraction in MULTISTREAM-VERTICES.md with352 instruction DWORDs, then run the same
`build/nv2a-vertex-test` command with those paths. The general capture harness now
accepts the69- and88-instruction programs. No SDK exclusion or build change is needed.
