#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#define RESOURCE_INDEX_BUFFER 7
struct Resource { uint32_t handle,data,bytes;unsigned type; };
static struct Resource resources[4096];
static unsigned capacity=2048, visits;
static int s_index_resource_dirty=1, fail_allocation;
static unsigned resource_capacity(void){return capacity;}
static struct Resource *resource_at(unsigned index){++visits;return &resources[index];}
static void *test_realloc(void *p,size_t n){return fail_allocation?NULL:realloc(p,n);}
#define realloc test_realloc
#include "index_resource_bounds.inc"
#undef realloc
static int reference(uint32_t indices,uint32_t count)
{
    for(unsigned i=0;i<capacity;++i){struct Resource *r=&resources[i];
        if(r->handle&&r->type==7&&indices>=r->data&&
           (uint64_t)indices<=(uint64_t)r->data+r->bytes&&
           (uint64_t)indices+(uint64_t)count*2>(uint64_t)r->data+r->bytes)return 0;
    }return 1;
}
int main(void)
{
    for(unsigned i=0;i<capacity;++i)resources[i]=(struct Resource){i+1,0x1000+i*4,128,i%97?2:7};
    uint32_t random=1;
    for(unsigned i=0;i<20000;++i){random=random*1664525+1013904223;
        uint32_t address=0x1000+(random&16383), count=(random>>20)&255;
        assert(index_resource_bounds(address,count)==reference(address,count));
    }
    assert(visits==capacity); // Warm checks never scan unrelated resources.
    resources[1]=(struct Resource){5,0x7000,16,7};s_index_resource_dirty=1;
    assert(!index_resource_bounds(0x7010,1)); // Inclusive end pointer must reject.
    resources[2]=(struct Resource){6,0x7008,2,7};s_index_resource_dirty=1;
    assert(!index_resource_bounds(0x7008,2)); // Overlap: larger container cannot hide small one.
    resources[2].handle=0;s_index_resource_dirty=1;
    assert(index_resource_bounds(0x7008,2)); // Released overlapping resource disappears.
    resources[2]=(struct Resource){6,0x7008,2,2};s_index_resource_dirty=1;
    assert(index_resource_bounds(0x7008,2)); // Reused slot as vertex resource is excluded.
    assert(index_resource_bounds(0x9000,3)); // Unknown literal guest pointer is allowed.
    assert(!index_resource_bounds(0x7000,UINT32_MAX));
    capacity=4096;fail_allocation=1;s_index_resource_dirty=1;
    assert(!index_resource_bounds(0x7010,1)&&s_index_resource_dirty);
    assert(index_resource_bounds(0x9000,3)); // Failed cache growth uses faithful full scan.
    fail_allocation=0;assert(!index_resource_bounds(0x7010,1)&&!s_index_resource_dirty);
    free(s_index_candidates);
    puts("PASS index candidates: 20000 reference comparisons, no warm full scan, creation/release/reuse, overlap/end, raw pointers, allocation fallback");
}
