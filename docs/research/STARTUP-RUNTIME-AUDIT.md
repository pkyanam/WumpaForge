# Startup, timing and thread audit

Research checkpoint: 2026-09-08, through boot26. Read-only source/log research;
no implementation edits, new builds or game launches during this audit.

## Assessment and status correction

The proven current state is **the original intro/cutscene render loop**, not
“main render loop” in the sense of title/menu/gameplay. Boot26's sampled main
thread is inside `sub_2D950`, called from `sub_87BF0` at original `0x87E19`.
It is rendering through `A8140 → A0990 → 9F370 → A8240 → A5D40 → 3B010` and the
native indexed draw bridge. The parent reports actual GPU frame120 solid black;
that image alone does not establish either an expected transition or a clock bug.
The previous green loading-object frame likewise does not establish a visible
intro scene. `docs/STATUS.md` belongs to the parent and was not edited here.

The original fade is updated once per intro loop, without a wall-clock query.
It starts at255 with step−8, reaching0 after32 update calls unless another
original routine changes the step/state. Therefore **an indefinitely black
screen cannot be attributed to a stopped fade clock without first showing that
this original update is not running or its state is being reset**. Host swap
number120 is not a count of intro update calls: loading/splash swaps precede it,
and the rendering code has several submission phases. Compare values at matched
intro-loop boundaries before interpreting a frame count.

## Original game control flow and diagnostic fields

The local authority for addresses is `local/reports/disasm/asm/text.asm`, derived
from the supplied read-only XBE. Generated C is a comparison target, not the
source of truth. Names below describe observed use; original debug symbols are
unavailable.

| Priority | Guest address / expression | Proven use and interpretation |
| --- | --- | --- |
| 1 | `0x17BE64` signed32 | Current intro/cutscene selector passed to `2D950`; main skips this sequence when−1 (`87DE9`). `2D130` selects lighting cases0..4 with it. Exact scene names remain unverified. |
| 1 | `0x8F4DB4` signed32 | Next selector, initialized−1 at `2D9A3`; copied into current selector at `2DD13`. Nonnegative repeats outer scene-loading loop; negative returns to main. |
| 1 | `0x1A097C` signed32 | Fade amount. Initialized255 at `2DA7C`; `85D40` adds signed step and clamps0..255. |
| 1 | `0x23C40C` signed32 | Fade step. Initialized−8 at `2DA86`; reset0 at endpoints. `2BB40` can derive another step from remaining animation time. |
| 1 | `0x23C408` signed32 | One-update completion latch set when fade reaches255 from below, then decremented. Intro selector0 loop continues until fade255 and latch0 (`2DC7F..2DC95`). |
| 1 | `0x942064` u32 | Update/render gate, initialized1 at `2DA6F`; zero skips the rendering block and can end a nonzero-selector intro. |
| 1 | `0x8F4DFC` signed32 | Current scene/animation index. Used to index32-entry arrays; validate0..31 before following them. It is not proven to be a framebuffer index. |
| 1 | `0x23C2E0 + index*4` guest pointer | Current additional scene object; nonzero drives `37340(object,1.0)` and `393D0(object)`. Zero suppresses those particular draws, not necessarily all scene rendering. |
| 1 | `0x1BA928 + index*4` guest pointer `instance` | Current animation instance; used by update and render paths. Capture pointer validity before following it. |
| 1 | `instance+0x6C` u32 | Animation control flags. `7A9A0` tests0x100,0x2,0x80,0x1; `7AC00` uses0x200 to prevent a duplicate update in a pass. |
| 1 | `instance+0x70`, `instance+0x74` float32 | Animation position and per-update increment. `2C2C0` sets increment0.5, with specific overrides; `7A9F3..7A9FF` adds increment into position when its flag/pause conditions allow. |
| 2 | `instance+0x58` guest pointer `definition`; `definition+8` float32 | Animation definition and endpoint used by `7A9A0`, and by fade-step callback `2BB40`. Check finite values and pointer extent. |
| 2 | `0x1BAA2C` u32 | Conditional animation/audio gate in `2C330`: particular audio-status/input combinations force `instance+0x74=0`, otherwise `2C2C0` restores the increment. |
| 2 | `0x23B758`, `0x23B75C` signed32 | Pause/transition amount and direction; amount0..30 is passed into `7AC00`, whose worker skips advancing animation for nonzero argument. `6B0E0` increments/decrements this per loop. |
| 2 | `0x1BAA98` float32 | Optional animation increment override compared in `2C2C0`. |
| 2 | `0x23C2D4` u32 | Animation/audio control bitmask passed to `ABFA0`; it is not an elapsed-time counter. |
| 2 | `0x23C3F4` guest pointer | Loaded audio data pointer from object+0x38 (`85CEE`); it is not elapsed time. |
| 2 | `0x23BA84` u32, `0x23C290` pointer | Fade draw color and material. `85D40` constructs color `0xFFrrrrrr` from fade amount; `85DD0` skips the overlay entirely when amount0, otherwise calls `37650`. Do not assume amount is directly an alpha byte. |
| 2 | `0x1BABB0` u32 | Original vblank callback counter; callback `39910` increments it. Pair delta with host monotonic time and delivered callback count. |

Capture two or three snapshots at `2DA90` or immediately after `85D40`, together
with native frame number and host monotonic time. This yields a useful decision:

- Fade reaches0 and animation position advances while images stay black: inspect
  native draw/target/depth/viewport/shader state first; no fade-clock fix is justified.
- Fade remains255 with step−8 across proven update executions: inspect generated
  `85D40` and unexpected writers. The current generated integer/clamp logic matches
  the original on source inspection; no specific lifter defect was found there.
- Animation position stays fixed: inspect increment,0x6C flags, pause amount and
  audio gate before suspecting host clocks. A zero increment can be intentional.
- Scene selection changes/reloads: a black sample can be a transition; capture
  frames around those changes rather than waiting an arbitrary longer duration.

Additional local boundary facts: `2D130` configures lighting by intro selector and
scene index; it is not the progression timer. `2D950` calls `85D40` at `2DB51` and
uses a fixed float `0x3C888889` (1/60) when calling `9EBA0` at `2DB85`. `7A9A0`
advances animation through x87 float operations, so finite position/increment and
correct AOT x87 flags deserve checks if scene progression is wrong. These are
specific diagnostic leads, not proven causes of boot26's black frame.

## Primary architecture references and native-runtime implications

Sources checked online at nxdk revision
`29638d0b001f179b73c3513489af10ddc2986216`. nxdk is an independently maintained
open-source Xbox SDK, not Microsoft's original title SDK. Its public source can
corroborate general contracts; the retail instruction offsets above remain the
authority for this game's ABI. No reference source was copied into implementation.

1. [nxdk thread startup and exit](https://github.com/XboxDev/nxdk/blob/29638d0b001f179b73c3513489af10ddc2986216/lib/winapi/thread.c)
   (MIT): zero requested stack size comes from the image header; each thread has
   its own aligned TLS initialization; startup forwards the routine's returned
   status to ExitThread. GetExitCodeThread references a kernel object, distinguishes
   terminated state from STILL_ACTIVE and dereferences afterward. This supports
   the local image-derived stack/TLS and real native exit-lifecycle approach.
   Caveat: nxdk tests `HasTerminated`, while this game's SDK explicitly tests byte+4.
2. [nxdk kernel declarations](https://github.com/XboxDev/nxdk/blob/29638d0b001f179b73c3513489af10ddc2986216/lib/xboxkrnl/xboxkrnl.h)
   (CC0): dispatcher objects have a signal field and ETHREAD carries exit status;
   kernel interfaces distinguish stdcall and fastcall. Native ARM64 pointer sizes
   cannot replace guest32-bit pointer/output layouts. Existing focused tests cover
   the actual retail exit-status read, DWORD output canaries and reference cleanup.
3. [nxdk pbkit vblank handling](https://github.com/XboxDev/nxdk/blob/29638d0b001f179b73c3513489af10ddc2986216/lib/pbkit/pbkit.c)
   (MIT): the display-interrupt path increments its vblank count, handles ready
   presentation buffers and pulses a vblank event. This corroborates that display
   timing and CPU draw submission are distinct events. It does not establish the
   exact retail D3DVBLANKDATA callback ABI; that came from this XBE and the existing
   local SDK audit. A real display mode's refresh, not the Mac panel's refresh,
   should determine the compatibility callback cadence.
4. [Apple pthread condition waits](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man3/pthread_cond_timedwait.3.html):
   timed waits atomically release/reacquire the mutex, and their documented
   `abstime` is a system-time deadline. `src/timing_bridge.c` instead maintains a
   monotonic absolute phase and uses Apple's relative wait extension. This avoids
   accumulating callback duration into successive deadlines. This source does not
   guarantee exact realtime wakeup delivery; scheduling lateness must be measured.
5. [LLVM atomic/concurrency guide](https://llvm.org/docs/Atomics.html): volatile and
   atomic are separate concepts. Plain volatile accesses do not establish the
   synchronization needed for shared C memory. The generated `MEM32` uses volatile
   guest storage, while the native vblank thread and main thread share counters.
   This is an unresolved architecture-level correctness concern for AOT code on
   ARM64, not evidence that it caused the current black frame. Investigate actual
   shared accesses and guest synchronization before choosing barriers/atomics or
   serialized guest scheduling; do not replace every memory access indiscriminately.

## Proven runtime behavior versus remaining assumptions

Proven locally: compiled ARM64 workers execute original entries; guest stacks and
TLS are separate; explicit worker exit publishes real status; SDK per-query object
views fix the observed completion poll; vblank callbacks execute original39910;
logo hold was passed. Component tests cover these boundaries. Source audit confirms
`85D40` uses per-loop arithmetic rather than QPC or vblank in this intro loop.

Limits that must remain visible:

- Thread object views are per-reference snapshots, not full persistent ETHREADs.
  Current GetExitCodeThread obtains a fresh reference each poll; future code that
  retains a thread object across an exit will need actual live state support.
- Vblank scheduling skips stale callback bursts after host lateness while its data
  counter records elapsed blanks. The game's39910 increments once per delivered
  callback, so it can lag elapsed blanks under overload. Quantify this with paired
  counters; do not force the game counter. This cannot alone explain the per-loop
  fade remaining fixed if that loop executes normally.
- RDTSC is scaled in `kernel_hal.c:xbox_ReadTimeStampCounter` to the console rate;
  kernel QPC/frequency currently forward a coherent native nanosecond pair. Mixed
  assumptions between these domains are not globally proven safe. No direct
  QPC/RDTSC dependency was found in the fade arithmetic audited here.
- Graphics upload work appeared in the boot26 sampled main-thread stack. That
  proves CPU time was spent preparing draws, not that fragments became visible.
  The parent/graphics audit owns actual target, clipping and fragment evidence.

Next action after the research pause should be a single bounded run capturing the
priority1 fields at matched intro update points and a nearby actual GPU image.
That measurement distinguishes an expected initial fade, a paused animation,
repeated scene loading, and a native rendering failure without adding speculative
runtime work.
