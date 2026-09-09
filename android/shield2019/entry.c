#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include "shield_host.h"
extern int wrath_game_main(int argc, char **argv);
int SDL_main(int argc, char **argv)
{
    if(!wumpa_host_start("native.log"))return 1;
    const char *storage=SDL_AndroidGetExternalStoragePath();
    char assets[4096];
    int n=snprintf(assets,sizeof(assets),"%s/assets",storage);
    if(n<0 || (size_t)n>=sizeof(assets)) return 1;
    char *game_argv[]={"WumpaForge",assets,NULL};
    (void)argc; (void)argv;
    return wrath_game_main(2,game_argv);
}
