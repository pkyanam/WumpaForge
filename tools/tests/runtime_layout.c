#include "kernel/xbox_runtime_layout.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    XboxRuntimeLayout l;
    assert(xbox_plan_runtime_layout(0x009500e0, 20, 64u << 20, &l));
    assert(l.rw_data == 0x00960000 && l.kernel_data == 0x009a0000);
    assert(l.tls_block == 0x009d0000 && l.stack_base == 0x009e0000);
    assert(l.tls_thread >= l.tls_block + 20 && l.stack_base > l.tls_thread + 63);
    assert(l.heap_base == 0x011e0000);
    assert(xbox_plan_runtime_layout_with_stack(0x009500e0,20,0x10000,64u<<20,&l));
    assert(l.stack_base==0x009e0000 && l.stack_size==0x40000 && l.heap_base==0x00a20000);
    assert(xbox_plan_runtime_layout_with_stack(0x009500e0,20,0x41001,64u<<20,&l));
    assert(l.stack_size==0x50000 && l.heap_base==0x00a30000);
    assert(!xbox_plan_runtime_layout_with_stack(0x009500e0,20,0xffffffffu,64u<<20,&l));
    /* Larger images and large TLS templates stay disjoint without title constants. */
    assert(xbox_plan_runtime_layout(0x02001234, 0x20100, 64u << 20, &l));
    assert(l.rw_data >= 0x02001234 && l.tls_thread >= l.tls_block + 0x20100);
    assert(l.stack_base >= l.tls_thread + 64 && l.heap_base < (64u << 20));
    assert(!xbox_plan_runtime_layout(60u << 20, 20, 64u << 20, &l));
    assert(!xbox_plan_runtime_layout(0xfffffff0, 20, 64u << 20, &l));
    assert(!xbox_plan_runtime_layout(0x10000, 0xffffffffu, 64u << 20, &l));
    puts("PASS: runtime layout excludes images and TLS, preserves 64MB RAM, rejects overflow/exhaustion");
}
