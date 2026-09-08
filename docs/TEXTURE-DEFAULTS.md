# Original texture-stage defaults

Boot33 captured valid stage1 RGB ADD but ALPHAOP0. This was a native device
initialization omission: the game changes selected states and relies on SDK
defaults for the rest. Original alpha-combiner dispatch does not support zero.

Original initializer104330, called by104A50, supplies the precise contract:
10439D–1043C4 loops over four stages and32 states, loading each default byte from
10BD9C and calling FE170. Each stage then receives TEXCOORDINDEX=stage. Finally
1043C6–1043D7 select stage0 COLOROP4 (MODULATE) and ALPHAOP2 (SELECTARG1).
The other stages retain default COLOROP1/ALPHAOP1 (DISABLE). Evidence is the
supplied image disassembly in local/reports/disasm/asm/D3D.asm.

Native CreateDevice now reproduces these cache values using the default table
in the loaded image. All guest state entries remain32-bit; no host pointers or
binary assets are embedded. Existing native sampler/shader bridges consume the
cache. The render-state initialization added below mirrors the remaining verified SDK
cache defaults without claiming support for every rendering feature.

The standalone graphics fixture initializes the32-byte semantic default table
because it runs without an XBE. It checks color/alpha defaults and coordinate
indices for all four stages. The integrated actual GPU fixture also checks the
reached programmed-vertex/fixed-pixel two-texture equation and retained alpha.
Full graphics smoke passes, including prior indexed/immediate/framebuffer tests:
local/reports/graphics-defaults-mixed-smoke.log. Build command is recorded in
docs/D3D-INTEGRATION.md; link the native OpenGL framework as well.


## Original render defaults

The verified 4361 initializer at `0x104330` also sets render states 57 through145
from the DWORD table at `0x10BDC0`. Native CreateDevice now copies those values
into the guest cache at `0x10EE18 + state*4`, skipping reserved states116/135
exactly as the original does. It overrides state124 from the successfully created
depth surface presence, producing0 or1; the table's temporary2 is never passed to
native W-buffer handling. See `docs/research/RENDER-DEFAULTS-4361.md` for original
instruction evidence and the extracted89-entry inventory.

The currently supported native differences are synchronized at this initialization
boundary: depth comparison, alpha comparison, source/destination blend factors,
depth enablement, texture factor, and dithering. This supplies the original
ONE/ZERO blend pair, white texture factor, ALWAYS alpha comparison, LEQUAL depth
comparison, and disabled dithering. Existing native cull winding and other
already matching native defaults are preserved. Guest lighting/material defaults
are mirrored for subsequent original calls and queries; this does not implement
fixed lighting, fog, or other unsupported GPU features. The bridge does not run
hardware-writing guest initializer calls.

The existing standalone native GPU smoke now checks untouched nonzero canaries
at both reserved slots despite DEADBEEF source sentinels; white factor and valid
alpha comparison in both guest and native caches; initial color-write/front-face
values; actual GL ONE/ZERO factors and disabled dithering; and both depth-present
and depth-absent initialization. The full smoke also passed its real GPU pixel,
shader-pair, blending, alpha-test, indexed/immediate, and framebuffer regressions.
One compiler invocation linked the same source and existing native backend as
production, with the OpenGL framework. Results are in
`local/reports/graphics-render-defaults-smoke.log`. No full game rebuild or launch
was performed for this change; the parent owns subsequent game validation.
