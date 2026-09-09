# Registered pixel-shader coverage

Bounded audit of original `B6850` found19 static pixel definitions:18 registrations
through B6780 and one direct CreatePixelShader102B40 call. The current native
generator accepts all19 with each of the four fog/specular option combinations
(76/76 successful generations). This checks supported equations and validation;
it is not76 GPU rendering tests or a claim that every material binding works.

The only registered dependent-texture family is shader1B, already reached in the
hub and implemented for Story14. There is no additional unsupported texture mode,
dot mapping or combiner operation in this inventory.

All IDs below are hexadecimal. Every definition has final flags1FF, which include
the original texture-mode adjustment flag. All unspecified dot mappings/input
selectors arezero. Definitions use between1 and8 combiners.

| Modes per stage | Shader IDs | Native generator result |
|---|---|---|
| PROJECT2D, PROJECT2D, PROJECT2D, NONE | 03,0A,11,12,1D | All pass |
| PROJECT2D, PROJECT2D, NONE, NONE | 01,04,06,08,15,16,17,1E,21 | All pass |
| PROJECT2D on all four stages | 1C,1F,20 | All pass |
| PROJECT2D, NONE, NONE, NONE | 22 | Pass |
| PROJECT2D, DOTPRODUCT, DOTPRODUCT, DOT_REFLECT_SPECULAR | 1B | Pass; dot mapping111, input selection0 |

Actual hub shader01's definition is1ADF40; reflection shader1B is1AE550. Their
registration and live matching are recorded in PROGRAMMABLE-TEXTURE-MODES.md and
DOT-REFLECTION-SHADER.md. Arctic Antics material membership was not determined
by this static registration audit; no level assets were decoded.

## Separate pipeline risk

Shader06 is the one PS-only registration: B6D34 supplies PS1ACE60, B6D47 explicitly
zeros VS handle426D40, and B6D5F creates its PS in426D44. Manager B5A70's B5B1E
path selects vertex shader0 before selecting the programmed pixel shader. The
current bridge rejects FVF0 and fixed-vertex/programmed-pixel pairing. Its pixel
equations themselves pass generation. Material byte5F selects this ID through
B63F0's jump-table branch B6514, subject to model-format and override checks.
This is a concrete remaining setup combination, but no live trace proves it is
needed by the hub or Arctic Antics. Investigate only if that actual path is reached.

Remaining dynamic concerns such as ADDRESS=5, texture resource dimensionality,
fixed-stage state changes and vertex formats are outside static PS definitions.
The current live address-mode blocker remains the other agent's assigned work.

## Reproduction/evidence

`local/reports/static-pixel-shader-audit.json` records registration call address,
ID, VS/PS pointers, modes, mapping fields, counts and all76 generation results.
The audit parsed the push arguments immediately before B6780/102B40 calls within
`local/reports/disasm/asm/text.asm`'s B6850 function, resolving data through the
supplied XBE section headers. B6780's actual stack offsets establish the argument
order; the direct PS-only destination gives ID6 using `(426D44−426CCC)/20`.

The original definition bytes were passed unchanged to `nv2a_pixel_generate`
through a tiny local library built with one compiler job:

```sh
clang -std=c11 -O1 -Wall -Wextra -Werror -dynamiclib src/nv2a_pixel.c -o build/pixel-audit.dylib
```

No game, full build or asset viewer was launched. No production source changed.
