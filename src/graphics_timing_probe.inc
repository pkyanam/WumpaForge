/* Opt-in CPU-side profiling. No GPU synchronization or per-draw logging. */
#include <time.h>
static int wrath_profile_enabled(void)
{
    static int enabled=-1;
    if (enabled<0) { const char *v=getenv("WRATH_PROFILE"); enabled=v && !strcmp(v,"1"); }
    return enabled;
}
static uint64_t wrath_profile_clock(clockid_t clock)
{
    struct timespec t;
    if (clock_gettime(clock,&t)) return 0;
    return (uint64_t)t.tv_sec*1000000000ull+(uint64_t)t.tv_nsec;
}
struct WrathFrameProfile {
    uint64_t last_end,last_cpu,work,work_cpu,present,interval,max_interval;
    uint64_t uploads,upload_bytes,upload_time,binds,rebinds,draws,stream_bytes;
    unsigned frames,intervals;
};
static struct WrathFrameProfile wrath_profiles[2];
static struct WrathFrameProfile *wrath_profile_current(void)
{ return &wrath_profiles[window_thread()?0:1]; }
static uint64_t wrath_profile_begin(void)
{ return wrath_profile_enabled()?wrath_profile_clock(CLOCK_MONOTONIC):0; }
static void wrath_profile_upload(uint64_t start,uint64_t rgba_bytes)
{
    if (!start) return;
    struct WrathFrameProfile *p=wrath_profile_current();
    ++p->uploads; p->upload_bytes+=rgba_bytes;
    p->upload_time+=wrath_profile_clock(CLOCK_MONOTONIC)-start;
}
static void wrath_profile_bind(uint32_t handle,uint32_t previous)
{
    if (!wrath_profile_enabled()) return;
    struct WrathFrameProfile *p=wrath_profile_current();
    ++p->binds; p->rebinds+=handle && handle==previous;
}
static void wrath_profile_draw(uint64_t bytes)
{
    if (!wrath_profile_enabled()) return;
    struct WrathFrameProfile *p=wrath_profile_current(); ++p->draws; p->stream_bytes+=bytes;
}
static uint64_t wrath_profile_present_begin(void)
{
    uint64_t now=wrath_profile_begin();
    if (!now) return 0;
    struct WrathFrameProfile *p=wrath_profile_current();
    uint64_t cpu=wrath_profile_clock(CLOCK_THREAD_CPUTIME_ID);
    if (p->last_end) { p->work+=now-p->last_end; p->work_cpu+=cpu-p->last_cpu; }
    return now;
}
static void wrath_profile_present_end(uint64_t start)
{
    if (!start) return;
    struct WrathFrameProfile *p=wrath_profile_current();
    uint64_t now=wrath_profile_clock(CLOCK_MONOTONIC);
    uint64_t cpu=wrath_profile_clock(CLOCK_THREAD_CPUTIME_ID);
    p->present+=now-start; ++p->frames;
    if (p->last_end) {
        uint64_t delta=now-p->last_end;
        p->interval+=delta; ++p->intervals;
        if (delta>p->max_interval) p->max_interval=delta;
    }
    p->last_end=now; p->last_cpu=cpu;
    if (p->frames<60) return;
    uint32_t scene=read32(0x8F4DFC),instance=scene<32?read32(0x1BA928+scene*4):0;
    float position=0,step=0;
    if (instance>=0x1000 && instance<=0x4000000-0x78) {
        memcpy(&position,guest_ptr(instance+0x70),4); memcpy(&step,guest_ptr(instance+0x74),4);
    }
    double n=p->frames,interval_n=p->intervals?p->intervals:1;
    fprintf(stderr,"[wrath profile] thread=%s frame=%u fps=%.2f interval_ms=%.3f max_ms=%.3f work_wall_ms=%.3f work_cpu_ms=%.3f present_ms=%.3f uploads=%llu upload_MiB=%.3f upload_ms_per_frame=%.3f binds=%llu same_binds=%llu draws=%llu draw_input_MiB=%.3f scene=%u anim=%.3f step=%.3f audio_gate=%u vblank=%u\n",
        window_thread()?"main":"worker",read32(GUEST_SWAP_COUNT),
        p->interval?interval_n*1e9/p->interval:0,p->interval/interval_n/1e6,p->max_interval/1e6,
        p->work/interval_n/1e6,p->work_cpu/interval_n/1e6,p->present/n/1e6,
        (unsigned long long)p->uploads,p->upload_bytes/1048576.0,p->upload_time/n/1e6,
        (unsigned long long)p->binds,(unsigned long long)p->rebinds,(unsigned long long)p->draws,
        p->stream_bytes/1048576.0,scene,position,step,read32(0x1BAA2C),read32(0x1BABB0));
    memset(p,0,sizeof(*p)); p->last_end=now; p->last_cpu=cpu;
}
