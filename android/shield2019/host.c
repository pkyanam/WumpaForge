#include <SDL.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include "shield_host.h"
static pthread_t game_thread;
static int thread_ready;
int wumpa_is_game_thread(void) { return thread_ready && pthread_equal(pthread_self(),game_thread); }
int wumpa_host_start(const char *log_name)
{
    game_thread=pthread_self();thread_ready=1;
    /* Resolve desktop GL through the public EGL loader, never a vendor path. */
    SDL_setenv("SDL_VIDEO_GL_DRIVER","libEGL.so",1);
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"0");
    const char *storage=SDL_AndroidGetExternalStoragePath();
    const char *state=SDL_AndroidGetInternalStoragePath();
    if(!storage||!state) return 0;
    char profile[4096];
    int profile_n=snprintf(profile,sizeof(profile),"%s/profile",storage);
    if(profile_n>0 && (size_t)profile_n<sizeof(profile) && access(profile,F_OK)==0)
        SDL_setenv("WRATH_PROFILE","1",1);
    profile_n=snprintf(profile,sizeof(profile),"%s/profile-context",storage);
    if(profile_n>0 && (size_t)profile_n<sizeof(profile) && access(profile,F_OK)==0){
        SDL_setenv("WRATH_PROFILE","1",1);
        SDL_setenv("WRATH_PROFILE_CONTEXT","1",1);
        SDL_setenv("WRATH_TRACE_INPUT","1",1);
    }
    profile_n=snprintf(profile,sizeof(profile),"%s/no-release-flush",storage);
    if(profile_n>0 && (size_t)profile_n<sizeof(profile) && access(profile,F_OK)==0)
        SDL_setenv("WRATH_EGL_NO_RELEASE_FLUSH","1",1);
    profile_n=snprintf(profile,sizeof(profile),"%s/lazy-bind",storage);
    if(profile_n>0 && (size_t)profile_n<sizeof(profile) && access(profile,F_OK)==0)
        SDL_setenv("WRATH_EGL_LAZY_BIND","1",1);
    profile_n=snprintf(profile,sizeof(profile),"%s/defer-state",storage);
    if(profile_n>0 && (size_t)profile_n<sizeof(profile) && access(profile,F_OK)==0)
        SDL_setenv("WRATH_EGL_DEFER_STATE","1",1);
    profile_n=snprintf(profile,sizeof(profile),"%s/trace-loading-cs",storage);
    if(profile_n>0 && (size_t)profile_n<sizeof(profile) && access(profile,F_OK)==0)
        SDL_setenv("WRATH_TRACE_LOADING_CS","1",1);
    SDL_setenv("WRATH_STATE_ROOT",state,1);
    profile_n=snprintf(profile,sizeof(profile),"%s/shader-cache",storage);
    if(profile_n>0 && (size_t)profile_n<sizeof(profile) && access(profile,F_OK)==0){
        char cache[4096];
        int count=snprintf(cache,sizeof(cache),"%s/shader-cache",state);
        if(count>0 && (size_t)count<sizeof(cache)){
            mkdir(cache,0700);
            SDL_setenv("WRATH_SHADER_CACHE_ROOT",cache,1);
        }
    }
    char mapping[4096];int mapping_n=snprintf(mapping,sizeof(mapping),"%s/gamecontrollerdb.txt",storage);
    if(mapping_n>0&&(size_t)mapping_n<sizeof(mapping)&&access(mapping,R_OK)==0)
        SDL_SetHint(SDL_HINT_GAMECONTROLLERCONFIG_FILE,mapping);
    char path[4096];int n=snprintf(path,sizeof(path),"%s/%s",storage,log_name);
    if(n<0||(size_t)n>=sizeof(path))return 0;
    if(!freopen(path,"a",stdout)||dup2(fileno(stdout),STDERR_FILENO)<0)return 0;
    setvbuf(stdout,NULL,_IONBF,0);setvbuf(stderr,NULL,_IONBF,0);
    fprintf(stderr,"WumpaForge Android ARM64 host started (%s) pid=%ld\n",log_name,(long)getpid());
    return 1;
}

extern int xbox_D3D8GLAcquire(void);
extern int xbox_D3D8GLEnsureCurrent(void);
extern void wumpa_gl_flush_state(void);
extern void xbox_D3D8GLRelease(void);
void wumpa_pump_input_events(void)
{
    if(!wumpa_is_game_thread()){
        fprintf(stderr,"[shield] Input event pump called outside SDL game thread.\n");
        abort();
    }
    /* Android's pump may block through pause/resume. Keep loading workers from
       binding/swapping while SDL backs up or restores the window surface. */
    if(!xbox_D3D8GLAcquire())abort();
    if(!xbox_D3D8GLEnsureCurrent())abort();
    wumpa_gl_flush_state();
    SDL_PumpEvents();
    if(SDL_HasEvent(SDL_RENDER_DEVICE_RESET)){
        fprintf(stderr,"[shield] EGL context lost during input pump; resource restoration is not implemented.\n");
        exit(EXIT_FAILURE);
    }
    xbox_D3D8GLRelease();
}
