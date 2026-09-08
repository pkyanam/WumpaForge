/* Actual heap implementation; assert partition invariants after fragmentation. */
#include "kernel/xbox_memory_layout.c"
#include <assert.h>
#include <stdio.h>

static void invariant(void)
{
    uint32_t next=XBOX_HEAP_BASE;
    for(int i=0;i<g_heap_block_count;i++) {
        XboxHeapBlock b=g_heap_blocks[i];
        assert(b.addr==next && b.size && (uint64_t)b.addr+b.size<=XBOX_HEAP_TOP);
        if(i)assert(!(b.free && g_heap_blocks[i-1].free));
        next=b.addr+b.size;
    }
    assert(next==g_heap_next);
    if(g_heap_block_count)assert(!g_heap_blocks[g_heap_block_count-1].free);
}
static void reset(void) { xbox_reset_allocators();invariant(); }
static unsigned char *ptr(uint32_t p) { return (unsigned char*)g_memory_base+p; }
int main(void)
{
    g_memory_size=XBOX_TOTAL_RAM;g_memory_base=calloc(1,g_memory_size);assert(g_memory_base);
    g_memory_offset=(ptrdiff_t)(uintptr_t)g_memory_base;
    assert(xbox_plan_runtime_layout_with_stack(0x9500e0,20,0x10000,XBOX_TOTAL_RAM,&g_xbox_runtime_layout));
    reset();
    /* Reproduce a tiny resource header reusing a released multi-MB staging
     * buffer. The remaining bytes must still satisfy the next large request. */
    uint32_t big=xbox_HeapAlloc(4u<<20,16),guard=xbox_HeapAlloc(16,16);assert(big&&guard);
    memset(ptr(big),0xa5,4u<<20);assert(xbox_HeapFreeChecked(big));invariant();
    uint32_t tiny=xbox_HeapAlloc(24,16);assert(tiny==big && xbox_HeapBlockSize(tiny)==24);
    assert(xbox_HeapBlockSize(tiny+8)==16);
    for(unsigned i=0;i<24;i++)assert(ptr(tiny)[i]==0);
    assert(ptr(tiny)[24]==0xa5);
    uint32_t remainder=xbox_HeapAlloc((4u<<20)-24,4);assert(remainder==tiny+24);
    for(unsigned i=0;i<(4u<<20)-24;i++)assert(ptr(remainder)[i]==0);
    assert(g_heap_next==guard+16);invariant();
    assert(!xbox_HeapFreeChecked(tiny+4) && !xbox_HeapFreeChecked(0) && !xbox_HeapFreeChecked(0x1234));
    assert(xbox_HeapFreeChecked(tiny) && !xbox_HeapFreeChecked(tiny));
    xbox_HeapFree(remainder);assert(g_heap_block_count==2 && g_heap_blocks[0].size==(4u<<20));
    xbox_HeapFree(guard);assert(g_heap_block_count==0 && g_heap_next==XBOX_HEAP_BASE);invariant();

    /* Best fit keeps a larger free range available despite its earlier address. */
    uint32_t large=xbox_HeapAlloc(4096,16),g1=xbox_HeapAlloc(16,16);
    uint32_t small=xbox_HeapAlloc(128,16),g2=xbox_HeapAlloc(16,16);
    xbox_HeapFree(large);xbox_HeapFree(small);
    assert(xbox_HeapAlloc(64,16)==small);invariant();
    assert(xbox_HeapAlloc(4096,16)==large);(void)g1;(void)g2;

    /* Interior alignment produces a reusable prefix and suffix, never a gap. */
    reset();uint32_t head=xbox_HeapAlloc(20,4);big=xbox_HeapAlloc(1u<<20,4);guard=xbox_HeapAlloc(16,4);
    xbox_HeapFree(big);uint32_t aligned=xbox_HeapAlloc(4096,4096);
    assert(aligned>big && aligned%4096==0 && aligned+4096<guard);invariant();
    xbox_HeapFree(aligned);assert(g_heap_blocks[1].free && g_heap_blocks[1].size==(1u<<20));
    xbox_HeapFree(head);xbox_HeapFree(guard);assert(!g_heap_block_count);invariant();

    /* Repeated adjacent frees previously left tombstones blocking coalescing. */
    reset();uint32_t blocks[4];for(unsigned i=0;i<4;i++)blocks[i]=xbox_HeapAlloc(128,16);
    for(unsigned i=0;i<3;i++){xbox_HeapFree(blocks[i]);invariant();}
    assert(g_heap_block_count==2 && g_heap_blocks[0].size==384);
    xbox_HeapFree(blocks[3]);assert(!g_heap_block_count);invariant();
    uint32_t next=g_heap_next;
    assert(!xbox_HeapAlloc(16,6) && !xbox_HeapAlloc(UINT32_MAX,16));assert(g_heap_next==next);invariant();

    /* Fixed metadata capacity must not silently create an untracked block.
     * Construct a valid dense fixture directly to avoid quadratic setup work. */
    reset();g_heap_block_count=XBOX_HEAP_MAX_BLOCKS;
    for(int i=0;i<g_heap_block_count;i++)g_heap_blocks[i]=(XboxHeapBlock){XBOX_HEAP_BASE+(uint32_t)i*16,16,0};
    g_heap_next=XBOX_HEAP_BASE+XBOX_HEAP_MAX_BLOCKS*16;invariant();next=g_heap_next;
    assert(!xbox_HeapAlloc(16,4) && g_heap_next==next);
    uint32_t hole=XBOX_HEAP_BASE+100*16;xbox_HeapFree(hole);assert(xbox_HeapAlloc(16,4)==hole);invariant();
    free(g_memory_base);
    puts("PASS: best-fit split/zero/size semantics, aligned subranges/gaps, compact coalescing/tail reclaim, checked free, overflow and metadata exhaustion");
}
