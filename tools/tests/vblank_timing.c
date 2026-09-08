/* Real native timer + allocator/TIB integration, no game assets required. */
#include "kernel/xbox_memory_layout.c"
#include "../../src/timing_bridge.c"
#include <assert.h>
#include <stdatomic.h>

static atomic_uint calls, swap_seen;
static uint32_t test_main_stack, test_main_tib;
static void compiled_callback(void)
{
    assert(g_esp != test_main_stack && g_fs_base != test_main_tib);
    assert(timing_read(g_fs_base + 0x18) == g_fs_base);
    assert(timing_read(g_fs_base + 8) == timing_stack + 16 - XBOX_THREAD_STACK_SIZE);
    uint32_t data = timing_read(g_esp + 4);
    assert(data == timing_data && timing_read(data) > 0);
    if (timing_read(data + 8) & 1) atomic_store(&swap_seen, 1);
    atomic_fetch_add(&calls, 1);
    g_eax = 0xdeadbeef;
    g_esp += 4; /* compiled cdecl ret */
}
timing_guest_fn recomp_lookup(uint32_t va) { return va == 0x12340 ? compiled_callback : NULL; }
timing_guest_fn recomp_lookup_manual(uint32_t va) { (void)va; return NULL; }
static void set_callback(uint32_t va)
{
    g_esp = test_main_stack - 8;
    timing_write(g_esp + 4, va);
    sub_000FEC90();
    assert(g_esp == test_main_stack && g_eax == va);
    assert(timing_read(0x10C110 + 0x2430) == va);
}
static void pause_ms(long ms)
{
    struct timespec ts = {ms / 1000, ms % 1000 * 1000000};
    nanosleep(&ts, NULL);
}
int main(void)
{
    g_memory_size = XBOX_TOTAL_RAM;
    g_memory_base = calloc(1, g_memory_size);
    assert(g_memory_base);
    g_memory_offset = (ptrdiff_t)(uintptr_t)g_memory_base;
    assert(xbox_plan_runtime_layout(0x9500e0, 20, XBOX_TOTAL_RAM, &g_xbox_runtime_layout));
    xbox_reset_allocators();
    g_tls_total = 20;
    g_tls_template_va = g_xbox_runtime_layout.tls_block;
    test_main_tib = g_fs_base;
    test_main_stack = XBOX_STACK_TOP;
    timing_write(0x10EBF0, 0x10C110);
    wrath_vblank_set_refresh(60);
    uint64_t begin = timing_now();
    set_callback(0x12340);
    g_eax = 0xabcddcba;
    pause_ms(1100);
    uint64_t elapsed = timing_now() - begin;
    assert(g_eax == 0xabcddcba && g_fs_base == test_main_tib && g_esp == test_main_stack);
    unsigned count = atomic_load(&calls);
    double expected = (double)elapsed * 60 / 1e9;
    assert(count >= expected - 4 && count <= expected + 2);
    wrath_vblank_notify_swap();
    pause_ms(50);
    assert(atomic_load(&swap_seen));
    set_callback(0);
    count = atomic_load(&calls);
    pause_ms(50);
    assert(atomic_load(&calls) == count);
    wrath_vblank_set_refresh(50);
    begin = timing_now();
    set_callback(0x12340);
    pause_ms(300);
    elapsed = timing_now() - begin;
    set_callback(0);
    unsigned count50 = atomic_load(&calls) - count;
    expected = (double)elapsed * 50 / 1e9;
    assert(count50 >= expected - 3 && count50 <= expected + 2);
    wrath_vblank_shutdown();
    assert(g_thread_stacks_used == 0 && !timing_started);
    free(g_memory_base);
    printf("PASS: native 60/50 Hz callbacks (%u first interval), cdecl ABI, isolated stack/TIB/registers, swap data, unregister, shutdown\n", count);
}
