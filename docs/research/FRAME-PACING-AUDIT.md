# Native frame pacing audit

The user reports audible output with slow video and audio/video drift. This audit
is source based; it does not claim a measured bottleneck or launch another game.

## Proven pacing behavior

`third_party/xboxrecomp/src/d3d/d3d8_gl.c` `dev_Swap` calls `dev_Present` once.
On macOS the latter uses SDL_GL_SwapWindow on the main thread, CGLFlushDrawable
on the loading worker, then `pace_present`, then event pumping. Initialization
explicitly sets SDL_GL_SetSwapInterval(0), then CGL interval1. This is intended to
avoid SDL's separate display-link wait while preserving the native driver swap
interval. Actual blocking time must still be measured.

`pace_present` uses a shared absolute monotonic deadline, not an extra fixed
16.7ms sleep after every swap. Time already spent rendering and in the driver
counts toward that deadline. If at least one period late, it resets the next
deadline to now+period; otherwise it advances the existing deadline by one period.
The first present starts the schedule without sleeping. There is no source
proof of an unconditional doubled wait. Near-deadline phase interactions between
CGL interval1 and software pacing remain a measurable possibility. Do not remove
the limiter before checking driver-only behavior: the earlier native worker
fixture observed six swaps in0.057s without the software cap.

`src/timing_bridge.c` schedules vblank callbacks on an independent pthread at the
requested refresh, default60Hz. It invokes original compiled callback39910; it
never sleeps the render thread. Late callbacks coalesce elapsed blanks while
invoking the original callback once, so comparing host blank count and guest
1BABB0 is useful if that thread is starved. It does not advance scene animation.

Original2C2C0 normally sets instance+74 animation increment0.5;7A9F3..7A9FF adds
that increment once per permitted update. Original2D950 also passes a fixed1/60
step at2DB85. Native audio consumes samples according to its output clock. Thus,
if the current intro makes one update per rendered frame,30 rendered updates/sec
advance animation at half its intended60-update rate while audio continues.
This explains why accurate vblank delivery alone cannot fix the reported drift;
it is a mechanism, not a measurement of the present FPS. Do not alter the
original increment or audio clock to conceal lost rendering performance.

## Concrete unnecessary-work candidates

1. `src/graphics.c` SetTexture `sub_000FFC90` unconditionally sets `r->dirty=1`
   on every bind, even when the same handle is already bound. Non-P8 images then
   immediately enter full mip/face CPU decoding and glTexImage2D in
   `upload_texture_images`; P8 images invalidate the revision and expand before
   drawing. Ordinary draw-time uploads otherwise return quickly when clean.
   This is a strong source-proven repeated-work candidate. Preserve guest-write
   correctness: lock flags, surface copies, render-target readback, and direct
   writes through retained guest pointers must be audited before simply removing
   this invalidation. A bounded content comparison on bind could establish
   unchanged data, but repeated full-byte hashing also has a cost. Measure bytes
   decoded/uploaded and bind frequency first.
2. `src/index_bridge.inc` `draw_indexed_vertices` validates indices, mallocs a
   count*stride buffer, expands each indexed vertex, then calls the ordinary draw
   path. Programmable drawing uploads that expanded buffer with glBufferData
   and glDrawArrays. This discards GPU vertex reuse and increases copies/upload
   traffic. It is faithful but inefficient. A later native glDrawElements path
   should preserve literal guest index pointers, baseVertex and existing bounds
   tests; do not bypass validation merely for speed.
3. Every programmable draw in `src/shader_bridge.inc` resets16 attributes,
   queries uniform locations, uploads all192 vertex constants, looks up sampler
   bindings and reassigns sampler parameters. Uniform locations and unchanged
   declaration/sampler state can be cached independently of guest CPU execution.
   Costs are unmeasured. Shader program generation/linking is already cached.
4. Each outer SDK call acquires the recursive context mutex, locks CGL, binds the
   context; finish clears it and releases the locks. Present pacing holds that
   device lock. This is required by the current shared main/worker model, but
   lock wait and context churn are candidates if both threads still draw.
5. Normal capture_frame only reads pixels at explicitly requested capture frames;
   it is not an unconditional framebuffer readback. Run performance measurement
   without captures or LLDB per-frame breakpoints. Existing intro_probe explicitly
   warns that debugger stops invalidate wall-clock performance observations.

The current production compile flags are-O2 (with debug symbols), not-O0.
Source inspection alone cannot rank CPU AOT execution against these renderer costs.

## Minimal next-run diagnostic proposal

Use an opt-in native `WRATH_FRAME_TIMING=1` accumulator; no debugger breakpoints,
per-draw printf, GPU query, glFinish, or framebuffer capture. Proposed insertion
points, awaiting parent integration:

- In dev_Present take monotonic timestamps A before driver swap, B after it,
  C after pace_present, D after event pumping. At A/D also read the current
  thread's CPU clock. Retain previous D/CPU stamp separately for main and worker.
- Record frame interval D-current minus D-previous; inter-present work wall time
  A-current minus D-previous; inter-present thread CPU delta; driver swap B-A;
  software pacing C-B; event pumping D-C. Label the CPU field **thread CPU**, not
  GPU time or pure rendering time: original game logic is included. Label wall
  work as wall time: it may include kernel waits and context contention.
- Maintain fixed accumulators and a small fixed sample ring, report one aggregate
  per60 completed frames (or per second) with average/p95/max and elapsed seconds.
  Include thread id/main flag so worker loading does not masquerade as intro FPS.
- Cheap counters at existing paths: draw calls, indexed-expanded bytes,
  glBufferData bytes, dirty texture upload count/decoded pixels/upload bytes,
  SetTexture binds and same-handle rebinds. Increment counters only, no clock
  reads or printing in each vertex/texel loop. Native GL serialization protects
  the accumulators. Count cache-hit uploads separately from actual uploads.
- At the aggregate boundary read guest swap count, scene index8F4DFC,
  instance1BA928[index]+70/+74, audio gate1BAA2C, guest vblank1BABB0, and the audio
  subsystem's already available consumed-frame/queued-frame counters. Validate
  the instance bounds. These correlate slow frames with actual animation rate.

Interpretation: long work time with short driver/pace waits means rendering or
AOT work is the bottleneck; high dirty-upload bytes prioritize texture lifetime
tracking; large expansion bytes prioritize native indexed submission. A near33ms
frame interval with little CPU work and distinct driver plus pacing waits points
to clock/phase interaction. Long wall work but low thread CPU suggests waiting,
contention, or GPU backpressure rather than expensive CPU translation. A single
ordinary bounded parent run should distinguish these without changing timing.

Only this report was written. No production source, build, or launch changed.

## Implemented diagnostic for the parent's next run

Set `WRATH_PROFILE=1`. `src/graphics_timing_probe.inc` now records the opt-in
accumulators through minimal hooks in graphics.c. It prints `[wrath profile]`
every60 successful Swap calls, with separate main/worker windows. The backend
prints `[wrath present profile]` every60 native presents, splitting driver swap,
software pacing, and event-pump duration. The original pacing and rendering
behavior are unchanged. The cumulative graphics patch preserves its existing
backend/CMake changes and passes reverse-apply checking.

Actual fields use average/max intervals (no percentile sample array), and count
input draw bytes rather than adding a separate indexed-expansion hook. This
keeps the initial instrumentation small. Upload timing covers successful dirty
texture decode/upload and GL-state restoration; initial P8 palette-index
validation precedes that interval. RGBA bytes count all uploaded mips/faces.
`work_cpu_ms` is current-thread CPU between presents, including original game
logic, not GPU execution. First-window counters include initialization/loading
before the first present, so use later windows when judging steady intro cost.
`max_ms` is maximum frame interval; backend `max_present_ms` is the different,
maximum duration spent inside its present function.

Validation: direct compilation of production graphics/backend plus the ordinary
GPU smoke with `WRATH_PROFILE=1` passed. A separate tiny deterministic fixture,
`tools/tests/frame_profile.c`, exercised the60-frame aggregate/reset boundary
without GL or guest code. It reported exactly60.00fps,16.667ms interval,
10.000ms work wall,6.667ms present,1.000ms upload per frame and60 same-handle
binds, matching its independent synthetic clock schedule. Logs:
`local/reports/graphics-profile-smoke.log`, `local/reports/frame-profile-unit.log`.
No full game build or launch was run by this subtask.
