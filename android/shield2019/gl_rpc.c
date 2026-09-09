#include "gl_rpc.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
/* One borrowed payload at a time, bounded independently of producer count.
 * submit serializes whole calls; queue protects handoff/completion only. */
static pthread_mutex_t submit=PTHREAD_MUTEX_INITIALIZER,queue=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed=PTHREAD_COND_INITIALIZER;
static pthread_t worker;
static _Atomic int active;
static _Thread_local int owner;
static SDL_Window *rpc_window;
static SDL_GLContext rpc_context;
static void (*pending)(void *);
static void *payload;
static int ready,success,done,stopping;
static unsigned long long calls;
/* Extra clocks and name lookup are opt-in; normal RPC takes neither path. */
static int profiling;
static uint64_t execution_wall,execution_cpu;
struct RPCProfile {
    char name[48];
    uint64_t count,wall,caller_cpu,render_wall,render_cpu,maximum;
};
static struct RPCProfile profiles[128];
static unsigned profile_frames;
static uint64_t profile_untracked;
static uint64_t rpc_clock(clockid_t clock)
{
    struct timespec value;
    if(clock_gettime(clock,&value))abort();
    return (uint64_t)value.tv_sec*1000000000ull+(uint64_t)value.tv_nsec;
}
/* Caller owns submit; worker has completed and published its timings. */
static void profile_add(const char *name,uint64_t wall,uint64_t cpu)
{
    for(unsigned i=0;i<128;++i) {
        struct RPCProfile *p=&profiles[i];
        if(p->count && strcmp(p->name,name))continue;
        if(!p->count)snprintf(p->name,sizeof(p->name),"%s",name);
        ++p->count;p->wall+=wall;p->caller_cpu+=cpu;
        p->render_wall+=execution_wall;p->render_cpu+=execution_cpu;
        if(wall>p->maximum)p->maximum=wall;
        return;
    }
    ++profile_untracked;
}
static void lock(pthread_mutex_t *mutex);
static void unlock(pthread_mutex_t *mutex);
static void profile_present(void)
{
    if(!profiling || !wumpa_gl_rpc_active())return;
    lock(&submit);
    if(++profile_frames==60) {
        uint64_t count=0,wall=0,caller_cpu=0,render_wall=0,render_cpu=0;
        for(unsigned i=0;i<128;++i) {
            count+=profiles[i].count;wall+=profiles[i].wall;caller_cpu+=profiles[i].caller_cpu;
            render_wall+=profiles[i].render_wall;render_cpu+=profiles[i].render_cpu;
        }
        fprintf(stderr,"[shield rpc profile] swaps=60 all_callers=1 calls=%llu untracked=%llu total_ms_per_frame=%.3f caller_cpu_ms=%.3f render_wall_ms=%.3f render_cpu_ms=%.3f handoff_ms=%.3f\n",
            (unsigned long long)count,(unsigned long long)profile_untracked,wall/60e6,caller_cpu/60e6,
            render_wall/60e6,render_cpu/60e6,(wall>=render_wall?wall-render_wall:0)/60e6);
        unsigned char printed[128]={0};
        for(unsigned rank=0;rank<12;++rank) {
            int best=-1;
            for(unsigned i=0;i<128;++i)
                if(!printed[i] && profiles[i].count && (best<0 || profiles[i].wall>profiles[best].wall))best=(int)i;
            if(best<0)break;
            const struct RPCProfile *p=&profiles[best];printed[best]=1;
            fprintf(stderr,"[shield rpc call] name=%s count=%llu total_ms_per_frame=%.3f caller_cpu_ms=%.3f render_wall_ms=%.3f render_cpu_ms=%.3f handoff_ms=%.3f max_ms=%.3f\n",
                p->name,(unsigned long long)p->count,p->wall/60e6,p->caller_cpu/60e6,
                p->render_wall/60e6,p->render_cpu/60e6,(p->wall>=p->render_wall?p->wall-p->render_wall:0)/60e6,p->maximum/1e6);
        }
        memset(profiles,0,sizeof(profiles));profile_frames=0;profile_untracked=0;
    }
    unlock(&submit);
}
static void lock(pthread_mutex_t *mutex) {if(pthread_mutex_lock(mutex))abort();}
static void unlock(pthread_mutex_t *mutex) {if(pthread_mutex_unlock(mutex))abort();}
static void rpc_wait(void) {if(pthread_cond_wait(&changed,&queue))abort();}
static void wake(void) {if(pthread_cond_broadcast(&changed))abort();}
int wumpa_gl_rpc_active(void) {return atomic_load_explicit(&active,memory_order_acquire);}
int wumpa_gl_rpc_owner(void) {return owner;}
static void *run(void *unused)
{
    (void)unused;owner=1;
    int attached=SDL_GL_MakeCurrent(rpc_window,rpc_context)>=0;
    lock(&queue);success=attached;ready=1;wake();
    while(attached && !stopping) {
        while(!pending && !stopping)rpc_wait();
        if(stopping)break;
        void (*function)(void *)=pending;void *argument=payload;
        unlock(&queue);
        uint64_t start_wall=profiling?rpc_clock(CLOCK_MONOTONIC):0;
        uint64_t start_cpu=profiling?rpc_clock(CLOCK_THREAD_CPUTIME_ID):0;
        function(argument);
        uint64_t used_cpu=profiling?rpc_clock(CLOCK_THREAD_CPUTIME_ID)-start_cpu:0;
        uint64_t used_wall=profiling?rpc_clock(CLOCK_MONOTONIC)-start_wall:0;
        lock(&queue);execution_cpu=used_cpu;execution_wall=used_wall;pending=NULL;done=1;++calls;wake();
    }
    unlock(&queue);
    if(attached && SDL_GL_MakeCurrent(rpc_window,NULL)<0)abort();
    owner=0;return NULL;
}
void wumpa_gl_rpc_call_named(const char *name,void (*function)(void *),void *argument)
{
    if(owner || !wumpa_gl_rpc_active()) {function(argument);return;}
    lock(&submit);
    if(!wumpa_gl_rpc_active()) {unlock(&submit);function(argument);return;}
    uint64_t start_wall=profiling?rpc_clock(CLOCK_MONOTONIC):0;
    uint64_t start_cpu=profiling?rpc_clock(CLOCK_THREAD_CPUTIME_ID):0;
    lock(&queue);done=0;payload=argument;pending=function;wake();
    while(!done)rpc_wait();
    if(profiling) {
        uint64_t cpu=rpc_clock(CLOCK_THREAD_CPUTIME_ID)-start_cpu;
        uint64_t wall=rpc_clock(CLOCK_MONOTONIC)-start_wall;
        profile_add(name?name:"unnamed",wall,cpu);
    }
    unlock(&queue);unlock(&submit);
}
void wumpa_gl_rpc_call(void (*function)(void *),void *argument)
{wumpa_gl_rpc_call_named("unnamed",function,argument);}
struct Bind {SDL_GLContext context;int result;};
static void bind(void *argument)
{
    struct Bind *request=argument;
    /* Android_GLES_MakeCurrent does not reacquire Android_ActivityMutex. */
    request->result=SDL_GL_MakeCurrent(rpc_window,request->context);
}
static int backup(void *unused)
{
    (void)unused;struct Bind request={NULL,-1};wumpa_gl_rpc_call_named("lifecycle-bind",bind,&request);
    if(request.result<0) {fprintf(stderr,"[shield rpc] context detach failed during pause\n");abort();}
    return request.result;
}
static int restore(void *unused)
{
    (void)unused;struct Bind request={rpc_context,-1};wumpa_gl_rpc_call_named("lifecycle-bind",bind,&request);
    if(request.result<0)fprintf(stderr,"[shield rpc] context restore failed; resource restoration unavailable\n");
    return request.result;
}
int wumpa_gl_rpc_start(SDL_Window *window,SDL_GLContext context)
{
    const char *value=getenv("WRATH_GL_RPC");
    if(!value || strcmp(value,"1"))return 0;
    lock(&submit);
    if(wumpa_gl_rpc_active()) {unlock(&submit);return 0;}
    rpc_window=window;rpc_context=context;ready=success=done=stopping=0;pending=NULL;calls=0;
    const char *profile=getenv("WRATH_GL_RPC_PROFILE");profiling=profile && !strcmp(profile,"1");
    memset(profiles,0,sizeof(profiles));profile_frames=0;profile_untracked=0;
    if(SDL_GL_MakeCurrent(window,NULL)<0) {unlock(&submit);return -1;}
    int created=pthread_create(&worker,NULL,run,NULL)==0;
    if(created) {
        lock(&queue);while(!ready)rpc_wait();int ok=success;unlock(&queue);
        if(ok) {
            atomic_store_explicit(&active,1,memory_order_release);
            SDL_WumpaSetGLContextCallbacks(backup,restore,NULL);
            fprintf(stderr,"[shield rpc] persistent GL owner active (synchronous borrowed arguments)\n");
            unlock(&submit);return 0;
        }
        if(pthread_join(worker,NULL))abort();
    }
    if(SDL_GL_MakeCurrent(window,context)<0)abort();
    unlock(&submit);return -1;
}
static void interval(void *argument) {*(int *)argument=SDL_GL_GetSwapInterval();}
int wumpa_gl_rpc_swap_interval(void) {int value=0;wumpa_gl_rpc_call_named("swap-interval",interval,&value);return value;}
static void swap(void *argument) {SDL_GL_SwapWindow(argument);}
int wumpa_gl_rpc_swap(SDL_Window *window) {wumpa_gl_rpc_call_named("swap",swap,window);profile_present();return 0;}
void wumpa_gl_rpc_stop(void)
{
    if(owner)abort();
    lock(&submit);
    if(!wumpa_gl_rpc_active()) {unlock(&submit);return;}
    SDL_WumpaSetGLContextCallbacks(NULL,NULL,NULL);
    lock(&queue);stopping=1;wake();unlock(&queue);
    if(pthread_join(worker,NULL))abort();
    atomic_store_explicit(&active,0,memory_order_release);
    /* Teardown caller owns graphics serialization and may now destroy SDL. */
    if(SDL_GL_MakeCurrent(rpc_window,rpc_context)<0)abort();
    fprintf(stderr,"[shield rpc] stopped after %llu synchronous submissions\n",calls);
    unlock(&submit);
}
