# Resizing and fullscreen presentation

The game retains its original internal color/depth resolution (normally640×480).
A separate native OpenGL presentation step scales the completed image into the
actual drawable, with centered black bars preserving the game's aspect ratio.
Window size never changes guest viewport coordinates, resource dimensions,
CopyRects, screenshot resolution, or vertex constants. Mouse controls use relative
motion, so they do not need a mapping from window coordinates to game pixels.

The SDL window is resizable and supports the normal macOS maximize/fullscreen
window controls. F11 or Control–Command–F toggles desktop fullscreen; repeated
keydown events are ignored. F10 toggles optional bounded sharpening at strength0.25;
it starts off. Mac keyboards configured for media keys may require Fn with F10/F11.
`WRATH_SHARPNESS=0.25` can select it at launch (accepted range0..1).
Fullscreen preserves the desktop display mode. Escape
remains the game's Start/pause binding. The main thread alone handles SDL window
changes and caches the drawable's pixel size for worker presentation. Retina pixel
size is distinct from window size in logical points.

The backend owns two RGBA8 color FBOs and a D24S8 internal depth/stencil buffer.
Before presenting, it copies the actual internal back image to the internal front
surface, then scales the back image into the drawable. The fixed internal surface
names stay valid across resize/fullscreen transitions. Existing graphics capture,
back/front resource reads, render-target binding and CopyRects use these FBOs.
The presentation step restores read/draw FBO/buffer selection, color masks, clear
color, scissor, rasterizer discard and framebuffer-sRGB enable. The optional shader
also restores the active program/VAO, viewport, texture unit/binding/sampler,
polygon mode, blend, depth, stencil, cull and logic-op enables. It does not enqueue
frames. Default scaling is a linear framebuffer blit; sharpening uses the isolated
`presentation_filter.h` module in one draw instead of that final blit. No increased
internal rendering resolution or reconstructed detail is claimed. The original
640×480 capture intentionally excludes window scaling/sharpening; compare actual
window screenshots to assess output quality.

Primary contracts checked2026-09-08:
- [SDL_SetWindowFullscreen](https://wiki.libsdl.org/SDL2/SDL_SetWindowFullscreen):
  desktop fullscreen and returning to windowed mode without a display-mode change.
- [SDL_GL_GetDrawableSize](https://wiki.libsdl.org/SDL2/SDL_GL_GetDrawableSize):
  drawable pixels may differ from logical window size on Retina.
- [SDL_WindowFlags](https://wiki.libsdl.org/SDL2/SDL_WindowFlags): resizable and
  high-DPI flags; app packaging already sets NSHighResolutionCapable.

Production integration tests passed: `local/reports/presentation-build.log` and
`presentation-smoke.log`.
`tools/tests/graphics_presentation.c` checks
actual pixels after wide/tall/4:3 resizing, letterbox colors, preserved internal
front/back color and depth, changed GL-state restoration, and the real fullscreen
keyboard event path including repeated-key suppression, plus F10 filter toggling
and720/1080/1440 requested output sizes. Window-manager sizing limits may constrain
the actual drawable; assertions use its measured pixel dimensions. The separate
filter fixture covers exact1280×720/1920×1080/2560×1440 GPU targets.

The combined renderer test passed with actual game-size capture enabled:
`local/reports/presentation-combined-build.log` and `presentation-combined-smoke.log`.
It includes both fixed fog variants, real backbuffer/texture render targets,
CopyRects/readback/pack-state capture, depth, morph/stream/shader and resource tests.
The updated main/worker context test passed48 readbacks and6 real presents in0.095s:
`presentation-threads-build.log` and `presentation-threads-smoke.log`. Cumulative
`patches/xboxrecomp-graphics.patch` includes the new backend include and passes
reverse-apply checking against the dependency checkout. Parent owns production
builds and live game fullscreen/quality/performance verification.
