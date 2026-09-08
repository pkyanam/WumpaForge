# Xbox graphics audit — boot26, 2026-09-08

Read-only research during the requested architecture pause. No game launch,
renderer modification, or new synthetic test was performed. Scope: the current
native graphics compatibility layer, not a recommendation to emulate Xbox CPU
instructions. SDK calls can implement these contracts using native GPU APIs while
the original game CPU code remains ahead-of-time compiled ARM64.

## What the existing evidence establishes

`local/reports/game-frame-120-boot26.bmp` contains exactly307,200 black RGB pixels
(640×480,32bpp; read directly with Python stdlib). This proves a black presented
frame, not why it is black. `boot-26.log:2461` samples the main thread in
`upload_texture_images` with a palette pointer, called by an indexed four-vertex
triangle strip:101BC0→3B010→A5D40→A8240→9F370→A0990→A8140→2D950.
Five programmable shader pairs linked earlier. Boot26 is executing rendering;
it is not stalled at the previously missing Begin boundary. One stack sample
inside a texture upload does not establish a persistent upload hang.

## Highest-impact findings

| Priority | Finding and confidence | Consequence / discriminating test |
| --- | --- | --- |
| 1 | **Confirmed missing fixed pixel-state semantics.** Backend `d3d8_gl.c` fixed fragment shader always computes vertex diffuse × texture0. `bind_texture0` treats a bound resource as an enabled stage. It never consumes COLOROP/ALPHAOP/arguments, RESULTARG, texture factor, or later stages. | A valid textured draw can become black or transparent when Xbox expects texture-only color, constant color/alpha, another stage, or a disabled stage. Capture actual four-stage state at the first post-loading fixed draw before choosing which operations to implement. |
| 1 | **Confirmed missing fixed sampling semantics.** `bind_texture0` forces LINEAR and REPEAT. It ignores actual address/filter/mip state, TEXCOORDINDEX, texture transforms and projective coordinate sizes; `setup_fvf_attribs` always supplies two coordinates from the first FVF texture set. | Coordinates may sample the wrong location, including transparent/black borders. Test the exact live state with a small asymmetric texture and actual UVs. Programmed sampling already handles more of these states, so compare paths rather than assuming a shared decoder fault. |
| 1 | **High-confidence winding risk, cause unproved.** Xbox SDK FDAD0 selects FRONT/BACK from the requested winding and10F014; FDB40 updates front-face state and reapplies culling. Current bridge instead removes a fixed GL winding and does not distinguish coordinate conventions between fixed XYZ and screen-coordinate paths. | A sign error can cull every visible triangle. Establish a truth table using the original SDK packet equations and captured post-transform vertices. Test CW/CCW for fixed XYZ, XYZRHW and programmed draws, with an off-center viewport. Do not apply an unmeasured global flip. |
| 2 | **Confirmed incomplete fixed lighting/material path.** Backend stores material/light values, skips normal/specular input fields, and has no lighting or fog calculation in its fixed shader. | Correct geometry with material-derived color can be dark or incorrectly lit. First record LIGHTING/material color-source state; actual first immediate FVF142 includes diffuse but no normal, so missing lighting alone is not established as this frame's cause. |
| 2 | **Confirmed programmed texture-state omissions.** `shader_draw` applies U/V wrapping, filters and border color, but not W addressing, mip LOD bias/MAXMIPLEVEL, texture ALPHAKILL or color key. The latter are independent of the final alpha test. | Wrong sampling and extra/missing fragments. Fail explicitly or implement when the captured title state enables them. Current source does not prove that any is enabled in frame120. |
| 3 | **Confirmed avoidable upload churn, correctness change needs care.** SetTexture marks a resource dirty on every bind. P8 revision caching therefore invalidates even without new pixel writes; the sampled boot26 stack is doing this upload. | Likely CPU/driver cost; not evidence that the upload causes black. Count uploads versus actual lock/write/palette revisions. Retain guest-memory coherence when optimizing, including persistent lock pointers and render-target resolves. |

Texture operations have distinct color and alpha equations; substituting modulation
for all operations is invalid. Microsoft's documentation supplies the equations,
but **PC enumeration values cannot be copied wholesale**: Xbox texture operations
and state indices differ. Cxbx's reconstructed Xbox definitions corroborate
COLOROP12, ALPHAOP16, TEXCOORDINDEX28 and the divergent operation values. The actual
4361 executable remains the authority for cache locations and ABI.
[Microsoft texture operations](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dtextureop),
[Xbox reconstructed definitions](https://github.com/Cxbx-Reloaded/Cxbx-Reloaded/blob/585c49a50af1255ab155099e06f24505f9c5a800/src/core/hle/D3D8/XbD3D8Types.h).

For this title,9F480 sets FVF142, starts quads and enables alpha testing:
9F536 sets state60=1;9F53F obtains reference from1A3B04;9F54D sets comparison
state58=0x204 (GREATER). A wrong fixed alpha equation can therefore discard
an otherwise valid quad. The reference and actual input alpha still need capture.

## Contracts checked and remaining assumptions

**Vertex transforms.** NV2A programmed output is screen space; reconstructing clip
coordinates and multiplying by output W is supported by xemu. Its implementation
also truncates screen XY to1/16 and clamps W away from zero/infinity. Our generator
does these operations. Adding another perspective divide would be incorrect.
Our viewport inversion followed by native viewport mapping is algebraically valid
for the supported finite range. However, the hardcoded24-bit depth scale only
covers that depth representation; D16/floating/W-buffer modes and degeneracies
are not established. Capture real oPos and constants before changing transform
math. [xemu programmed vertex conversion](https://github.com/xemu-project/xemu/blob/fdfb5a8f481b2f870c57080e74ec8d3a31a47053/hw/xbox/nv2a/pgraph/glsl/vsh-prog.c).

The fixed W×V×P row-vector convention and GL upload transpose handling were
already tested locally, as was D3D z[0,w]→GL z[-w,w]. Those tests do not prove
that the live guest matrices are correct. The original FEA20 matrix cache is
consumed at draw time; compare a real emitted point using those matrices.
Xbox fixed transforms also include lighting, texture generation/matrices and
viewport effects beyond this reduced backend.
[xemu fixed-function generator](https://github.com/xemu-project/xemu/blob/fdfb5a8f481b2f870c57080e74ec8d3a31a47053/hw/xbox/nv2a/pgraph/glsl/vsh-ff.c).

**Immediate attributes.** Original101E60 maps register−1 to method1518 and generic
register0 to1A00. Both emit when the fourth position component arrives; other
attributes persist. This matches the bridge. Generic2f position emits on its second
component; xemu assigns z0,w1 but marks those defaults with a hardware uncertainty
comment. Keep that distinction documented. Our active game UV2f use does not depend
on their unused z component. Initial current-attribute defaults and non-unit-W
fixed positions remain limited, rather than fully hardware-verified.
[xemu method handlers](https://github.com/xemu-project/xemu/blob/fdfb5a8f481b2f870c57080e74ec8d3a31a47053/hw/xbox/nv2a/pgraph/pgraph.c).

**Winding.** xemu explicitly adjusts front-face winding for its clip-space Y
convention. This is evidence that winding and coordinate conversion must be audited
together; copying its GL state alone into our differently oriented render targets
would be insufficient.
[xemu draw state](https://github.com/xemu-project/xemu/blob/fdfb5a8f481b2f870c57080e74ec8d3a31a47053/hw/xbox/nv2a/pgraph/gl/draw.c).

**P8 and resources.** Palette sizes256/128/64/32 and byte-index→32-bit palette
lookup agree with xemu. Expanding palette colors before native interpolation,
with separate stage variants, is appropriate. Existing native tests cover colors,
alpha, mips, distinct palettes and lifetimes; they do not prove the live palette
was populated correctly. Guest32-bit headers and pointer-free handles remain
necessary; native objects cannot be written into Xbox headers.
[xemu palette/texture conversion](https://github.com/xemu-project/xemu/blob/fdfb5a8f481b2f870c57080e74ec8d3a31a47053/hw/xbox/nv2a/pgraph/texture.c).

Current render targets use a separate real FBO texture, resolve actual pixels back
to guest storage on target switches, and upload the sampled resource afterward.
That is a coherent supported-path strategy, not a known stale-texture bug.
Rendering and sampling the same underlying guest allocation without a switch,
arbitrary registered external resources, depth texture variants and tiled aliasing
remain unverified. No evidence warrants replacing the existing resource model wholesale.

**Register combiners.** Native general/final combiner equations, independent RGB
and alpha expressions, per-stage constants and simultaneous writes are grounded
in the published NV contract. Original108140..1081CA evidence supports the special
zero-final-word SDK default already documented in `docs/PIXEL-SHADERS.md`.
Advanced modes reject explicitly. However, compiler success and synthetic equation
tests cannot prove the actual eight-stage shader's texture/constant inputs are
correct. Differentially evaluate its real inputs stage by stage before attributing
black to the combiner algebra.
[NVIDIA combiner specification](https://registry.khronos.org/OpenGL/extensions/NV/NV_register_combiners.txt),
[per-stage constants](https://registry.khronos.org/OpenGL/extensions/NV/NV_register_combiners2.txt).

Texture alpha-kill is a separate discard after texture fetch in xemu; our pixel
generator currently has only the final alpha comparison.
[xemu pixel generation](https://github.com/xemu-project/xemu/blob/fdfb5a8f481b2f870c57080e74ec8d3a31a47053/hw/xbox/nv2a/pgraph/glsl/psh.c).

## Next diagnostic, before more compatibility changes

1. Record one bounded actual frame's draw ledger: target, primitive/count, program
   handles/FVF, complete stage caches at10EC18+stage×128, render states, viewport,
   matrices/constants, and texture/palette content hashes. Include Clear/Copy/Swap
   order and return errors, so a valid scene later cleared is distinguishable.
2. For the first expected visible fixed and programmed draws, capture real
   post-transform vertices, triangle signed areas, UV/color/alpha values, current
   GL cull/depth/blend/alpha state, target attachment and texture completeness.
   A GPU sample query and actual target readback can distinguish clipped/culled
   geometry, discarded fragments, zero shader output and later overwrite.
3. Reproduce the captured API inputs in a focused native regression fixture.
   Implement only demonstrated state mismatches, retaining the original guest
   logic/assets. Diagnostic render-state bypasses must not become production fixes
   or be presented as a working game.

The Apple “unloadable texture” warning exists in the combined synthetic smoke
log, **not in boot26's log**. It is not currently evidence for this black frame.

## Source provenance

Primary implementation references fetched during this audit are cached, ignored,
under `local/reports/graphics-research/`. xemu revision
`fdfb5a8f481b2f870c57080e74ec8d3a31a47053`; nxdk revision
`29638d0b001f179b73c3513489af10ddc2986216` (NV register definitions cross-check:
[nxdk nv_regs.h](https://github.com/XboxDev/nxdk/blob/29638d0b001f179b73c3513489af10ddc2986216/lib/pbkit/nv_regs.h)).
Cxbx header revision is pinned above; existing older SDK reconstruction references
and their differences are recorded in `docs/PIXEL-SHADERS.md`. Microsoft/Khronos
pages were consulted on2026-09-08. Reverse-engineered implementations are strong
corroboration, not a claim of complete original hardware conformance.
