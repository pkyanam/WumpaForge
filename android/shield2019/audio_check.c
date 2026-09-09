/* Diagnostic uses the exact production PCM sink, including its callback/ring.
 * Counters are inspected only after shutdown has joined the callback. */
#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <unistd.h>
#include "shield_host.h"
#include "apu/apu_xaudio2.c"
int SDL_main(int argc,char **argv)
{
    (void)argc;(void)argv;
    if(!wumpa_host_start("audio-check.log"))return 1;
    SDL_setenv("WRATH_TRACE_AUDIO","1",1);
    if(!xa2_init()){
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Audio check","Could not open the audio output. See audio-check.log.",NULL);return 1;
    }
    const unsigned total=96000;unsigned produced=0;Uint32 deadline=SDL_GetTicks()+5000;
    while(produced<total&&(Sint32)(SDL_GetTicks()-deadline)<0){
        SDL_PumpEvents();
        unsigned room;
        SDL_LockAudioDevice(g_sdl_device);room=SDL_PCM_CAPACITY-g_sdl_queued;SDL_UnlockAudioDevice(g_sdl_device);
        if(room<256){SDL_Delay(1);continue;}
        int16_t block[256*2];
        for(unsigned i=0;i<256;i++){
            unsigned frame=produced+i,within=frame%48000;double envelope=1.0;
            if(within<480)envelope=within/480.0;else if(within>47520)envelope=(48000-within)/480.0;
            double frequency=frame<48000?440.0:660.0;
            int16_t sample=(int16_t)(1600.0*envelope*sin(6.283185307179586*frequency*within/48000.0));
            block[2*i]=frame<48000?sample:0;block[2*i+1]=frame>=48000?sample:0;
        }
        if(!xa2_submit_samples(block,256))break;
        produced+=256;
    }
    SDL_Delay(160);xa2_shutdown();
    int passed=produced==total&&g_sdl_played==total&&g_sdl_nonzero_out>0;
    char report[512];snprintf(report,sizeof(report),"PCM callback check: %s\nSubmitted: %u frames; consumed: %llu; nonzero samples: %llu.\nYou should have heard a quiet left tone, then a right tone. Callback counts cannot confirm speaker routing or audibility.",passed?"PASS":"FAIL",produced,(unsigned long long)g_sdl_played,(unsigned long long)g_sdl_nonzero_out);
    puts(report);SDL_ShowSimpleMessageBox(passed?SDL_MESSAGEBOX_INFORMATION:SDL_MESSAGEBOX_ERROR,"Stereo audio diagnostic",report,NULL);
    SDL_Quit();fflush(NULL);_exit(passed?0:1);
}
