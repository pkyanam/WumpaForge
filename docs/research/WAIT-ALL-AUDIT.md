# Atomic multi-object wait audit

The POSIX host compatibility layer previously implemented `WaitAll` by calling
the consuming single-object wait on each handle. If one auto-reset event was
ready and a second was not, the aggregate call timed out after consuming the
first signal. Semaphores lost tokens; mutexes could acquire ownership despite
the failed aggregate wait.

The documented contract requires leaving object states unchanged until the
entire set is ready. See Microsoft's
[WaitForMultipleObjectsEx reference](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitformultipleobjectsex).

The fix checks and acquires the set under all participating object locks, sorted
by address. A bounded64-pointer stack array avoids heap allocation and opposite
lock-order deadlocks. Failed polls modify no object. Duplicate entries and
invalid counts fail before acquisition. The existing1ms polling interval remains;
this change does not add a worker or affect the single-object wait path.

## Reproduction and validation

`tools/tests/wait_all.c` linked against the production `win32_compat.c` produced
four failed checks before the fix: event preservation, semaphore preservation,
and mutex ownership/error state. Afterward, UBSan passes cover zero and finite
timeouts, manual/auto-reset events, semaphore consumption, exact mutex recursion,
invalid counts/duplicates, and2000 paired mutex waits with opposite input orders.
The existing current-thread handle/lifetime regression also passes.

```sh
clang -std=c11 -O1 -g -fsanitize=undefined -Ithird_party/xboxrecomp/src tools/tests/wait_all.c third_party/xboxrecomp/src/platform/win32_compat.c -o build/input/test_wait_all
build/input/test_wait_all
```

Local evidence: `local/reports/wait-all-before.log`, `wait-all-after.log`, and
`thread-handles-wait-audit.log`. This fixes a proven compatibility defect;
the supported XBE's113-import inventory does **not** import kernel235
(`NtWaitForMultipleObjectsEx`) or158 (`KeWaitForMultipleObjects`). No current
gameplay stall is attributed to it. This is groundwork for remaining compatibility
paths, not evidence that a scene transition is fixed.

Unchanged limits include APC wake behavior during a single-object wait, mutex
abandonment, the polling scheduler, and handle lifetime validation. As in the host
API contract, callers must keep handles open throughout a wait. No live game was
launched or controlled for this audit.
