# Render defaults audit — 2026-09-08

Read-only comparison of original104330 against current native CreateDevice and
backend defaults. No source modification, build, or game launch. Complete extracted
table: ignored `local/reports/render-defaults-4361.csv` (89 entries).

## Exact initialization contract

104351 starts state57 and104356 selects DWORD table10BDC0. The loop through104386
sets states57..145 using FD860, skipping offsets0xEC/0x138 (states116/135), whose
table values are DEADBEEF sentinels. 104388..104398 then sets state124 from whether
device+2088 holds a depth surface; its table value2 is not the final default.
Do not copy2 into the native W-buffer-rejecting setter as the final state.

The guest cache is10EE18+state×4. It lies in D3D's zero-filled virtual tail:
section VA0FD680, raw size0EA8C, virtual size11F04. File-backed bytes end at10C10C.
Consequently the cache's initial image values are zero, not these SDK defaults.
Reading the file past a section's raw extent incorrectly returns unrelated code
from subsequent sections; the CSV extractor explicitly zero-fills that extent.

## Demonstrated differences relevant to current consumers

| Xbox state/cache | Original default | Current native initialization | Material effect |
| --- | --- | --- | --- |
|129 TEXTUREFACTOR /10F01C|FFFFFFFF, white|Guest0; backend factor0|New mixed fixed-pixel lowering reads this directly. Factor-selected color/alpha can become zero before the game writes it. Highest-priority nonzero guest default.|
|57 ZFUNC /10EEFC|0203, LEQUAL|Guest0; backend LEQUAL|Native drawing default agrees, guest queries/state logic do not.|
|58 ALPHAFUNC /10EF00|0207, ALWAYS|Guest0; backend ALWAYS|Mixed/programmed fragment uniforms read guest cache; enabling alpha without resetting the comparison can select an invalid comparison.|
|62/63 SRC/DESTBLEND|1/0, ONE/ZERO|Guest0/0; backend SRCALPHA/INVSRCALPHA|Native blend factors differ. Latent while blending is disabled, visible if enabled before factor setters.|
|64 ZWRITE /10EF18|1|Guest0; backend1|Drawing agrees initially; guest cache disagrees.|
|67 COLORWRITE /10EF24|01010101|Guest0; backend mask15|Native writes all channels; guest zero is not evidence that actual writes are disabled. Already demonstrated in boot28/29 diagnostics.|
|124 ZENABLE /10F008|Depth-surface presence,0/1|Guest0; backend1 unconditionally|SetRenderTarget later derives native depth enable from guest cache. Correct initial depth-present state is important.|
|127 FRONTFACE /10F014;128 CULLMODE /10F018|0900/0901|Guest0/0; backend semantic CCW cull|Guest front-face value0 is invalid. Preserve existing measured coordinate/culling conventions when aligning defaults.|
|65 DITHER /10EF1C|0|Guest0; backend leaves GL default enabled|Actual GL enable differs even though cache agrees. Low-impact color precision difference.|

Fog enable82/10EF60 and specular enable93/10EF8C both default0, agreeing with
zero initialization and with boot33's capture. There is no demonstrated missing
nonzero enable for either. Fog end85/10EF6C and density86/10EF70 default1.0 rather
than0, but native fog remains explicitly unsupported when enabled.

Lighting92/10EF88, local viewer94/10EF90 and color vertex95/10EF94 default1.
Back/front specular material sources96/100 default2, diffuse sources97/101
default1; ambient/emissive sources default0. All are currently zero initially.
These are real guest-state mismatches, but the reduced native fixed shader does
not implement lighting/material-source semantics. Copying defaults alone does
not add lighting. The original game explicitly writes lighting0 in39840 and1 in
39870, so do not assume its live lighting state is still the initial default.

Other already-consumed defaults: fill120/backfill121 are GL_FILL1B02;
stencil fail126 and zfail68/pass69 are GL_KEEP1E00; stencil comparison70 is
ALWAYS0207; stencil masks72/73 are FFFFFFFF; blend equation74 is FUNC_ADD8006;
shade66 is SMOOTH1D01. Current native GL/static helper defaults broadly agree,
but their guest mirrors remain zero until written. Two-sided lighting122 and
stencil enable125 are0 and already agree.

## Minimal recommendation

1. Initialize the guest render cache from the title's actual DWORD table, skipping
   states116/135 and overriding124 from the actual native depth-surface presence.
   Retain exact4361 indices; newer Xbox enum tables have inserted states.
2. Align the currently supported native state defaults at the same initialization
   boundary, especially ONE/ZERO blending, depth presence and disabled dithering.
   Reuse validated enum/mask conversion. Keep guest and host caches consistent.
3. Do not run the original full initializer blindly: it calls hardware-writing
   setters, including unimplemented states. A native initializer should not
   introduce a GPU pushbuffer wait or manufacture hardware acknowledgments.
4. Add a small initialization assertion for guest defaults and mapped native
   state; then use the ordinary bounded next boot. Existing game setters may
   override many defaults, so these findings alone do not explain a later frame.

Evidence is the supplied title's original SDK bytes and current local sources:
`local/reports/disasm/asm/D3D.asm`104330/FD860,
`src/graphics.c`CreateDevice and state setters,
`src/shader_bridge.inc`pixel options/uniforms/fixed-stage factor,
`third_party/xboxrecomp/src/d3d/d3d8_gl.c`CreateDevice/apply_render_states.
No new external semantic assumptions are needed for the table/value comparison.
