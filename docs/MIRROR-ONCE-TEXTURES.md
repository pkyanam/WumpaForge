# Native mirror-once texture addressing

Story15 passed the dependent reflection shader and stopped at address mode5.
The actual sampler cache has U=V=5, W=1 on stages0/1, including a linear640×480
screen texture and a normalized2D texture. Pixel modes are[1,1,1,0].

Microsoft's primary [D3DTEXTUREADDRESS definition](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dtextureaddress)
assigns MIRRORONCE=5 and defines absolute-value addressing followed by clamping.
The Xbox enum in the pinned toolkit's `src/d3d/d3d8_xbox.h` agrees. The Khronos
[EXT_texture_mirror_clamp specification](https://registry.khronos.org/OpenGL/extensions/EXT/EXT_texture_mirror_clamp.txt)
defines mirror-clamp-to-edge as abs followed by the ordinary half-texel edge
clamp. This differs from periodically mirrored repeat.

A hidden16×16 SDL OpenGL4.1 core context on this M3 was queried using
SDL_GL_ExtensionSupported. ARB_texture_mirror_clamp_to_edge,
EXT_texture_mirror_clamp, and ATI_texture_mirror_once all returned0. The native
extension enum therefore cannot be used on this machine.

The native pixel generator exposes per-stage U/V mirror-once uniforms for
PROJECT2D. Coordinates are first divided by projective W and, for linear textures,
by the actual texture dimensions. Selected axes then take abs; native
GL_CLAMP_TO_EDGE performs the edge clamp. textureGrad receives derivatives of the
original projected, normalized coordinates before abs or clamp. Thus a quad that
crosses zero or samples beyond the edge retains the original mip footprint.
Other address modes retain their existing textureProj path. Uniform locations
are cached at link time and each sampler receives its own live state, including
when two stages bind the same resource. No texture copies or extra shader-cache
variants are introduced. The same generated pixel path serves fixed texture
stages and programmable combiners.

This bounded implementation supports the reached2D sampling contract. Cubemap
mode5 remains an explicit error: applying abs to cube direction vectors would
change face selection and would be incorrect. Volume sampling remains subject
to its existing explicit unsupported-resource boundary. The generator equations
and test code are independently authored; no external implementation was copied.

## Validation

One compiler job built the full native GL smoke suite, which passed first try:
`local/reports/mirror-once-build.log` and `mirror-once-smoke.log`.

`tools/test_mirror_once.inc` reads actual GPU pixels for normalized and linear
coordinates, independent U/V flags, negative projective W, coordinates beyond
both ends, and transition back to ordinary clamp. Color-coded mip levels verify
LOD2 across a zero-crossing quad and beyond the clamp edge; using derivatives of
abs/clamped coordinates would instead select the red base level. The existing
full guest shader bridge fixture also changes two samplers of the same texture
between clamp and mirror-once and checks distinct live pixel results. Earlier
reflection, fog, vertex, texture-cache, indexed, lifetime and resource-growth
fixtures all remain passing. No game or full AOT build was launched by this task;
actual hub navigation remains the parent's next validation.
