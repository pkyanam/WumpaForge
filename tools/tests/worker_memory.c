/* Exercise the real allocator/TIB implementation without launching game code.
 * Inclusion grants fixture access to private loader state; unused loader/device
 * functions are dead-stripped by the standalone test link. */
#include "kernel/xbox_memory_layout.c"
#include <assert.h>
#include <pthread.h>

#define TEST_THREADS 4
#define TEST_ALLOCS 256
struct worker_fixture { uint32_t tag, tib, stack, blocks[TEST_ALLOCS]; };
static void *worker_fixture_run(void *opaque)
{
    struct worker_fixture *w = opaque;
    w->stack = xbox_AllocThreadStack();
    w->tib = xbox_AllocThreadTib();
    assert(w->stack && w->tib);
    g_fs_base = w->tib;
    g_esp = w->stack;
    g_eax = w->tag;
    uint32_t *tib = (uint32_t *)((uintptr_t)w->tib + g_memory_offset);
    assert(tib[0] == 0xffffffffu && tib[6] == w->tib);
    uint32_t block_va = tib[1] - g_tls_total;
    uint32_t *block = (uint32_t *)((uintptr_t)block_va + g_memory_offset);
    assert(block[0] == block_va + g_tls_total && block[1] == 0x87654321u);
    block[1] = w->tag;
    for (unsigned i = 0; i < TEST_ALLOCS; i++) {
        w->blocks[i] = xbox_HeapAlloc(128, 16);
        assert(w->blocks[i] >= XBOX_HEAP_BASE && xbox_HeapBlockSize(w->blocks[i]) == 128);
        *(uint32_t *)((uintptr_t)w->blocks[i] + g_memory_offset) = w->tag;
    }
    assert(g_eax == w->tag && g_esp == w->stack && g_fs_base == w->tib);
    return NULL;
}
int main(void)
{
    g_memory_size = XBOX_TOTAL_RAM;
    g_memory_base = calloc(1, g_memory_size);
    assert(g_memory_base);
    g_memory_offset = (ptrdiff_t)(uintptr_t)g_memory_base;
    assert(xbox_plan_runtime_layout(0x009500e0, 20, XBOX_TOTAL_RAM, &g_xbox_runtime_layout));
    xbox_reset_allocators();
    g_tls_total = 20;
    g_tls_template_va = g_xbox_runtime_layout.tls_block;
    uint32_t *parent_tib = (uint32_t *)((uintptr_t)XBOX_TIB_MAIN + g_memory_offset);
    parent_tib[6] = XBOX_TIB_MAIN;
    uint32_t *template = (uint32_t *)((uintptr_t)g_tls_template_va + g_memory_offset);
    template[1] = 0x87654321u;
    g_eax = 0xabcdef01u;
    pthread_t threads[TEST_THREADS];
    struct worker_fixture workers[TEST_THREADS] = {0};
    for (unsigned i = 0; i < TEST_THREADS; i++) {
        workers[i].tag = i + 1;
        assert(!pthread_create(&threads[i], NULL, worker_fixture_run, &workers[i]));
    }
    for (unsigned i = 0; i < TEST_THREADS; i++) assert(!pthread_join(threads[i], NULL));
    assert(g_eax == 0xabcdef01u && g_fs_base == XBOX_TIB_MAIN && template[1] == 0x87654321u);
    for (unsigned i = 0; i < TEST_THREADS; i++) {
        for (unsigned j = i + 1; j < TEST_THREADS; j++) {
            assert(workers[i].tib != workers[j].tib && workers[i].stack != workers[j].stack);
            for (unsigned a = 0; a < TEST_ALLOCS; a++)
                for (unsigned b = 0; b < TEST_ALLOCS; b++)
                    assert(workers[i].blocks[a] != workers[j].blocks[b]);
        }
        for (unsigned a = 0; a < TEST_ALLOCS; a++) {
            assert(*(uint32_t *)((uintptr_t)workers[i].blocks[a] + g_memory_offset) == workers[i].tag);
            xbox_HeapFree(workers[i].blocks[a]);
        }
        xbox_HeapFree(workers[i].tib);
        xbox_FreeThreadStack(workers[i].stack);
    }
    assert(g_thread_stacks_used == 0);
    free(g_memory_base);
    puts("PASS: four real pthreads keep guest registers/TIB/TLS/stacks/heap blocks isolated and release stacks");
}
