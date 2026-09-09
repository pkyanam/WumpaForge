#include "gl_rpc.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
        function(argument);
        lock(&queue);pending=NULL;done=1;++calls;wake();
    }
    unlock(&queue);
    if(attached && SDL_GL_MakeCurrent(rpc_window,NULL)<0)abort();
    owner=0;return NULL;
}
void wumpa_gl_rpc_call(void (*function)(void *),void *argument)
{
    if(owner || !wumpa_gl_rpc_active()) {function(argument);return;}
    lock(&submit);
    if(!wumpa_gl_rpc_active()) {unlock(&submit);function(argument);return;}
    lock(&queue);done=0;payload=argument;pending=function;wake();
    while(!done)rpc_wait();
    unlock(&queue);unlock(&submit);
}
struct Bind {SDL_GLContext context;int result;};
static void bind(void *argument)
{
    struct Bind *request=argument;
    /* Android_GLES_MakeCurrent does not reacquire Android_ActivityMutex. */
    request->result=SDL_GL_MakeCurrent(rpc_window,request->context);
}
static int backup(void *unused)
{
    (void)unused;struct Bind request={NULL,-1};wumpa_gl_rpc_call(bind,&request);
    if(request.result<0) {fprintf(stderr,"[shield rpc] context detach failed during pause\n");abort();}
    return request.result;
}
static int restore(void *unused)
{
    (void)unused;struct Bind request={rpc_context,-1};wumpa_gl_rpc_call(bind,&request);
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
int wumpa_gl_rpc_swap_interval(void) {int value=0;wumpa_gl_rpc_call(interval,&value);return value;}
static void swap(void *argument) {SDL_GL_SwapWindow(argument);}
int wumpa_gl_rpc_swap(SDL_Window *window) {wumpa_gl_rpc_call(swap,window);return 0;}
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
