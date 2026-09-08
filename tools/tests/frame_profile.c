/* Deterministic aggregate check; no GL, XBE, or guest execution. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define GUEST_SWAP_COUNT 1
static uint64_t wall,cpu;
static int fake_clock(clockid_t which,struct timespec *out)
{
    uint64_t now=which==CLOCK_THREAD_CPUTIME_ID?cpu:wall;
    out->tv_sec=now/1000000000ull; out->tv_nsec=now%1000000000ull; return 0;
}
static int window_thread(void) { return 1; }
static uint32_t read32(uint32_t address) { return address==0x8F4DFC?~0u:60; }
static void *guest_ptr(uint32_t address) { (void)address; abort(); }
#define clock_gettime fake_clock
#include "../../src/graphics_timing_probe.inc"
int main(void)
{
    assert(!setenv("WRATH_PROFILE","1",1));
    for (unsigned i=0;i<60;++i) {
        wall=(uint64_t)i*16666667+1000000;
        cpu=(uint64_t)i*5000000+100000;
        uint64_t upload=wrath_profile_begin(); wall+=1000000;
        wrath_profile_upload(upload,4096);
        wrath_profile_bind(1,1); wrath_profile_draw(8192);
        wall=(uint64_t)i*16666667+10000000;
        uint64_t start=wrath_profile_present_begin();
        wall=(uint64_t)(i+1)*16666667; cpu=(uint64_t)(i+1)*5000000;
        wrath_profile_present_end(start);
    }
    assert(wrath_profiles[0].frames==0 && wrath_profiles[0].last_end==wall);
    assert(wrath_profiles[0].uploads==0 && wrath_profiles[1].last_end==0);
    puts("PASS: deterministic 60-frame aggregation and thread window reset");
}
