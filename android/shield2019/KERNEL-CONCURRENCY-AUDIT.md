# Kernel concurrency audit — September 9, 2026

This bounded source review examines dispatch identity, guest register ownership,
and callback arguments. It does not establish whole-game thread safety. Source
references below refer to the preserved `third_party/xboxrecomp/src/kernel/`
checkout unless another path is specified; Android patch replay shifts line numbers.
No speculative runtime changes accompany this document.

## Proven dispatch race and deployment correction

`kernel_bridge.c:5262` originally stored the current kernel thunk slot in one
process-global integer. `recomp_lookup_kernel` assigned it and returned the same
function pointer for every slot. Another guest thread could overwrite the slot
before `kernel_thunk_dispatch` read it. This changes both the selected service and
its stdcall argument cleanup, not merely a diagnostic counter.

The actual-source fixture `tests/test_kernel_dispatch_tls.py` deterministically
interleaves lookups for ordinals 277 and 99. The original implementation invokes
99 for both, moving the first simulated stack to `0x1010` instead of `0x1008`.
Android TLS passes the thread/stack checks and a nested-callback cleanup check.
`runtime-kernel-dispatch-tls.patch` implements the correction (commit `0364047`).

A subsequent device run did **not** contain that correction: Git-format patch
headers were silently skipped when `git apply` ran inside a nested repository
build directory. The binary selector was still an ELF `OBJECT` at `0xAA7140`,
with ordinary ADRP/STR/LDR instructions. Commit `00763f2` fixes these two new
patch headers and moves their fixtures into the repository's ignored build tree
so they exercise the real replay environment. Check the installed matching
library's symbol type is `TLS` before attributing device behavior to the fix.
Root coordinates the current device validation; this audit does not claim a
successful level run or resolution of every previous heap failure.

Actual generated `RECOMP_ICALL`, `RECOMP_ICALL_SAFE`, and `RECOMP_ITAIL` macros
resolve then immediately invoke. Native `sub_0003A7F0` disassembly confirms this;
there is no function-pointer cache at its critical-section call sites. The
common thunk must not be retained across another same-thread kernel lookup,
but no such use was found. The dispatcher snapshots its slot locally before
calling a bridge, preserving the outer cleanup across nested callbacks.

## Remaining findings and reachability limits

| Path | Concrete source issue | Evidence for this title | What is not established |
| --- | --- | --- | --- |
| Handle token allocation | `kernel_bridge.c:2426` scans and writes `s_handle_table` without synchronization. Two creators can choose the same empty slot and overwrite one native handle. Resolve/read/take helpers at 2449–2498 also access the shared table without synchronization. | Device startup trace contains `NtOpenFile` ordinal202 and `NtCreateFile` ordinal190 returning guest tokens, then `NtClose` ordinal187. File creation reaches `bridge_write_handle` at2537; spawned thread handles also reach it at565. | No observed collision or proof that this title's handle creators overlap in the failing scene. A complete fix must address token publication and ownership, not only add a lock around one scan. |
| Deferred procedure queue | `bridge_KeInsertQueueDpc` at1887 writes a shared slot and volatile tail without a lock or atomic publication; `kernel_drain_dpcs` at2006 copies slots without synchronization. Concurrent producers can overwrite one slot; a consumer lacks acquire ordering for its callback and argument tuple. | Startup invokes `KeInitializeDpc` ordinal107 at guest return site `0xF2725`, and `KeSetTimer` ordinal149 at `0xF26FA`; timer thread calls the drain function. | DPC initialization/timer use does not prove `KeInsertQueueDpc` is exercised, or that multiple producers overlap. No observed wrong queued callback is attributed to this issue. |
| Timer first initialization | `kernel_set_timer` around2281 checks/sets plain `g_timer_started`, initializes the critical section and starts the timer thread without one-time synchronization. Two initial callers could initialize/start twice. | The startup trace records the first `KeSetTimer` call before the main game starts. | A later concurrent *first* call is not demonstrated; once startup initialization completes this particular race is no longer reachable. |
| File APC cleanup (ABI issue, not a shared-selector race) | `deliver_one_apc` at2748 permits a kernel thunk fallback at2757, then unconditionally adds12 to guest ESP after the callback. A three-argument kernel thunk already pops its return address and12 stdcall argument bytes in the dispatcher, so this route double-cleans the arguments. | File completion callers pass APC fields to this helper at2979/3004/3255. | No evidence that this title supplies a nonzero kernel APC callback. The generated callback calling convention must also be established before changing cleanup globally. |

The ignored diagnostic source is `local/reports/shield2019/tls-live.log`.
It includes repeated startup sessions; examples above establish route use,
not counts of independent failures. Game captures and generated game code remain
excluded from Git.

## Paths checked without another dispatch-selector defect

- Guest register externs in title graphics, CRT, input, audio, timing and kernel
  bridge/trace code consistently use TLS. Per-worker stack/TIB ownership also
  uses TLS in `kernel_bridge.c:375–382`.
- `src/timing_bridge.c:84–104` snapshots the vblank callback under `timing_lock`,
  then resolves and invokes it with its own thread stack. Shutdown joins the
  timing thread before releasing its storage.
- `src/input_bridge.c:286–301` resolves the kernel event signal, invokes it
  immediately and restores its saved guest stack pointer.
- `src/audio_bridge.c:307–313` completion writes atomic guest status/count and
  frees a per-request host context; it does not execute guest register code.
- Generated dispatch lookup returns distinct compiled functions. The flat table
  is initialized in `src/main.c:71` before `xbe_entry_point` starts game execution.
- Kernel dispatch counters and some trace state remain unsynchronized shared
  diagnostics. They can make counts unreliable, but no wrong-service or stack
  cleanup mechanism analogous to the selector was established through them.

## Separate profiler attribution correction

`src/graphics_timing_probe.inc:26` grouped all workers into one profile while
subtracting `CLOCK_THREAD_CPUTIME_ID` timestamps. A replacement worker starts
with a different CPU clock, producing unsigned underflow and misleading CPU
costs. `title-profile-thread-clock.patch` gives each host thread its own profile;
`tests/test_profile_thread_clock.py` reproduces the old error and validates
independent intervals. This is measurement correctness, not a gameplay speedup.
