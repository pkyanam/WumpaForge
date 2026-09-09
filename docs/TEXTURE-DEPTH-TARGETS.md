# Texture render targets sharing the original depth surface

Build63's five failed hub target switches were captured by a coordinated,
read-only attach/detach without terminating the user's game. Ignored evidence:
`local/reports/play-24-depth-resources.json`.

All five color handles `118EA80/AA0/AC0/AE0/B00` are surface wrappers for the
same 256×256 format6 texture `118D010`, Data `B91000`, pitch1024. The requested
depth `B600A0` is the original 640×480 format2A D24S8 surface, pitch2560. These
are ordinary 2D texture views, not cube faces. Their rejection is confirmed;
its relationship to the user's intermittent Cortex distortion is not established.

## Original and native contracts

The supplied XBE's `FEF20` explicitly distinguishes the backbuffer and offscreen
color geometry at `FF0C9`, then independently retains the passed depth surface at
`FF1E3..FF1FF`. `FF215/FF21B` read the two Data fields separately. `FF27x..FF2Ax`
emit NV2A color/depth offsets and pitches. `FF2EE..FF328` derive depth/stencil
enables from the actual depth binding. This is existing depth storage shared
between color targets, not a request for a cleared temporary depth surface.
The original instructions are retained only in ignored `disasm/asm/D3D.asm`.

The backend's existing internal D24S8 renderbuffer remains canonical. Equal-size
targets attach it directly. A smaller color target needs a matching D24S8 view:
guest row0 is the top row, while GL row0 is at the bottom. Binding copies the
canonical top-left region into that view; switching away copies both modified
depth and stencil planes back to precisely that region. The remaining canonical
pixels are untouched. No depth is initialized to an invented value.

Copies use unscaled `glBlitFramebuffer` with `GL_NEAREST` and matching D24S8
formats, preserving read/draw bindings and scissor enable. Khronos specifies
the depth/stencil format and filtering constraints in
[glBlitFramebuffer](https://wikis.khronos.org/opengl/GlBlitFramebuffer).
The canonical depth-only FBO remains available when a color target detaches
depth. NULL detaches both planes; rebind reloads the original contents. Color
wrapper release deletes only its private view, never the canonical renderbuffer.
The automatic depth surface retains presentation ownership as well as its
current-target binding, matching the existing backbuffer ownership convention.

The supported boundary remains this original automatic, single-sample D24S8
surface, with color dimensions no larger than depth. Arbitrary user-created
depth formats, multisample combinations and cube render targets remain explicit.
Failed calls preserve the previous native and guest target. Rejection logging
is bounded and includes caller, handles, dimensions, formats and current target.

## Validation

`tools/test_texture_depth.inc` uses synthetic resources and actual GPU pixels.
It covers top-left mapping, real depth and stencil occlusion, preserved pixels
outside the smaller target, same-color NULL/rebind, multiple wrappers sharing
color bytes, equal-size direct sharing, invalid bounds/format, GL-error rollback,
and release lifetimes. The strict native component compile is recorded in
`local/reports/texture-depth-build.log`. Root ran the visible GPU suite;
`local/reports/texture-depth-smoke.log` passes every existing fixture and the new
shared-depth fixture. The first run correctly rejected the test's invalid
non-power-of-two swizzled full-size texture; the fixture now uses linear BGRA8
for that case, with swizzled format6 retained for the smaller target. Production
behavior was not relaxed to satisfy it. Full build/live validation remains
root-owned and pending. No game assets or captures are tracked.
