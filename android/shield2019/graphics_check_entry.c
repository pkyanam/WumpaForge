#include <SDL.h>
#include <stdio.h>
#include <unistd.h>
#include "shield_host.h"
extern int wumpa_graphics_checks(void);
int SDL_main(int argc,char **argv)
{
    (void)argc;(void)argv;
    if(!wumpa_host_start("graphics-check.log"))return 1;
    int result=wumpa_graphics_checks();
    printf("Graphics component checks returned %d\n",result);fflush(NULL);
    /* This Activity has a dedicated diagnostic process. Exit so driver/test
       globals cannot leak into a subsequent run or the actual game process. */
    _exit(result);
}
