#include <SDL.h>
#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "input/xinput_xbox.h"
#include "shield_host.h"
static void status(const char *text)
{
    JNIEnv *env=(JNIEnv*)SDL_AndroidGetJNIEnv();
    jobject activity=(jobject)SDL_AndroidGetActivity();
    if(!env||!activity)return;
    jclass type=(*env)->GetObjectClass(env,activity);
    jmethodID method=(*env)->GetMethodID(env,type,"showStatus","(Ljava/lang/String;)V");
    if(method){jstring value=(*env)->NewStringUTF(env,text);(*env)->CallVoidMethod(env,activity,method,value);(*env)->DeleteLocalRef(env,value);}
    if((*env)->ExceptionCheck(env)){(*env)->ExceptionDescribe(env);(*env)->ExceptionClear(env);}
    (*env)->DeleteLocalRef(env,type);(*env)->DeleteLocalRef(env,activity);
}
int SDL_main(int argc,char **argv)
{
    (void)argc;(void)argv;
    if(!wumpa_host_start("controller-check.log"))return 1;
    SDL_setenv("WRATH_KEYBOARD","0",1);
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER))return 1;
    SDL_Window *window=SDL_CreateWindow("Controller check",0,0,960,540,SDL_WINDOW_FULLSCREEN_DESKTOP);
    if(!window){SDL_Log("Window: %s",SDL_GetError());return 1;}
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    if(!renderer){SDL_Log("Renderer: %s",SDL_GetError());return 1;}
    xbox_InputInit();int quit=0;Uint32 update=0,rumble_until=0;int previous_y=0;
    while(!quit){
        SDL_Event event;
        while(SDL_PollEvent(&event))if(event.type==SDL_QUIT || (event.type==SDL_KEYDOWN&&(event.key.keysym.sym==SDLK_ESCAPE||event.key.keysym.sym==SDLK_AC_BACK)))quit=1;
        XBOX_INPUT_STATE state={0};DWORD result=xbox_InputGetState(0,&state);
        Uint32 now=SDL_GetTicks();int y=state.Gamepad.bAnalogButtons[XBOX_BUTTON_Y]!=0;
        if(y&&!previous_y){XBOX_VIBRATION vibration={16000,16000};DWORD r=xbox_InputSetState(0,&vibration);printf("Rumble request result=%u\n",r);rumble_until=now+200;}
        previous_y=y;
        if(rumble_until&&(Sint32)(now-rumble_until)>=0){XBOX_VIBRATION stop={0,0};xbox_InputSetState(0,&stop);rumble_until=0;}
        if(now-update>=100){
            char text[2048];
            snprintf(text,sizeof(text),"Controller input through the game's Xbox compatibility path\n%s — %d Android/SDL joystick(s)\nButtons: %04X   A/B/X/Y: %u/%u/%u/%u   L/R triggers: %u/%u\nLeft stick: %d, %d   Right stick: %d, %d\nPress the top face button (Y / Triangle) for a short rumble test.\nDisconnect/reconnect to test recovery. Back returns to the launcher.",
                result==0?"Port 1 connected":"No mapped gamepad on port 1",SDL_NumJoysticks(),state.Gamepad.wButtons,
                state.Gamepad.bAnalogButtons[XBOX_BUTTON_A],state.Gamepad.bAnalogButtons[XBOX_BUTTON_B],state.Gamepad.bAnalogButtons[XBOX_BUTTON_X],state.Gamepad.bAnalogButtons[XBOX_BUTTON_Y],
                state.Gamepad.bAnalogButtons[XBOX_BUTTON_LTRIGGER],state.Gamepad.bAnalogButtons[XBOX_BUTTON_RTRIGGER],state.Gamepad.sThumbLX,state.Gamepad.sThumbLY,state.Gamepad.sThumbRX,state.Gamepad.sThumbRY);
            status(text);update=now;
        }
        SDL_SetRenderDrawColor(renderer,12,27,35,255);SDL_RenderClear(renderer);
        SDL_SetRenderDrawColor(renderer,result==0?40:160,result==0?180:50,75,255);
        SDL_Rect stick={220+state.Gamepad.sThumbLX/256,360-state.Gamepad.sThumbLY/256,24,24};SDL_RenderFillRect(renderer,&stick);
        stick.x=650+state.Gamepad.sThumbRX/256;stick.y=360-state.Gamepad.sThumbRY/256;SDL_RenderFillRect(renderer,&stick);
        SDL_RenderPresent(renderer);
    }
    xbox_InputShutdown();SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
    puts("Controller diagnostic closed normally.");fflush(NULL);_exit(0);
}
