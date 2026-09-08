#include "platform/win32_compat.h"
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

static HANDLE worker_dup;
static void *worker(void *unused)
{
    (void)unused;
    assert(DuplicateHandle(GetCurrentProcess(), (HANDLE)(uintptr_t)0xfffffffeu,
                           GetCurrentProcess(), &worker_dup, 0, FALSE, DUPLICATE_SAME_ACCESS));
    assert(WaitForSingleObject(worker_dup, 0) == WAIT_TIMEOUT);
    return NULL;
}
int main(void)
{
    HANDLE first, second, third;
    assert(DuplicateHandle(GetCurrentProcess(), (HANDLE)(uintptr_t)0xfffffffeu,
                           GetCurrentProcess(), &first, 0, FALSE, DUPLICATE_SAME_ACCESS));
    assert(first != (HANDLE)(intptr_t)-2 && first != (HANDLE)(uintptr_t)0xfffffffeu);
    DWORD code;
    assert(GetExitCodeThread(first, &code) && code == 259u /* STILL_ACTIVE */);
    assert(WaitForSingleObject(first, 0) == WAIT_TIMEOUT);
    assert(SetThreadPriority(first, THREAD_PRIORITY_HIGHEST));
    assert(DuplicateHandle(GetCurrentProcess(), first, GetCurrentProcess(), &second,
                           0, FALSE, DUPLICATE_SAME_ACCESS));
    assert(CloseHandle(first));
    assert(GetThreadPriority(second) == THREAD_PRIORITY_HIGHEST);
    assert(DuplicateHandle(GetCurrentProcess(), second, GetCurrentProcess(), &third,
                           0, FALSE, DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE));
    assert(GetExitCodeThread(third, &code) && code == 259u /* STILL_ACTIVE */);
    assert(CloseHandle(third));
    /* CLOSE_SOURCE must not consume the lifetime reference of a pseudohandle. */
    assert(DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &third,
                           0, FALSE, DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE));
    assert(CloseHandle(third));
    assert(DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &third,
                           0, FALSE, DUPLICATE_SAME_ACCESS));
    assert(GetExitCodeThread(third, &code) && code == 259u);
    assert(CloseHandle(third));
    pthread_t thread;
    assert(!pthread_create(&thread, NULL, worker, NULL));
    assert(!pthread_join(thread, NULL));
    assert(WaitForSingleObject(worker_dup, 0) == WAIT_OBJECT_0);
    assert(GetExitCodeThread(worker_dup, &code) && code == 0);
    assert(CloseHandle(worker_dup));
    puts("PASS: 32-bit current-thread duplication, independent lifetime, priority and thread-exit signaling");
}
