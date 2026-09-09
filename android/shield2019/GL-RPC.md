# Opt-in persistent GL owner prototype

The app-external `gl-rpc` marker sets `WRATH_GL_RPC=1` at startup. It is disabled
by default. Run the native graphics component Activity first. Root owns Android
builds, installation and device tests; host fixtures do not establish Tegra driver
correctness or a performance improvement.

The original SDK caller retains guest registers/TLS, native graphics mutex,
resource lifetime and guest-lock ownership. Only raw GL entry points move to a
persistent pthread. The typed loader generates synchronous borrowed-argument
requests for all used GL signatures. A scalar-state batch and its next immediate
pointer/getter operation execute in one submission, preserving order. Caller stack
buffers, shader string arrays and getter output pointers remain alive until the
request completes. Returned driver pointers retain their original lifetime rules.
No GL command is silently skipped and no guest callback runs on the render thread.

A single payload slot and producer mutex bound queue storage and serialize complete
requests. The executor uses a separate handoff mutex and never obtains the native
graphics mutex. Calls already on the owner execute directly to avoid self-deadlock.
Start/stop and SDL lifecycle must remain serialized by the existing graphics
ownership; concurrent unprotected startup/destruction is outside this interface.

Initialization creates SDL/window/context and loads entry points on the SDL game
thread, then detaches and binds the existing context on the render owner. Normal
SDK returns leave it current there. Presentation and swap-interval queries run on
the owner. SDL window sizing and events remain on the original SDL thread.

The isolated SDL patch redirects its Android context backup/restore callbacks to
synchronous detach/rebind requests. SDL holds `Android_ActivityMutex` there, so
these callbacks invoke only the existing-context MakeCurrent path, which does not
reacquire that mutex. They never swap, create a context or pump events. The normal
caller already holds the graphics mutex and has drained earlier GL requests before
pumping. Failed restoration sends SDL's existing render-reset event; the host ends
the game process because resource recreation is unsupported. The graphics fixture
stops/joins the render owner and rebinds its caller before `SDL_Quit`.

Validation so far: preparation replay succeeds; the actual pthread queue passes
host ASan/UBSan tests for two producers, borrowed stack input/output, nested owner
calls, startup failure, detach/restore and shutdown. The generated loader compiles
all116 Khronos signatures in the fixture, including shader double pointers and
returned pointers. Existing scalar queue ordering/overflow tests also pass.
Device graphics, pause/resume, context-reset behavior and matched performance
measurements are still required. The main-thread CPU clock no longer includes
moved driver work: compare wall time and render-thread CPU/RPC wait measurements,
not the old CPU counter alone. Pointer/getter-heavy paths may pay substantial
synchronous wakeup overhead; this prototype makes no60FPS guarantee.

Host checks:

```sh
python3 android/shield2019/tests/test_gl_rpc_ordering.py
python3 android/shield2019/tests/test_state_queue_duplicates.py
```

## Optional submission profiling

Add `gl-rpc-profile` alongside `gl-rpc` to set `WRATH_GL_RPC_PROFILE=1`.
Only this profiling mode adds per-submission wall and thread-CPU clocks. Every60
swaps it reports aggregate totals and the12 operation names with highest total
synchronous latency, using a bounded128-entry table. `untracked` exposes table
exhaustion. Windows combine all submitting threads and may span loading phases.

`total_ms_per_frame` measures submission-to-completion time after producer
serialization. `caller_cpu_ms` is CPU consumed by the waiting caller within that
interval; `render_wall_ms`/`render_cpu_ms` cover execution of the raw operation on
the owner. `handoff_ms` is total wall minus renderer wall: scheduling, wakeup and
handoff overhead, not a direct measurement of time asleep. Profiling itself adds
clock calls and accounting cost. Producer-mutex contention before submission and
report formatting are excluded. Do not compare these clock-instrumented timings
as if they were zero-overhead benchmark results.

A named GL operation includes preceding deferred scalar commands executed in that
same submission. `state-flush` identifies a standalone scalar batch; `swap` and
`lifecycle-bind` identify presentation/lifecycle requests. Counts represent RPC
submissions, not all raw GL calls. `glGetIntegervBatch` is an ordered synchronous
array of up to32 queries: it preserves output pointers, pending state order and
GL error state while reducing adjacent getter roundtrips.

`python3 android/shield2019/tests/test_gl_rpc_profile.py` checks enabled/disabled
reporting against the actual pthread and generated-wrapper fixture, named counts,
caller/renderer timing fields, and bounded output. Independent getter fixtures
check four-element outputs/canaries, query order, retained error state, and zero
count behavior. Device profiling is still needed to identify dominant submissions.
