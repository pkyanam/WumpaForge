/* Native D3D8 vertical-blank callback delivery. See docs/RUNTIME.md.
 * Calls AOT-compiled guest functions; no CPU interpreter or NV2A ISR is used. */
#include "kernel/xbox_memory_layout.h"
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

extern _Thread_local uint32_t g_eax, g_ecx, g_esp, g_ebp, g_seh_ebp;
typedef void (*timing_guest_fn)(void);
extern timing_guest_fn recomp_lookup(uint32_t);
extern timing_guest_fn recomp_lookup_manual(uint32_t);

static pthread_mutex_t timing_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t timing_changed = PTHREAD_COND_INITIALIZER;
static pthread_t timing_thread;
static int timing_started, timing_stopping, timing_in_callback;
static unsigned timing_hz = 60;
static uint32_t timing_callback, timing_stack, timing_tib, timing_data;
static uint32_t timing_blank, timing_swap_blank, timing_swap_flags;

static uint32_t timing_read(uint32_t va)
{
    return *(volatile uint32_t *)((uintptr_t)va + xbox_GetMemoryOffset());
}
static void timing_write(uint32_t va, uint32_t value)
{
    *(volatile uint32_t *)((uintptr_t)va + xbox_GetMemoryOffset()) = value;
}
static uint64_t timing_now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + ts.tv_nsec;
}

/* Mutex held on entry/return. Registration and shutdown can interrupt waits. */
static void timing_wait(uint64_t remaining_ns)
{
    struct timespec ts = {(time_t)(remaining_ns / 1000000000ull),
                         (long)(remaining_ns % 1000000000ull)};
#ifdef __APPLE__
    pthread_cond_timedwait_relative_np(&timing_changed, &timing_lock, &ts);
#else
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    ts.tv_sec += now.tv_sec;
    ts.tv_nsec += now.tv_nsec;
    if (ts.tv_nsec >= 1000000000) { ts.tv_nsec -= 1000000000; ts.tv_sec++; }
    pthread_cond_timedwait(&timing_changed, &timing_lock, &ts);
#endif
}

static void *timing_run(void *unused)
{
    (void)unused;
    g_fs_base = timing_tib;
    g_esp = timing_stack;
    g_ebp = g_seh_ebp = timing_stack;
    timing_write(timing_tib + 8, timing_stack + 16 - XBOX_THREAD_STACK_SIZE);

    pthread_mutex_lock(&timing_lock);
    uint64_t epoch = timing_now(), tick = 1;
    unsigned hz = timing_hz;
    while (!timing_stopping) {
        if (!timing_callback) {
            pthread_cond_wait(&timing_changed, &timing_lock);
            epoch = timing_now(); tick = 1; hz = timing_hz;
            continue;
        }
        if (hz != timing_hz) { epoch = timing_now(); tick = 1; hz = timing_hz; }
        uint64_t deadline = epoch + tick * 1000000000ull / hz;
        uint64_t now = timing_now();
        if (now < deadline) { timing_wait(deadline - now); continue; }
        /* Absolute phase avoids accumulating callback duration/sleep error.
         * After host suspension, record elapsed blanks without a burst of
         * stale callbacks. There is one invocation per delivered blank. */
        uint64_t elapsed_tick = (now - epoch) * hz / 1000000000ull;
        if (elapsed_tick < tick) elapsed_tick = tick;
        timing_blank += (uint32_t)(elapsed_tick - tick + 1);
        tick = elapsed_tick + 1;
        uint32_t callback = timing_callback;
        timing_write(timing_data, timing_blank);
        timing_write(timing_data + 4, timing_swap_blank);
        timing_write(timing_data + 8, timing_swap_flags);
        timing_swap_blank = timing_swap_flags = 0;
        timing_in_callback = 1;
        pthread_mutex_unlock(&timing_lock);

        timing_guest_fn fn = recomp_lookup(callback);
        if (!fn) fn = recomp_lookup_manual(callback);
        if (!fn) {
            fprintf(stderr, "[wrath timing] callback 0x%08X has no AOT implementation\n", callback);
            abort();
        }
        /* D3DVBLANKCALLBACK is cdecl(D3DVBLANKDATA *). Its ret consumes
         * only the synthetic return address; caller removes the argument. */
        g_esp = timing_stack - 8;
        timing_write(g_esp, 0);
        timing_write(g_esp + 4, timing_data);
        fn();
        if (g_esp != timing_stack - 4) {
            fprintf(stderr, "[wrath timing] callback 0x%08X violated cdecl stack ABI\n", callback);
            abort();
        }
        g_esp = timing_stack;
        pthread_mutex_lock(&timing_lock);
        timing_in_callback = 0;
        pthread_cond_broadcast(&timing_changed);
    }
    pthread_mutex_unlock(&timing_lock);
    return NULL;
}

void wrath_vblank_set_refresh(uint32_t hz)
{
    /* Xbox default in this progressive NTSC mode is 60 Hz. A native display's
     * refresh does not change the game's requested clock. */
    if (!hz) hz = 60;
    if (hz < 24 || hz > 240) {
        fprintf(stderr, "[wrath timing] unsupported refresh %u Hz\n", hz);
        abort();
    }
    pthread_mutex_lock(&timing_lock);
    timing_hz = hz;
    pthread_cond_broadcast(&timing_changed);
    pthread_mutex_unlock(&timing_lock);
}

void wrath_vblank_notify_swap(void)
{
    pthread_mutex_lock(&timing_lock);
    timing_swap_blank = timing_blank;
    timing_swap_flags = 1; /* D3DVBLANK_SWAPDONE: host swap completed. */
    pthread_mutex_unlock(&timing_lock);
}

void wrath_vblank_shutdown(void)
{
    pthread_mutex_lock(&timing_lock);
    if (!timing_started) { pthread_mutex_unlock(&timing_lock); return; }
    timing_stopping = 1;
    pthread_cond_broadcast(&timing_changed);
    pthread_mutex_unlock(&timing_lock);
    pthread_join(timing_thread, NULL);
    xbox_HeapFree(timing_data);
    xbox_HeapFree(timing_tib);
    xbox_FreeThreadStack(timing_stack);
    pthread_mutex_lock(&timing_lock);
    timing_started = timing_stopping = 0;
    timing_callback = timing_stack = timing_tib = timing_data = 0;
    timing_blank = timing_swap_blank = timing_swap_flags = 0;
    pthread_mutex_unlock(&timing_lock);
}

/* 4361 SDK SetVerticalBlankCallback: mov eax,[esp+4]; mov ecx,[10EBF0];
 * mov [ecx+2430],eax; ret 4. Preserve those visible register/memory effects. */
void sub_000FEC90(void)
{
    uint32_t callback = timing_read(g_esp + 4);
    uint32_t device = timing_read(0x0010EBF0);
    if (!device) { fputs("[wrath timing] callback registration before device\n", stderr); abort(); }
    pthread_mutex_lock(&timing_lock);
    timing_write(device + 0x2430, callback);
    timing_callback = callback;
    if (callback && !timing_started) {
        timing_stack = xbox_AllocThreadStack();
        timing_tib = xbox_AllocThreadTib();
        timing_data = xbox_HeapAlloc(12, 16);
        if (!timing_stack || !timing_tib || !timing_data ||
            pthread_create(&timing_thread, NULL, timing_run, NULL)) {
            fputs("[wrath timing] unable to allocate native callback worker\n", stderr);
            abort();
        }
        timing_started = 1;
        fprintf(stderr, "[wrath timing] native %u Hz vblank -> AOT callback 0x%08X\n", timing_hz, callback);
    }
    pthread_cond_broadcast(&timing_changed);
    /* Unregister waits for an outstanding invocation, except when called by
     * that callback itself. Once it returns no stale invocation can start. */
    while (!callback && timing_in_callback && !pthread_equal(pthread_self(), timing_thread))
        pthread_cond_wait(&timing_changed, &timing_lock);
    pthread_mutex_unlock(&timing_lock);
    g_eax = callback;
    g_ecx = device;
    g_esp += 8;
}
