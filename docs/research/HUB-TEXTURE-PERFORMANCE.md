# Live hub texture and profiling audit

Story16/build56 reached the actual controllable hub. No game was launched,
interrupted, or attached to by this audit. An attempted read-only3-second sample
found PID81343 already exited at the subsequent vertex-stream compatibility stop;
the command report is `local/reports/story16-hub-sample-command.log`.

The28 main-thread hub windows at frames4842–6462 in `story-16.log` have median
30.00FPS,29.954ms work wall time,24.653ms work CPU time,3.044ms Present, and5.304ms
texture upload time per frame. Median uploads are540 and175.624MiB **per60-frame
aggregate**, approximately270 uploads/second at30FPS. The transition/interaction
windows span18.47–48.22FPS and should not be mistaken for steady comparisons.

The final resource snapshot describes22,895,998 guest bytes across3107 resources
(398 textures,118 palettes, one cube). Thus exhaustion of the64MiB exact texture
shadow budget cannot explain this run's upload churn. The current render-target
and CopyRects implementation reads GPU pixels to CPU, encodes guest memory, and
later decodes/uploads changed bytes when sampled. This is a plausible repeated
cost, but the earlier aggregates do not identify the actual uploading handles or
separate readback/encode/resolve time. No speculative coherence shortcut is made.

## Bounded next-run attribution

With `WRATH_PROFILE=1`, each60-frame window now records up to32 uploading resource
handles with format, dimensions, count, RGBA bytes, and elapsed upload time. The
five highest-time handles are printed. Overflow upload counts are explicit; no
allocation, per-upload logging, or unbounded map is introduced. Upload aggregation
includes all resources even when detailed attribution overflows.

Surface summaries record GPU-readback, CopyRects, and render-target resolve
counts, bytes, and milliseconds/frame. Copy source bytes describe the full source
read performed by the current implementation, even for a subrectangle. Timings
are inclusive: CopyRects and resolve may include readback, so their times must
not be added to readback as if disjoint. Existing behavior and guest-memory
coherence remain unchanged.

## Profiling overhead isolation

The prior `WRATH_PROFILE` path took nine monotonic timestamps per outer native
context acquire/release pair. The SDK can call these functions hundreds of
thousands of times per60 frames. This instrumentation is now separately enabled
only by `WRATH_PROFILE_CONTEXT=1` together with `WRATH_PROFILE=1`; ordinary frame,
upload, surface, draw-driver, event-pump and Present measurements remain enabled
without it. The context summary explicitly prints `context_timing=0/1` so zero
lock totals are distinguishable from measured zero waits. Locks, recursion,
thread ownership, CGL binding, event pumping and swap pacing are unchanged.

The optional component-only `WRATH_BENCH_CONTEXT=1` measurement executes three
rounds of200,000 actual native context acquire/release pairs after releasing the
smoke fixture's outer lock. Median cost on this M3 was39.950ns/pair with context
timing off and188.285ns with it on, a148.335ns difference. At the observed0.94M
pairs/60 frames, this predicts approximately2.32ms/frame of profiling overhead.
At2.4M pairs it would predict5.93ms/frame. This microbenchmark is not evidence
that disabling instrumentation alone restores60FPS; a matched live hub window
is still required. No game timestep is changed to conceal missed frames.

## Validation and next comparison

Full GPU smoke passes with context timing both off and on; reports are
`hub-profile-build.log`, `hub-profile-context-off.log`, and
`hub-profile-context-on.log` under ignored `local/reports`. The deterministic
`tools/tests/frame_profile.c` test verifies inclusive category totals, the60-frame
reset, resource reuse, and33rd-resource attribution overflow without memory growth;
report `hub-profile-aggregate-test.log`. All builds used one compiler job.
The cumulative graphics patch passes reverse-application checking and retains
the required Apple OpenGL framework CMake linkage.

Run the same hub with `WRATH_PROFILE=1` and context timing absent/0. Compare steady
FPS/work with the earlier windows, then use the upload-handle and surface summaries
to choose a specific optimization. Preserve exact guest-byte mutation detection,
palette variants, render-target ownership, copy orientation, and resource lifetime.

## Story17/build57: attribution confirmed

The27 heavy hub windows (`upload_MiB>160`, animation0) have median32.60FPS,
28.156ms work wall time,22.855ms work CPU time,2.456ms Present, and5.536ms uploads.
Context timing is explicitly0. Removing detailed lock timestamps modestly improves
performance; it does not restore60FPS.

Per-resource records identify linear BGRA8 (`format12hex`) texture01261060,
640×480, uploaded120 times/60 frames. It accounts for140.625MiB/window and
approximately3.9ms/frame by itself. Several128×128 RGBA8 targets account for the
remaining roughly1.7ms/frame. `upload_untracked=0` in these windows.

The25 surface windows with180 CopyRects and420 resolves per60 frames have median
6.664ms CopyRects and3.811ms resolves per frame. There are600 GPU readbacks,
237.188MiB/window, taking5.853ms/frame inclusive of row orientation conversion.
Readback is nested in copy/resolve; adding it again would double-count work.
Upload+copy+resolve is approximately16ms/frame of identified serial work. This
confirms actual changed render textures and the GPU→CPU→guest→GPU conversion
path, rather than snapshot-budget exhaustion or invalidation-only upload churn.

A bounded next step is a byte-identical linear32-bit upload/row-copy path:
format0x12 guest BGRA8 already matches native upload/readback format, but the
current loops dispatch encode/decode per pixel. Preserve pitch, subrectangles,
row orientation, guest CPU visibility, exact snapshots, mips and direct guest
writes. GPU-authoritative resource coherence would require a larger independent
audit; these measurements do not authorize dropping CPU-visible copies.

This monitoring task made no production edits. The read-only sampling attempt
found PID30506 already exited at the subsequent vertex-stream stop (report
`story17-hub-sample-command.log`); it did not interrupt the live session. Parent
owned game launch, input, stop captures and the next compatibility boundary.

## Implemented bounded format optimization

The direct format0x12 upload and row-copy path is now implemented and passes the
full GPU suite. It preserves exact shadows, guest visibility, row pitch, alpha,
copy bounds and orientation. See [implementation and GPU coverage](../LINEAR-BGRA-FASTPATH.md).
This removes redundant CPU conversions; actual next-run FPS and the existing
per-resource/copy summaries will determine the performance gain.
