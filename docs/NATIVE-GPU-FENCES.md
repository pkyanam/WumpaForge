# Original GPU fences backed by native completion

Story18 reached the Arctic Antics portal mesh while still in the hub. The game
issued a native DrawVertices call, then its unbridged InsertFence entered the
original Xbox push-buffer submission loop. The stop at103370 waited on a GPU
register flush bit which the native OpenGL renderer does not own. Clearing that
bit would not establish completion of the actual rendered work.

## Original ABI and call evidence

All addresses below are from the supplied XBE disassembly under ignored
`local/reports/disasm/asm/`; no original game bytes are added to Git.

| Address | Original operation and contract |
| --- | --- |
| 3AFEE | Calls FED90 immediately after DrawVertices101B20; stores EAX into the game's buffer registry at1E8D8C+index×32. |
| FED90 | InsertFence(), no arguments, DWORD result; calls103420 with flags0. |
| 103420 | Emits method1D70 semaphore-release with the current device+30 value, increments that value by2, and submits through103330 unless flags bit2 suppresses submission. |
| 1030F2/F9/FF | Original initialization: next fence5, completed3 through device+34, initial auxiliary value3 at device+48. IDs are odd, not even. |
| 1034D0 | BlockOnTime(time, avoid_interrupt), callee pops8. Compares unsigned modular differences between next, requested and completed time; requesting current next first inserts it. |
| 1037A0 | BlockUntilIdle(), no arguments; waits on current next time. |
| 103B20 | Resource_IsBusy(resource), callee pops4. Checks original common/binding bits and optional surface parent, then resource+8 time against device+30/completion; clears completed resource time. |

The game calls IsBusy at3A331/3A381 before locking vertex-buffer storage. Its
original bound-resource checks are preserved rather than replaced by a constant
false. No separately called IsFencePending function address was found in this
supplied title; a native pending helper is tested, without inventing a hook.

## Native implementation

`src/gpu_fence_bridge.inc` backs InsertFence with glFenceSync and submits through
glFlush. The [Khronos ARB_sync specification](https://registry.khronos.org/OpenGL/extensions/ARB/ARB_sync.txt)
defines a signaled GPU_COMMANDS_COMPLETE fence as completion of all prior commands
in the same native command stream, including their framebuffer effects. Waiting
uses glClientWaitSync with real completion checks; a wait timeout continues to
wait and a driver failure reports an error. No timeout or allocation failure is
reported as successful GPU completion.

The existing graphics mutex/context ownership serializes fence insertion with
all native draws, copies and resource uploads across game threads. A fixed64-slot
ring retains native sync objects. Polling retires only signaled entries; when
full, insertion waits for real completion of the oldest entry before reuse.
A4-byte owned guest allocation holds completed time, and device+34 points to it.
It advances only when native GPU completion is observed. Device+30 starts at5
and advances by2 with ordinary32-bit rollover, matching the original initializer.
No host pointers enter guest RAM. The bounded host ring occupies about1KiB.

BlockOnTime supports the reached0/1 wait-mechanism flag with the same completion
contract for both values. BlockUntilIdle inserts/waits at the current command
position. IsBusy polls real fences, preserves common-bit/surface-parent checks,
uses original unsigned modular timestamp comparisons, and clears completed
resource timestamps. Polling these native APIs refreshes the guest completion
cell; no background thread fabricates progress. The toolkit's unrelated legacy
fence-mirror facility is not registered by this title and is not used here.

Hooks cover FED90,1034D0,1037A0 and103B20. Original internal push-buffer allocation,
interrupt-ring handling and arbitrary unbridged GPU packets are not claimed
implemented. In particular,103330 is not replaced by a generic loop bypass.

## Validation

`tools/test_gpu_fences.inc` uses actual GL commands and two asynchronous PBO
readbacks with different colors. Native fence waits establish ordered pixel
visibility. It verifies original5/7 initialization,193 successive insertions
with bounded storage and retirement, wrap through FFFFFFFD/FFFFFFFF/1, idle/current
next-time waiting, completed-cell updates, and texture/surface-parent IsBusy
binding behavior. Guest FIFO pointers and MMIO-related device fields remain
unchanged. The fixture does not assume that an initial nonblocking poll must be
pending: a real GPU may already have completed.

The entire combined native GPU smoke passed first try, including the separately
owned retained-vertex-input fixture and the BGRA8 optimization. One compiler job;
reports `local/reports/gpu-fences-build.log` and `gpu-fences-smoke.log`. Manual
hook config was frozen before the parent's lift, and production source was
frozen before the parent's build notification. No game was launched by this task;
actual portal dwell and level loading remain the next live validation.
