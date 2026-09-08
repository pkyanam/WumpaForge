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
cache. This change does not claim to implement every SDK render-state default.

The standalone graphics fixture initializes the32-byte semantic default table
because it runs without an XBE. It checks color/alpha defaults and coordinate
indices for all four stages. The integrated actual GPU fixture also checks the
reached programmed-vertex/fixed-pixel two-texture equation and retained alpha.
Full graphics smoke passes, including prior indexed/immediate/framebuffer tests:
local/reports/graphics-defaults-mixed-smoke.log. Build command is recorded in
docs/D3D-INTEGRATION.md; link the native OpenGL framework as well.
