# Story05 renderer timing

The user-observed backstory audio/video separation is supported by actual
frame timing. `local/reports/story-05.log` has opt-in native60-present aggregates
and sparse original story-boundary snapshots. Root owns the launch and input;
no separate game was launched for this audit.

## Station (movie1 scene0)

These complete windows exclude the initial loading-contaminated window:

| End frame | Animation | FPS | Work wall ms | Work CPU ms | Upload ms/frame | Uploads/window |
|---|---:|---:|---:|---:|---:|---:|
|3564|39|20.24|47.773|24.956|42.476|5940|
|3624|69|22.49|43.057|23.407|38.193|5832|
|3684|99|21.06|46.264|24.337|41.145|5820|

Upload work accounts for about89% of rendering work and expands/uploads roughly
370–375 MiB per60-present window. Presentation takes only1.2–1.6 ms/frame with
zero software pacing wait, so this is not doubled frame pacing. Context operations
and event pumping each total well below1 ms/frame; actual draw calls cost about
0.85–0.93 ms/frame. Texture upload time includes decode and GL submission/waits.

From frame3504 to3684 the original animation moves9→99 (90 units,3 nominal seconds
at30 units/sec), while vblank3861→4370 measures509 blanks,8.483 seconds. The video
accumulates about5.48 seconds of delay over that measured interval. No timestep
or game counter is changed to disguise this shortfall.

## Corridor (movie1 scene1)

Frames3804 through4044 run58.94–60.00 FPS. Work is13.1–14.8 ms/frame, including
8.8–10.2 ms uploads. Original animation advances0.5 units per rendered frame.
This section is near its intended speed, but cannot remove station's existing
video/audio offset. The subsequent character scene stops on the independently
assigned vertex-stream compatibility issue.

Native audio agent independently found a stable512–1024 frame output queue,
nonzero audio output, unchanged cumulative underrun count2 and zero overflows.
Its detailed source/output accounting is separate; renderer measurements alone
establish that station video is much slower than its nominal timeline.

## Measured work and exact invalidation cause

Inspection found that SetPalette and PaletteLock changed content revisions even
without any byte changes. Those revisions forced the already byte-validated P8
cache to decode/upload again. The authorized correction removes only these
artificial revision changes. Exact comparison still advances revisions after
real guest writes; resource handle and per-stage palette dependencies remain.
See TEXTURE-SNAPSHOT-CACHE.md and tools/test_texture_snapshot.inc for regression.
The actual game must be measured again after this correction before claiming
its performance or synchronization improvement.

A bounded5-second `/usr/bin/sample` of the root-launched PID58694 produced a
report with no call graph. It supplies no usable sampled-stack evidence and is
not used to attribute decode versus GL-driver costs. No additional process
profiling or speculative driver optimization was performed.
