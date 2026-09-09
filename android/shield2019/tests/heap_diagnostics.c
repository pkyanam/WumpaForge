#include <assert.h>
#include "../heap_diagnostics.c"
ptrdiff_t g_xbox_mem_offset;
_Thread_local uint32_t g_esp,g_eax;
static unsigned original_calls;
void __real_sub_000F1629(void){++original_calls;g_eax=0x123456;g_esp+=12;}
static void put(uint32_t address,uint32_t value){memcpy((void*)(g_xbox_mem_offset+address),&value,4);}
int main(void){
    void *memory=calloc(1,0x4000000);assert(memory);g_xbox_mem_offset=(ptrdiff_t)memory;
    uint32_t heap=0x10000,head=heap+0x180;
    for(unsigned i=0;i<128;++i){put(head+i*8,head+i*8);put(head+i*8+4,head+i*8);}
    put(heap+0x10,0xEEFFEEFF);put(0x9453B0,heap);
    struct HeapSnapshot s=heap_snapshot(heap);assert(s.bad_bucket==UINT32_MAX&&s.signature==0xEEFFEEFF);
    put(head,0);s=heap_snapshot(heap);assert(s.bad_bucket==0&&s.bad_node==0);
    put(head,head);put(head+8,0xFFFFFFF8);s=heap_snapshot(heap);assert(s.bad_bucket==1&&s.bad_node==0xFFFFFFF8);put(head+8,head+8);
    s=heap_snapshot(0xFFFFFFF8);assert(s.bad_bucket==128);
    uint32_t a=0x20008,b=0x20028;put(head,a);put(head+4,b);put(a,b);put(a+4,head);put(b,head);put(b+4,a);
    s=heap_snapshot(heap);assert(s.bad_bucket==UINT32_MAX&&s.walked==2);
    put(a,0);s=heap_snapshot(heap);assert(s.bad_bucket==0&&s.bad_node==0);put(a,b);
    g_esp=0x9000;put(g_esp,0x1012DA);put(g_esp+4,0);put(g_esp+8,1234);setenv("WRATH_TRACE_HEAP","1",1);
    uint8_t saved[0x580];memcpy(saved,(void*)(g_xbox_mem_offset+heap),sizeof(saved));
    __wrap_sub_000F1629();assert(original_calls==1&&g_eax==0x123456&&g_esp==0x900C);
    assert(!memcmp(saved,(void*)(g_xbox_mem_offset+heap),sizeof(saved)));
    free(memory);puts("PASS heap observer: healthy/broken lists, invalid pointers, original call/ABI preserved, no heap writes");
}
