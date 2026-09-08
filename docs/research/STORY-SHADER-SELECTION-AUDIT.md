# Original chamber shader selection audit

Read-only audit, 2026-09-08. Original USA XBE, XDK4361; local original
`text.asm` and XBE section contents. No speculative opcode implementation or
shader replacement was made.

## Selection order and useful evidence

`B5A70(this,index,vertexClass)` has ECX pointing at manager`426CB8`, with two
stack arguments and `ret8`. It first stores selected index at manager+4 and
vertex class at+8. A20-byte record at `manager + index*20` contains VS handle+10,
PS handle+14, class+18, flags+1C and constant mode+20. The manager's previously
applied mode is at+0C.

If the selected record's mode differs, `B5A95` calls398C0/SetShaderConstantMode
before setting either shader. Only afterward doesB5B20 call398A0/SetVertexShader,
thenB5B30 call3ADD0/SetPixelShader. Therefore story07's69-instruction shader was
still the **preceding** bound shader when mode1 failed. That capture cannot
identify the shader the character was about to bind.

For the next stopped diagnostic, read manager426CB8's first16bytes and selected
record (index at426CBC; recordVS=`426CC8+index*20`,PS=`426CCC+index*20`,mode=
`426CD8+index*20`). Selected fields are written before the mode call and remain
useful even when the bound shader has not yet changed. CallerB7040 gets requested
index from signed word at draw object+2A (`B7075`) and class from its geometry
object+0C (`B707A`), then compares class against the record before binding.

Index−1 restores constant mode0,FVF152,pixel0. Index0 chooses FVF152/144/142 using
its class switch, then pixel0. Nonzero records choose their actual stored shader
handles. No generic guessed FVF should replace a missing shader.

## Static vertex-program coverage

InitializerB6850 creates shader records from XBE-resident program blobs through
B6780 or direct102440 calls. B6780's six arguments are index,VSdata,PSdata,
declarationClass,flags,constantMode. There are22 unique vertex blobs explicitly
pushed by this initializer. Each has a packed header with low16bits2078 and
high16bits instruction count, followed by that many four-DWORD NV2A slots.

| Blob address | Instruction count |
|---|---:|
| 1AA5E8 | 69 |
| 1AAA40 | 88 |
| 1AAFC8 | 61 |
| 1AB490 | 79 |
| 1ABA78 | 80 |
| 1ABF80 | 99 |
| 1AC5B8 | 36 |
| 1AC8F0 | 23 |
| 1ACB58 | 33 |
| 1ACF50 | 23 |
| 1AD1B8 | 21 |
| 1AD400 | 5 |
| 1AD458 | 12 |
| 1AD610 | 29 |
| 1AD7E8 | 25 |
| 1AD980 | 19 |
| 1ADAB8 | 47 |
| 1ADE68 | 13 |
| 1AE030 | 35 |
| 1AE358 | 31 |
| 1AE640 | 6 |
| 1AEB58 | 10 |

Scanning ILU=`(instructionDWORD1>>25)&7` across all these blobs finds **no ILU5,
6 or7**. Opcodes used are0..4, already covered by the existing generator. Thus
there is no evidence for adding EXP/LOG/LIT to solve this upcoming chamber path.
This is an opcode inventory, not a claim that all22 shaders have been generated,
linked or executed successfully.

Known mode1 helper-created records are1(VS1ADE68),8(1AD458),18(1AB490),
27(1AE358). Direct-created mode1 records include2,16,20,26. Selected record should
be captured to choose its actual program rather than inferring one from mode1.

Both declaration builderB57F0 and inline initializer declarations use FLOAT1
(format12),FLOAT2(22),FLOAT3(32),FLOAT4(42), and D3DCOLOR(40). These formats are
supported by `nv2a_vertex_input.c`. Stream1 declarations can provide v6..v15 as
FLOAT4 (160bytes/vertex) or smaller FLOAT3 deformation inputs. A supported format
does not prove the supplied buffer/stride/index range is valid. Story08 actually
reaches repeated stream1 range/allocation failures; those need live buffer-bound
analysis, not another format decoder or ILU opcode guessed from this inventory.

To reproduce without downloading/building: read originalB6850's push-immediate
addresses from`local/reports/disasm/asm/text.asm`; resolve those addresses through
`local/reports/default_analysis.json` sectionvirtual_addr/raw_addr/raw_size into
`local/assets/default.xbe`. Retain only headers whose low16bits are2078 and scan
exactly the high16bit instruction count beginning atblob+4. Do not scan random
asset bytes and call matching patterns shader evidence.
