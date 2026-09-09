/* Opt-in observation of the original heap. No guest writes, replacement or recovery.
 * Snapshot precedes the original allocator's lock and may observe concurrent edits;
 * an anomaly is a diagnostic lead, not proof of corruption by itself. */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <pthread.h>
extern ptrdiff_t g_xbox_mem_offset;
extern _Thread_local uint32_t g_esp,g_eax;
extern void __real_sub_000F1629(void);

static int heap_readable(uint32_t address,uint32_t size)
{
    return address>=0x1000u&&address<0x4000000u&&size<=0x4000000u-address;
}
static uint32_t heap_word(uint32_t address)
{
    uint32_t value=0;
    if(heap_readable(address,4))memcpy(&value,(const void*)(g_xbox_mem_offset+(uintptr_t)address),4);
    return value;
}
struct HeapSnapshot {uint32_t signature,flags,free_units,first,last,bad_bucket,bad_node,walked;};
static struct HeapSnapshot heap_snapshot(uint32_t heap)
{
    struct HeapSnapshot s={0};s.bad_bucket=UINT32_MAX;
    if(!heap_readable(heap,0x580)){s.bad_bucket=128;s.bad_node=heap;return s;}
    s.signature=heap_word(heap+0x10);s.flags=heap_word(heap+0x18);s.free_units=heap_word(heap+0x30);
    s.first=heap_word(heap+0x180);s.last=heap_word(heap+0x184);
    for(unsigned i=0;i<128;++i) {
        uint32_t head=heap+0x180+i*8,next=heap_word(head),previous=heap_word(head+4);
        if((next&7)||(previous&7)||!heap_readable(next,8)||!heap_readable(previous,8)||
           heap_word(next+4)!=head||heap_word(previous)!=head) {
            s.bad_bucket=i;s.bad_node=next;return s;
        }
    }
    /* Bucket zero is the sorted large-block list walked at EF7DB. */
    uint32_t head=heap+0x180,node=s.first,previous=head;
    while(node!=head&&s.walked<64) {
        if((node&7)||node<0x1008||!heap_readable(node-8,16)||heap_word(node+4)!=previous){s.bad_bucket=0;s.bad_node=node;return s;}
        previous=node;node=heap_word(node);++s.walked;
    }
    if(node!=head&&(!node||!heap_readable(node,8))){s.bad_bucket=0;s.bad_node=node;}
    return s;
}
static int heap_trace_enabled(void)
{
    static atomic_int cached=ATOMIC_VAR_INIT(-1);int enabled=atomic_load_explicit(&cached,memory_order_relaxed);
    if(enabled<0){const char *v=getenv("WRATH_TRACE_HEAP");enabled=v&&!strcmp(v,"1");atomic_store_explicit(&cached,enabled,memory_order_relaxed);}
    return enabled;
}
void __wrap_sub_000F1629(void)
{
    if(!heap_trace_enabled()){__real_sub_000F1629();return;}
    static atomic_uint calls,anomalies,stateblocks;
    unsigned sequence=atomic_fetch_add_explicit(&calls,1,memory_order_relaxed)+1;
    uint32_t stack=g_esp,caller=heap_word(stack),flags=heap_word(stack+4),request=heap_word(stack+8),heap=heap_word(0x9453B0);
    struct HeapSnapshot before=heap_snapshot(heap);
    int bad=before.bad_bucket!=UINT32_MAX;
    unsigned anomaly=bad?atomic_fetch_add_explicit(&anomalies,1,memory_order_relaxed)+1:0;
    unsigned stateblock=caller==0x1016D3u?atomic_fetch_add_explicit(&stateblocks,1,memory_order_relaxed)+1:0;
    int report=sequence<=16||(bad&&anomaly<=32)||(stateblock&&stateblock<=64);
    if(report) {
        fprintf(stderr,"[shield heap] before seq%u thread%llx caller0x%X request%u flags0x%X heap0x%X sig0x%X heapflags0x%X freeunits%u large0x%X/0x%X walked%u badbucket%u badnode0x%X snapshot_unlocked=1\n",
            sequence,(unsigned long long)(uintptr_t)pthread_self(),caller,request,flags,heap,before.signature,before.flags,before.free_units,before.first,before.last,before.walked,before.bad_bucket,before.bad_node);
        fflush(stderr);
    }
    __real_sub_000F1629();
    if(report) {
        uint32_t result=g_eax;struct HeapSnapshot after=heap_snapshot(heap);
        fprintf(stderr,"[shield heap] after seq%u result0x%X large0x%X/0x%X badbucket%u badnode0x%X\n",sequence,result,after.first,after.last,after.bad_bucket,after.bad_node);
    }
}
