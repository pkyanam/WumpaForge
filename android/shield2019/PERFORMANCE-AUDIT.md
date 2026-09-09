# September 9 presentation and CPU timing audit

This source-only pass used the existing Shield reports; it did not launch the game,
change pacing, or measure a new build. The root task owns device validation.

## Evidence

In `local/reports/shield2019/cache-game.log`, the original opening scene's frame455
window reports20.61FPS,48.531ms interval,47.458ms work wall time,45.000ms work
thread CPU time, and1.073ms presentation. Nearby presentation windows report
approximately1.08ms driver time and0.000ms pacing. Frame635 reports41.39FPS,
21.486ms work CPU time, and1.042ms presentation. These CPU clocks include native
runtime and driver execution; they do not identify guest AOT code as the bottleneck.

The existing `perf-cached-report.txt` samples693 user task-clock events over8seconds.
It includes NVIDIA compiler/optimizer functions, `nvLoseCurrent`, memory routines,
and AOT `sub_00101BC0`. This is useful evidence for context/shader work, but not a
matched before/after experiment or a GPU execution-time measurement.

The backend requests swap interval1 and then uses one monotonic refresh deadline.
The deadline accounts for elapsed rendering/swap time, rather than adding a fixed
sleep after every frame. The observed slow windows spend essentially no time in
that pacing loop. Changing game animation steps or removing pacing is not supported
by this evidence as a path to60FPS.

## Output sizing

`d3d8_present.inc:init_output_buffers` allocates both color targets and depth at
`g.backbuf_w`/`g.backbuf_h`, taken from the guest presentation parameters. The final
presentation copies to the front-image target at that same internal size, then
scales to an aspect-preserving rectangle in the actual drawable. SDL Android
replaces requested window dimensions with `Android_SurfaceWidth/Height`; asking
SDL for640x480 therefore does not establish a640x480 Android output surface.
The source does not accidentally promote the internal scene FBO to4K. The actual
output surface size needed explicit logging before deciding whether a lower output
resolution would save measurable work. Existing logs identify bilinear filtering.

## Diagnostic correction

`runtime-profile-output.patch` fixes the runtime's presentation/context profile
bucket selection to distinguish the Android game thread from its loading worker.
Previously only Apple selected the worker bucket: Android mixed the60-present
window across threads while the context counters were thread-local. Earlier
runtime `thread=main` labels therefore cannot identify worker presentation costs.
The title's separate frame/CPU profiler already uses the Android-aware
`window_thread()` and is unaffected.

The patch also logs internal dimensions, actual drawable dimensions, reported SDL
swap interval, and target refresh when the drawable changes. It does not change
rendering or pacing. Exact patch application and output were checked against an
isolated copy of the upstream runtime files. Compilation and physical output
validation remain root-task gates; no speedup is claimed for this diagnostic fix.

## Indexed resource scan sample attribution

An8-second `task-clock:u` capture of PID6947 recorded761 samples with0 loss.
79 samples (10.38%) land in native DrawIndexedVertices;74 are at
`index_bridge.inc:67`,3 in its inlined `resource_at`, and2 on adjacent loop lines.
This identifies the full resource-table bounds scan, not vertex expansion, as
that sampled cost. Native and NVIDIA GL code each account for34.82% of samples.
Raw ignored evidence: `render-current.data`, `render-current-dump.txt`, and
`render-current-report.txt` under `local/reports/shield2019/`.

The candidate-list and duplicate-texture-check patches passed the full physical
Shield graphics suite together. Repeat source preparation also passes; it now
removes the disposable title copy before replaying patches that create files.
Current rendering measurements follow after deployment; synthetic validation is
not a measured60FPS guarantee. Mac source and package remain unchanged.

## Combined device validation

PID8086 (source71bf969 plus preparation fix) reports640x480 internal,
1920x1080 drawable and swapinterval1. Opening windows:51.22/52.18FPS at
animation326/356;28.54 at416 and21.64 at446. At446, workCPU42.709ms,
present1.038ms. Earlier cache build at446 measured20.83FPS; uncontrolled device
load and slightly shifted window boundaries limit precise comparison.
The target is still unmet. Large remaining gains require reducing render-driver
submission/context work. Do not hold the graphics lock across arbitrary original
guest execution: potential guest lock/wait ordering must be proven or explicitly
handled. The9.98-second maximum early frame also includes loading and1571texture
uploads over its60-frame window; steady-state FPS does not describe that pause.

## Final uniform-cache validation

Source9166913 fullphysicalGPU suite returned0, including changedvertexconstant
pixels. PID8695 measured51.73/52.72FPS at animation326.5/356.5;28.32/21.32 at
416.5/446.5;16.20 at686.5. The small change is not a controlled speedup claim.
Initial load max9952.665ms persists. Logs: uniform-values-game.log and
uniform-values-graphics.log. At77%weeklyusage, stop new work to preserve the20%
remainingfloor buffer. No60FPS, audio-sync or hub-crash completion is claimed.

Next rendering work: measure a fixed heavy scene, then design a way to reduce
EGL ownership transfers without holding graphics locks across guest waits. A
render-thread command submission design must copy pointer payloads at submission,
preserve getter/readback/deletion ordering and guest resource lifetimes, handle
SDL lifecycle/context loss and propagate failures. It is a substantial change,
not an approved shortcut to remove locking. Separately trace the original heap
free-list corruption beforeEndStateBlock allocation; retain fullstate semantics.

## Loading I/O follow-up

The path-case adapter resolves components when translating a path for open or
metadata operations. It does not resolve the path on every read. Exact Android
component matches use `fstatat`; missing exact spelling scans only the current
parent's siblings, then opens the next directory. There is no full-tree scan.
Repeated opens can repeat those scans, but the available reports do not establish
that as the loading bottleneck. No immutable-asset cache was added speculatively.

A concrete avoidable operation exists in `bridge_NtReadFile`: every successful
read emitted a byte-prefix diagnostic with `fprintf` and `fflush`. Android sends
unbuffered stderr to `native.log` on app external storage. The existing
`hub-transition.log` contains22423 read records for326394318 delivered bytes,
including13900 requests of8192bytes and7152 requests of4096bytes. There are288
short reads and120 nonzero statuses. These counts span the retained trace, not a
measured single level-load interval.

`runtime-read-log-budget.patch` applies the existing kernel trace budget to
successful full-read dumps on Android. Short reads and nonzero statuses retain
their diagnostic records. Read calls, offsets, returned bytes/status, guest
IO_STATUS_BLOCK writes, event/APC completion, and asset contents are unchanged.
The existing POSIX implementation issues one `read` and an optional `lseek` per
request. The change removes routine external-storage log writes after the trace
budget, but its device latency benefit is not measured and it does not establish
that endless loading is fixed. The previously captured heap crash is separate.

`python3 android/shield2019/tests/test_read_log_budget.py` passes a host UBSan
fixture that extracts the actual patched bridge and POSIX read function. It reads
real temporary-file bytes, checks guest byte counts/status and completion calls,
and verifies budgeted successful logging plus retained short/EOF/invalid-handle
records. No Android build or device run was performed by this audit task.

## Compressed uploads and Morton addressing

Native sampling-only DXT1/3/5 uploads use the advertised S3TC capability, preserving
all faces/mips and CPU decode fallback. Physical fixtures cover alpha, selectors,
cubes, mip chains, mutation, forced fallback and preexisting GL error propagation.
Opening volume only drops133.253→131.753MiB; most opening content is uncompressed.
Morton column lookup ff42244 preserves every byte offset, with bounded scratch and
allocation-failure fallback. Host UBSan checks and full Shield GPU suite pass.
First loading-window upload CPU39.297→21.109ms/frame, longest pause10.41→8.87s,
same131.753MiB. Single-run comparisons; not a controlled latency guarantee.

The later black attract scene has active main-thread draws after loader exit.
There are67 successful traced outer unlocks. Repeated128x128 format06 uploads
correspond to1962 resolves totaling122.625MiB. Snapshot allocation is only1.7MiB,
so current evidence contradicts snapshot-cap exhaustion. Correlate per-resource
resolve bytes, guest mutations and upload snapshots before attempting to avoid
readbacks. Preserve guest coherence and original heap semantics. User-selected
portal loading and original heap corruption remain unresolved.
