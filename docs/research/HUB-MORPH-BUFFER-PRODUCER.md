# Story10 retained morph stream audit

Read-only audit of supplied original disassembly and native bridge source. No
source change, compiler, debugger attachment or game launch was used.

The Story10 stop is stream1 handle1B348F0, data1B3DC00, bytes1600, stride160;
slot6 FLOAT4 at offset0 requires end1936 for the current mesh's indices0..12.
Physical constantc122 is zero. The shader has88 instructions. The controller
agent owns the transitive shader-dependency proof; this report independently
checks the original producer/allocation/binding side.

## Original allocation and binding

All addresses below are from ignored `local/reports/disasm/asm/text.asm`.

- A8940 builds morph data. A8B65 checks that the maximum nonzero morph influences
  per vertex is no greater than10; the larger case branches to A8EF5.
- A8B70 reads model+14 (vertex count); A8B73 forms5*n and A8B7A shifts by5, giving
  n*160 bytes. A8B82 calls39ED0 with that exact byte count. A8B8A stores the returned
  game buffer-registry handle in (model+4C)+10, the morph buffer field.
-39ED0 at39F28 reads the requested byte count, passes it directly as the first
  argument of100D70 at39F3D/39F3E, and records it in the original registry at
  1E8D90+slot*32. Native100D70 likewise records exactly arg0 in Resource.bytes and
  allocates that many data bytes; there is no observed native divide/stride loss.
- Active morph render A5F50 reads this model morph field. If its +10 buffer exists,
  A60BB pushes format3, A60BD the buffer handle, A60BE stream1, then calls3A6A0.
-3A6A0 calls9A510 to map format to byte stride and passes the registry's actual
  native/Xbox resource handle to102580. Inspecting the original9A58C/9A564 switch
  tables proves format3 is the only format producing stride160 (9A55A).

Therefore160 describes one vertex record containing up to10 FLOAT4 morph entries,
not a stream-frequency divisor or a global10-element matrix palette.1600 is
consistent with a previously drawn ten-vertex morph mesh. The snapshot lacks an
allocation history linking this exact handle to its producing model, so the prior
model's identity/count is an inference, not a captured fact.

## Why the smaller buffer can remain bound

The captured main draw stack is A8190→A8240→9F110→A0990→A8140→87BF0. Within A8190,
only the active morph branch A81A4 calls A5F50. The ordinary branch A81AB calls
B7040, B6310, and binds stream0 atA81C5; it never unbinds stream1.

B6310 checks model+4C and instance+20. If either is absent, B63BB pushes a zero
vector and B63CE passes logical constant26 to3AD50. The +96 constant-bank mapping
makes this physicalc122, exactly the captured zero vector. In active morph paths,
B6338..B63A4 uploads30 influence weights to logicalconstants-30..-1 and sets
logical26.x=1. The regular path therefore disables morphing through a shader
constant while retaining the previously bound secondary resource.

This supports the controller agent's proposed faithful fix: prove all secondary
input dependencies vanish before observable shader outputs when the actual
constant is exactly zero. It does not justify reading outside the buffer,
changing the supplied index values, making a larger allocation, ignoring nonzero
weights, or unconditionally treating the secondary stream as a palette. The
current original regular mesh can have more vertices than the retained morph
mesh; its stream1 data must be irrelevant when the original shader disables it.

Next validation belongs to the shader agent: compare the captured88-instruction
program's GPU output with varied secondary data at exactzero weight, while proving
nonzero/tiny-nonzero weights still require valid bounded data. No change to
allocation or native Resource.bytes is warranted by this audit.
