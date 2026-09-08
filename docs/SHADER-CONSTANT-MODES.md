# Xbox4361 shader constant modes

Story-07 successfully renders the space station/corridor, then reaches
SetShaderConstantMode1026F0(mode1) at the character scene. The earlier bridge
accepted only0 and incorrectly described that as the192-constant mode.

The [Cxbx Xbox type definitions](https://github.com/Cxbx-Reloaded/Cxbx-Reloaded/blob/d6d63da6eadbfeba382f41389595a975569da244/src/core/hle/D3D8/XbD3D8Types.h)
identify0 as96CONSTANTS,1 as192CONSTANTS, and bit10 as NORESERVEDCONSTANTS.
The supplied original4361 executable independently establishes the behavior:

- 1026F4 tests input bit10, reflected into device+8 bit200. The current native
  implementation supports the reached modes0/1; other mode/flag values still fail.
- 102710 removes that flag,102716 stores the remaining mode at device+2018,
  and10271C immediately branches to return for nonzero mode. Thus mode1 changes
  ownership of the shared range without clearing/remapping constants, changing
  dirty flags or updating viewport aliases.
- Mode0 marks dirty1600 at102722 and emits actual hardware methods restoring
  fixed-pipeline defaults. Native compatibility now mirrors these bank writes.
- SetVertexShaderConstant102AA0 always adds96 at102AB4, without inspecting the
  mode. Its physical bank is always192 vectors; mode0's lower range is shared
  with fixed-pipeline state. The signed API index mapping is unchanged.
- Deferred fixed texture-transform1081F9, lighting1089C0 and matrix108CEF paths
  skip their updates when device+2018 is1. Native programmable draws already use
  the explicit192-vector GPU uniform bank, independent of fixed-pipeline matrices.

## Returning to mode0

Original102738 selects constant upload index60, then copies12 DWORDs from10B208:

| Physical vector | Restored value |
|---|---|
| c60 | (0, .5, 1, 2) |
| c61 | (-1, 0, 1, 2) |
| c62 | (0, 0, -1, 0) |

Calls to103030 emit the transposed identity table at10B1B8 into texgen plane
methods840/880/8C0/900. These alias physical c64–67,c72–75,c80–83,c88–91.
Method9D0 then restores fog plane c57=(0,0,1,0). This mapping follows pinned xemu
[method implementations](https://github.com/xemu-project/xemu/blob/fdfb5a8f481b2f870c57080e74ec8d3a31a47053/hw/xbox/nv2a/pgraph/pgraph.c)
and [register aliases](https://github.com/xemu-project/xemu/blob/fdfb5a8f481b2f870c57080e74ec8d3a31a47053/hw/xbox/nv2a/nv2a_regs.h).
Both tables were checked against the supplied XBE bytes. Production reads the
actual mapped tables rather than maintaining a second hardcoded copy.

These are hardware bank writes, so the SDK CPU readback cache at device+B98 is
left intact. Neither mode switch emits viewport methods: explicit c58/c59 values
persist until SetViewport, shader selection or a target change emits the existing
viewport aliases. The NORESERVED flag is deliberately still unsupported here;
its separate original FE2B0 branch must be implemented if reached.

## Validation

`tools/test_shader_constant_mode.inc` checks that mode1 preserves all192 vectors,
clears only the supported flag bit, stores mode1 and leaves dirty bits unchanged.
It writes through signed negative API indices, renders actual c60 as RGB on the
native GPU, returns to mode0 and verifies the changed GPU color plus every restored
bank alias. Lower unrelated constants, viewport aliases and the CPU cache retain
their appropriate values. The smoke fixture supplies the exact original identity
and default tables because its empty test RAM contains no XBE.

Run the combined graphics smoke command documented in MULTISTREAM-VERTICES.md;
the constant-mode fixture is included automatically. Parent owns the next actual
story run and confirmation that the character scene advances. No new native
boundary or SDK exclusion is needed.
