#include "platform/win32_compat.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>

static unsigned failures;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); ++failures; } } while (0)

/* A failed aggregate wait must leave every initially ready object available. */
static void no_partial_consumption(void)
{
    HANDLE gate = CreateEventA(NULL, TRUE, FALSE, NULL);
    HANDLE event = CreateEventA(NULL, FALSE, TRUE, NULL);
    HANDLE sem = CreateSemaphoreA(NULL, 1, 1, NULL);
    HANDLE mutex = CreateMutexA(NULL, FALSE, NULL);
    HANDLE pair[] = {event, gate};
    CHECK(WaitForMultipleObjects(2, pair, TRUE, 0) == WAIT_TIMEOUT);
    CHECK(WaitForSingleObject(event, 0) == WAIT_OBJECT_0);
    pair[0] = sem;
    CHECK(WaitForMultipleObjects(2, pair, TRUE, 3) == WAIT_TIMEOUT);
    CHECK(WaitForSingleObject(sem, 0) == WAIT_OBJECT_0);
    pair[0] = mutex;
    CHECK(WaitForMultipleObjects(2, pair, TRUE, 0) == WAIT_TIMEOUT);
    CHECK(!ReleaseMutex(mutex)); /* The failed wait cannot acquire ownership. */
    CHECK(GetLastError() == ERROR_NOT_OWNER);
    CHECK(WaitForSingleObject(mutex, 0) == WAIT_OBJECT_0);
    CHECK(ReleaseMutex(mutex));
    /* Ready objects are consumed once when the full set becomes ready. */
    SetEvent(event); SetEvent(gate); ReleaseSemaphore(sem, 1, NULL);
    HANDLE all[] = {gate, sem, mutex, event};
    CHECK(WaitForMultipleObjects(4, all, TRUE, 0) == WAIT_OBJECT_0);
    CHECK(WaitForSingleObject(event, 0) == WAIT_TIMEOUT);
    CHECK(WaitForSingleObject(sem, 0) == WAIT_TIMEOUT);
    CHECK(WaitForSingleObject(gate, 0) == WAIT_OBJECT_0);
    CHECK(ReleaseMutex(mutex));
    CHECK(!ReleaseMutex(mutex));
    CloseHandle(event); CloseHandle(sem); CloseHandle(mutex); CloseHandle(gate);
}

typedef struct { HANDLE pair[2]; unsigned successes; } Contender;
static void *contend(void *context)
{
    Contender *c = context;
    for (unsigned i = 0; i < 1000; ++i) {
        if (WaitForMultipleObjects(2, c->pair, TRUE, 1000) != WAIT_OBJECT_0) break;
        ++c->successes;
        assert(ReleaseMutex(c->pair[0]));
        assert(ReleaseMutex(c->pair[1]));
    }
    return NULL;
}

static void reversed_order(void)
{
    HANDLE a = CreateMutexA(NULL, FALSE, NULL), b = CreateMutexA(NULL, FALSE, NULL);
    Contender c[2] = {{{a,b},0},{{b,a},0}};
    pthread_t threads[2];
    assert(!pthread_create(&threads[0], NULL, contend, &c[0]));
    assert(!pthread_create(&threads[1], NULL, contend, &c[1]));
    assert(!pthread_join(threads[0], NULL));
    assert(!pthread_join(threads[1], NULL));
    CHECK(c[0].successes == 1000 && c[1].successes == 1000);
    CloseHandle(a); CloseHandle(b);
}

int main(int argc, char **argv)
{
    (void)argv;
    no_partial_consumption();
    if (argc == 1) {
        HANDLE event = CreateEventA(NULL, FALSE, TRUE, NULL);
        HANDLE duplicate[] = {event, event};
        CHECK(WaitForMultipleObjects(2, duplicate, TRUE, 0) == WAIT_FAILED);
        CHECK(GetLastError() == ERROR_INVALID_PARAMETER);
        CHECK(WaitForSingleObject(event, 0) == WAIT_OBJECT_0);
        CHECK(WaitForMultipleObjects(0, duplicate, TRUE, 0) == WAIT_FAILED);
        CloseHandle(event);
        reversed_order();
    }
    if (failures) fprintf(stderr, "%u failed checks\n", failures);
    else puts("PASS: WaitAll atomic consumption, timeout preservation, manual/auto events, semaphore, mutex and reversed lock order");
    return failures ? 1 : 0;
}
