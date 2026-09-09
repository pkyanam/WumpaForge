#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static unsigned allocations;static int fail_grow,fail_temp;
static void *test_malloc(size_t n){++allocations;return fail_temp?NULL:malloc(n);}
static void *test_realloc(void *p,size_t n){++allocations;return fail_grow?NULL:realloc(p,n);}
#define malloc test_malloc
#define realloc test_realloc
#include "index_scratch.inc"
#undef malloc
#undef realloc
int main(void)
{
    struct IndexScratchLease a=index_scratch_acquire(100);assert(a.data&&a.retained);
    memset(a.data,0x61,100);unsigned before=allocations;
    struct IndexScratchLease nested=index_scratch_acquire(100);assert(nested.data&&!nested.retained&&nested.data!=a.data);
    memset(nested.data,0x62,100);assert(a.data[0]==0x61);index_scratch_release(nested);index_scratch_release(a);
    before=allocations;
    for(unsigned i=0;i<10000;++i){a=index_scratch_acquire(100);assert(a.data&&a.retained);memset(a.data,(int)i,100);index_scratch_release(a);}
    assert(allocations==before);
    fail_grow=1;a=index_scratch_acquire(8192);assert(a.data&&!a.retained);index_scratch_release(a);
    fail_temp=1;a=index_scratch_acquire(8192);assert(!a.data&&!a.retained);index_scratch_release(a);
    assert(!s_index_scratch_busy&&s_index_scratch_capacity==4096);
    fail_temp=fail_grow=0;a=index_scratch_acquire(INDEX_SCRATCH_LIMIT);assert(a.retained&&s_index_scratch_capacity==INDEX_SCRATCH_LIMIT);index_scratch_release(a);
    a=index_scratch_acquire(INDEX_SCRATCH_LIMIT+1);assert(a.data&&!a.retained);index_scratch_release(a);
    assert(s_index_scratch_capacity==INDEX_SCRATCH_LIMIT);
    a=index_scratch_acquire(0);assert(!a.data);index_scratch_release(a);
    free(s_index_scratch);
    puts("PASS indexed scratch: 10000 allocation-free reuses, nested alias isolation, growth/OOM fallback, 1MiB retained cap");
}
