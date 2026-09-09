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
