#include <SDL.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include "shield_host.h"
static pthread_t game_thread;
static int thread_ready;
int wumpa_is_game_thread(void) { return thread_ready && pthread_equal(pthread_self(),game_thread); }
extern int wrath_game_main(int argc, char **argv);
int SDL_main(int argc, char **argv)
{
    game_thread=pthread_self(); thread_ready=1;
    /* SDL's EGL path resolves desktop functions through eglGetProcAddress.
       libEGL is a public Android NDK library; no private vendor path is used. */
    SDL_setenv("SDL_VIDEO_GL_DRIVER","libEGL.so",1);
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"0");
    const char *storage=SDL_AndroidGetExternalStoragePath();
    if(!storage) { SDL_Log("No app-specific external storage: %s",SDL_GetError()); return 1; }
    const char *state=SDL_AndroidGetInternalStoragePath();
    if(!state) return 1;
    SDL_setenv("WRATH_STATE_ROOT",state,1);
    char log_path[4096];
    int log_n=snprintf(log_path,sizeof(log_path),"%s/native.log",storage);
    if(log_n<0 || (size_t)log_n>=sizeof(log_path)) return 1;
    if(!freopen(log_path,"a",stdout) || dup2(fileno(stdout),STDERR_FILENO)<0) return 1;
    char assets[4096];
    int n=snprintf(assets,sizeof(assets),"%s/assets",storage);
    if(n<0 || (size_t)n>=sizeof(assets)) return 1;
    char *game_argv[]={"WumpaForge",assets,NULL};
    (void)argc; (void)argv;
    return wrath_game_main(2,game_argv);
}
