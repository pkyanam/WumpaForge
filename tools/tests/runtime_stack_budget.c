/* Actual memory allocator and compatibility worker-stack pool; no game launch. */
#include "kernel/xbox_memory_layout.c"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    g_memory_size=XBOX_TOTAL_RAM;
    g_memory_base=calloc(1,g_memory_size);assert(g_memory_base);
    g_memory_offset=(ptrdiff_t)(uintptr_t)g_memory_base;
    assert(xbox_plan_runtime_layout_with_stack(0x9500e0,20,0x10000,XBOX_TOTAL_RAM,&g_xbox_runtime_layout));
    xbox_reset_allocators();
    assert(XBOX_STACK_SIZE==0x40000 && XBOX_HEAP_BASE==0xa20000);
    unsigned char *main_stack=(unsigned char*)g_memory_base+XBOX_STACK_BASE;
    memset(main_stack,0x5a,XBOX_STACK_SIZE);
    int slots[XBOX_WORKER_STACK_COUNT];
    uint32_t tops[XBOX_WORKER_STACK_COUNT];
    for(unsigned i=0;i<XBOX_WORKER_STACK_COUNT;i++) {
        slots[i]=xbox_worker_stack_alloc();assert(slots[i]>=0);
        tops[i]=XBOX_WORKER_STACK_TOP(slots[i]);
        uint32_t base=tops[i]+16-XBOX_WORKER_STACK_SIZE;
        assert(base>=XBOX_HEAP_BASE && xbox_HeapBlockSize(base)==XBOX_WORKER_STACK_SIZE);
        memset((unsigned char*)g_memory_base+base,(int)i,XBOX_WORKER_STACK_SIZE);
        for(unsigned j=0;j<i;j++)assert(tops[i]!=tops[j]);
    }
    assert(xbox_worker_stack_alloc()==-1);
    for(unsigned i=0;i<XBOX_STACK_SIZE;i++)assert(main_stack[i]==0x5a);
    for(unsigned i=0;i<XBOX_WORKER_STACK_COUNT;i++) {
        uint32_t base=tops[i]+16-XBOX_WORKER_STACK_SIZE;
        assert(*((unsigned char*)g_memory_base+base)==i);
        xbox_worker_stack_free(slots[i]);assert(XBOX_WORKER_STACK_TOP(slots[i])==0);
    }
    assert(XBOX_WORKER_STACK_TOP(-1)==0);
    /* Replay the reported aggregate allocation pressure plus one live timer.
     * No RAM increase or change to physical address aliases/masks is needed. */
    xbox_reset_allocators();
    int timer=xbox_worker_stack_alloc();assert(timer>=0);
    uint32_t previous=xbox_HeapAlloc(46256276,16);assert(previous);
    uint32_t failed_request=xbox_HeapAlloc(4279360,16);assert(failed_request);
    assert(failed_request+4279360<=XBOX_TOTAL_RAM);
    printf("PASS: main stack256KiB, heap%u bytes, 16 isolated lazy worker stacks; boot20 allocation pressure plus timer fits retail64MiB\n",XBOX_TOTAL_RAM-XBOX_HEAP_BASE);
    xbox_HeapFree(previous);xbox_HeapFree(failed_request);xbox_worker_stack_free(timer);
    free(g_memory_base);
}
