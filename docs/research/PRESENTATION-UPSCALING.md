# Optional presentation scaling and sharpening

Prepared separately from the playable-game critical path, 2026-09-08.
`src/presentation_filter.h` supplies a single GLSL330 fragment source,
`wrath_presentation_filter_glsl`. It is original dependency-free code, not an
AMD CAS/FSR implementation. No new library, model, SDK or asset is downloaded.
Default sharpness0 exactly follows the ordinary bilinear presentation sample.
Optional strength0.25 adds a five-sample cross unsharp filter, clamped to local
RGB extrema to limit overshoot. Alpha remains the center bilinear sample.

The source scene remains640x480. Larger output does not create new game detail,
correct source aliasing, or turn the game into a native-resolution render.
Sharpening can emphasize texture grain and pixel edges; it should be optional.
Its RGB operation is explicitly in the RGBA8 texture's code-value space, with no
implicit change to the game's color/gamma path.

## Integration contract

The window/presentation agent owns the fixed game render target, resize/fullscreen,
4:3 viewport, clear and GL state restoration. The shader uses:

- `in vec2 v_uv`: normalized source UV, with the backend's normal image orientation.
- `uniform sampler2D u_source`: completed game RGBA8 scene texture.
- `uniform vec2 u_source_size`: actual source dimensions, normally640x480.
- `uniform float u_sharpness`: clamped0..1; default0, suggested optional0.25.
- `out vec4 color`: filtered RGB and unchanged bilinear alpha.

Source sampling must use LINEAR and CLAMP_TO_EDGE. Use a sampler object or restore
texture state if the source texture can subsequently be used by game rendering.
Disable presentation blend/depth and preserve the normal output color encoding.
Clear the destination to black and draw only inside the centered aspect viewport.
The module adds no frame history, CPU readback, synchronization, or additional
render pass; it replaces the fragment source of an already required presentation
pass. It adds shader work, and therefore cannot guarantee zero added latency.

| Output canvas | Actual4:3 game viewport | Horizontal bars each side |
| --- | --- | --- |
| HD1280x720 |960x720 |160px |
| FHD1920x1080 |1440x1080 |240px |
| QHD2560x1440 |1920x1440 |320px |

These are physical output pixels. The window layer must use the drawable/backing
pixel size rather than assuming macOS logical points equal physical pixels.
This module is not yet connected to the game. Keep default bilinear and defer
optional sharpening controls until the requested winter stage is playable.

## GPU validation and bounded cost measurement

`tools/test_presentation_filter.c` opens a hidden standalone accelerated CGL
context, with no SDL/game window or audio. It renders synthetic640x480 RGBA8
patterns to the three full output sizes, compares actual GPU samples to an
independent CPU bilinear/filter reference, checks edge clamping and sampled alpha,
and confirms black pillarboxing. Both strength0 and0.25 pass on AppleM3,
OpenGL4.1 Metal90.5. Source edges, gradients and abrupt patterned transitions are
covered; a visual preference assessment on actual game frames remains pending.

```
clang -std=c11 -O2 -Wall -Wextra -Werror tools/test_presentation_filter.c -framework OpenGL -o build/test_presentation_filter
./build/test_presentation_filter
```

The fixture also records GL_TIME_ELAPSED for three batches of eight draws after
warmup. Last local results (`local/reports/presentation-filter-test.log`), GPU
milliseconds per presentation pass:

| Output | Bilinear median | Sharpness0.25 median | Sharpness observed range |
| --- | --- | --- | --- |
|720p |0.017859 |0.021386 |0.020963–0.035521 |
|1080p |0.035583 |0.035432 |0.035359–0.768083 |
|1440p |0.031167 |0.044469 |0.043646–0.050203 |

The nonmonotonic medians and1080p outlier show clock/driver noise in this tiny
synthetic workload. They are not a precise incremental-overhead estimate or an
end-to-end latency/FPS measurement, and exclude game rendering, window compositing,
scanout and input. They support only the bounded conclusion that this pass runs
on the native GPU with small steady synthetic cost. Gameplay measurements must
follow integration; do not add timer-query waits or readbacks to the game path.

## Primary-source alternative audit

[AMD's CAS overview](https://gpuopen.com/fidelityfx-cas/) describes its spatial
sharpening and optional scaling and MIT licensing. The
[original CAS header](https://github.com/GPUOpen-Effects/FidelityFX-CAS/blob/master/ffx-cas/ffx_cas.h)
documents its compute-oriented integration and linear-RGB input requirements,
including the risk of excessive sharpening on perceptual inputs. Reusing it
faithfully in this existing OpenGL presentation path would require a fragment
adaptation and deliberate color-space handling, plus its license notices.
The current bounded module uses a simpler independently written filter and
makes no CAS/FSR quality claim. No AMD implementation code was copied. An advanced
spatial reconstruction algorithm can be evaluated later if actual frame
comparisons justify additional complexity; it is not required for the three
output canvas sizes above.
