/* Optional bounded measurement with the actual native context and locks.
 * Must run after the smoke's outer graphics lock has been released. */
static void test_context_profile_bench(void)
{
    if(!getenv("WRATH_BENCH_CONTEXT")) return;
    const unsigned count=200000;
    for(unsigned round=0;round<3;++round) {
        uint64_t start=wrath_profile_clock(CLOCK_MONOTONIC);
        for(unsigned i=0;i<count;++i) {
            assert(xbox_D3D8GLAcquire());xbox_D3D8GLRelease();
        }
        uint64_t elapsed=wrath_profile_clock(CLOCK_MONOTONIC)-start;
        fprintf(stderr,"[context benchmark] context_timing=%s round=%u pairs=%u ns_per_pair=%.3f\n",
            getenv("WRATH_PROFILE_CONTEXT")?getenv("WRATH_PROFILE_CONTEXT"):"0",round,count,(double)elapsed/count);
    }
}
