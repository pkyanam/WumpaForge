#include <SDL.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
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
    SDL_setenv("WRATH_STATE_ROOT",state,1);
    char mapping[4096];int mapping_n=snprintf(mapping,sizeof(mapping),"%s/gamecontrollerdb.txt",storage);
    if(mapping_n>0&&(size_t)mapping_n<sizeof(mapping)&&access(mapping,R_OK)==0)
        SDL_SetHint(SDL_HINT_GAMECONTROLLERCONFIG_FILE,mapping);
    char path[4096];int n=snprintf(path,sizeof(path),"%s/%s",storage,log_name);
    if(n<0||(size_t)n>=sizeof(path))return 0;
    if(!freopen(path,"a",stdout)||dup2(fileno(stdout),STDERR_FILENO)<0)return 0;
    setvbuf(stdout,NULL,_IONBF,0);setvbuf(stderr,NULL,_IONBF,0);
    fprintf(stderr,"WumpaForge Android ARM64 host started (%s)\n",log_name);
    return 1;
}
