/* Exercise the actual guest ABI bridge with instrumented allocation ownership. */
#include "kernel/kernel_bridge.c"
#include <assert.h>

RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_seh_ebp;
ptrdiff_t g_xbox_mem_offset;
static int allocation_live, release_calls;
uint32_t xbox_HeapBlockSize(uint32_t address)
{ return allocation_live && address>=0x6000 && address<0x9000 ? 0x9000-address : 0; }
int xbox_HeapFreeChecked(uint32_t address)
{
    ++release_calls;
    if (!allocation_live || address!=0x6000) return 0;
    allocation_live=0; return 1;
}
static void invoke(uint32_t base,uint32_t size,uint32_t type)
{
    g_esp=0x1000;
    BRIDGE_MEM32(0x1000)=0x2000; BRIDGE_MEM32(0x1004)=0x2008;
    BRIDGE_MEM32(0x1008)=type;
    BRIDGE_MEM32(0x2000)=base; BRIDGE_MEM32(0x2004)=0xABCDEF12;
    BRIDGE_MEM32(0x2008)=size; BRIDGE_MEM32(0x200C)=0x34567890;
    bridge_NtFreeVirtualMemory();
    assert(BRIDGE_MEM32(0x2004)==0xABCDEF12 && BRIDGE_MEM32(0x200C)==0x34567890);
}
int main(void)
{
    uint8_t memory[0x10000]={0}; g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    allocation_live=1;
    invoke(0x6000,0,MEM_RELEASE);
    assert(g_eax==0 && !allocation_live && release_calls==1);
    assert(!BRIDGE_MEM32(0x2000) && !BRIDGE_MEM32(0x2008));
    invoke(0x6000,0,MEM_RELEASE); assert(g_eax==0xC000000Du);
    allocation_live=1;
    invoke(0x6000,4096,MEM_RELEASE); assert(g_eax==0xC000000Du && allocation_live);
    invoke(0x7000,0,MEM_RELEASE); assert(g_eax==0xC000000Du && allocation_live);
    memset(memory+0x6000,0xA5,0x3000);
    invoke(0x7100,64,MEM_DECOMMIT);
    assert(g_eax==0 && allocation_live && BRIDGE_MEM32(0x2000)==0x7000 && BRIDGE_MEM32(0x2008)==4096);
    for (unsigned i=0x6000;i<0x9000;++i) assert(memory[i]==(i>=0x7000&&i<0x8000?0:0xA5));
    invoke(0x8800,4096,MEM_DECOMMIT); assert(g_eax==0xC000000Du && allocation_live);
    BRIDGE_MEM32(0x1000)=0xFFFFFFFF; bridge_NtFreeVirtualMemory(); assert(g_eax==0xC000000Du);
    puts("PASS: guest virtual release, DWORD output canaries, ownership errors, bounded decommit preserving reservation");
}
