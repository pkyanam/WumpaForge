# Bounded actual-draw GPU probe

`src/graphics_probe.inc` is an opt-in diagnostic for one selected game frame.
`WRATH_TRACE_FRAME=N` selects draws for which the guest swap counter plus one
equals N. Invalid/missing settings disable the probe. It inspects at most the
first 16 native draw submissions of that frame. It does not change vertices,
shaders, textures, drawing state, game timing counters, or rendered pixels.

Include the file before `draw_vertices_data`, after resource/shader declarations.
Bracket the actual submission in **both** paths:

```c
unsigned probe = probe_begin();
HRESULT result = shader_draw(primitive, count, vertices, stride);
probe_end(probe, result);
```

Use the same bracket immediately around the final native
`s_device->lpVtbl->DrawPrimitiveUP(...)` call. Placing begin after argument checks,
uploads, conversion and fixed transforms prevents earlier error returns from
leaving a query open. Every nonzero token requires a matching end; do not bracket
an entire function with early returns unless all exits are handled.

The probe runs while the existing native graphics lock/context is held. It logs:

- `GL_SAMPLES_PASSED` for the actual draw, or unavailable when another occlusion
  query is active. It never ends or replaces an existing query.
- Actual draw FBO, selected color buffer, program, viewport, sample count, color
  mask, depth enable/write/comparison/range, blend factors/equations and culling.
- Scissor enable/rectangle, stencil enable and rasterizer-discard enable.
- Full target RGB nonblack count and center RGBA, then depth min/max/center and
  finite count when a depth attachment is available.

`WRATH_TRACE_DIR` optionally names an **existing absolute directory**. At most
eight actual target BMPs are written, named `frame-N-draw-DD-fbo-F.bmp`. Positive
BMP height preserves GL's bottom-up row order, producing an upright displayed
image. BMPs record actual RGB; alpha remains available in the center-pixel log.
Readback covers the current target dimensions, not just the viewport.

Each target is bounded to 4096×4096. RGBA8 and float depth reuse one buffer of at
most 64 MiB, freed after each draw; BMP output uses one extra row. Unsupported
non-default multisample readback is skipped without introducing a resolve draw.
The query-result wait and readback synchronize with the GPU and can slow the
selected frame. This is a diagnosis tool, not a performance measurement.

Pack-buffer binding and all pack parameters, read FBO, and both affected FBOs'
read-buffer selections are restored. No texture/sampler/active-unit state is
changed. The draw framebuffer and drawing state are only queried. The probe does
not consume GL's error queue. Dimensions and supported normalized color/depth
attachments follow this bridge's target contract; arbitrary integer attachment
formats are outside that contract.

Interpret sample count together with pixels: zero samples can mean clipping,
culling, alpha discard or depth/stencil rejection. Positive samples with black
RGB can mean black shader output, blending or color-write masks. This probe alone
does not identify which upstream state is wrong, and a later draw/clear may
overwrite an earlier correct result. Pair the output with the parent task's
actual guest draw/state ledger.

Validation: an isolated C11 include harness passes Clang `-Wall -Wextra -Werror
-fsyntax-only`, using installed epoxy headers. The ignored harness is
`local/reports/graphics-probe-syntax.c`. Integration and runtime GPU/state
restoration validation remain with the parent task. No game launch or full build
was performed while adding this file.

Parent validation: boot29 ran the integrated probe on actual frame120. Draw1
passed307200 samples and produced8445 nonblack RGB pixels (visible starfield);
draw2 passed307200 samples and replaced the frame with black at depth~0.592803.
Subsequent sampled scene draws passed zero samples, retaining that depth; the
earlier CPU projection put their sprite vertices around0.976. Color masks were
enabled, culling/scissor off. This establishes actual overdraw/occlusion, not that
the game's foreground quad is incorrect. The intro is still held waiting for
audio startup. BMPs/log: local/reports/gpu-boot29 and boot-29-gpu.log.
No diagnostic state bypass was used. A production graphics object compile passed.
