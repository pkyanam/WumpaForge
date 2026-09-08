#include <assert.h>
#include "../../src/crt_bridge.c"
_Thread_local uint32_t g_eax, g_esp;
ptrdiff_t g_xbox_mem_offset;
size_t g_xbox_total_ram = 0x10000;
int main(void)
{
    unsigned char *ram = calloc(1, g_xbox_total_ram);
    assert(ram); g_xbox_mem_offset = (ptrdiff_t)ram;
    /* Exercise both overlap directions, alignment tails, zero and long copies.
     * Expected bytes come from a snapshot, independent of memmove itself. */
    for (unsigned n=0;n<257;++n) for (unsigned shift=0;shift<9;++shift)
        for (unsigned backwards=0;backwards<2;++backwards) {
            unsigned char expected[1024], before[1024];
            for (unsigned i=0;i<1024;++i) ram[0x2000+i]=before[i]=expected[i]=(i*71)^n;
            uint32_t src=0x2100+(backwards?0:shift), dst=0x2100+(backwards?shift:0);
            for (unsigned i=0;i<n;++i) expected[dst-0x2000+i]=before[src-0x2000+i];
            uint32_t stack[]={0x12345678,dst,src,n};g_esp=0x1000;
            memcpy(ram+g_esp,stack,sizeof(stack));
            sub_000F5DD0();
            assert(g_eax==dst&&g_esp==0x1004);
            assert(!memcmp(ram+0x2000,expected,1024));
            assert(!memcmp(ram+0x1000,stack,sizeof(stack)));
        }
    free(ram);puts("CRT memmove overlap/alignment/cdecl tests passed");
}
