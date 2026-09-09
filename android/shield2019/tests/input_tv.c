/* Actual SDL virtual-device/backend integration, no game or Bluetooth needed. */
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include "input/xinput_xbox.h"
typedef struct { Uint16 low, high; unsigned calls; } Motor;
static int callback_success;
static int rumble(void *data, Uint16 low, Uint16 high) {
    Motor *m=data; m->low=low;m->high=high;++m->calls;return callback_success;
}
static SDL_Joystick *attach(const char *name, Uint32 axes, Uint32 buttons, Motor *motor) {
    static Uint16 next_product=1;
    SDL_VirtualJoystickDesc d={0};d.vendor_id=0x7ffe;d.product_id=next_product++;d.version=SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
    d.type=SDL_JOYSTICK_TYPE_GAMECONTROLLER;d.naxes=axes?SDL_CONTROLLER_AXIS_MAX:0;
    d.nbuttons=SDL_CONTROLLER_BUTTON_MAX;d.name=name;d.axis_mask=axes;d.button_mask=buttons;
    d.userdata=motor;d.Rumble=rumble;
    int index=SDL_JoystickAttachVirtualEx(&d);assert(index>=0&&SDL_IsGameController(index));
    SDL_Joystick *joy=SDL_JoystickOpen(index);assert(joy);
    return joy;
}
static void detach(SDL_Joystick *joy) {
    SDL_JoystickID id=SDL_JoystickInstanceID(joy);int found=0;
    for(int i=0;i<SDL_NumJoysticks();++i)if(SDL_JoystickGetDeviceInstanceID(i)==id){assert(!SDL_JoystickDetachVirtual(i));found=1;break;}
    assert(found);SDL_JoystickClose(joy);
}
static XBOX_INPUT_STATE state(unsigned slot) {XBOX_INPUT_STATE s;assert(!xbox_InputGetState(slot,&s));return s;}
#define BUTTON(x) (1u << SDL_CONTROLLER_BUTTON_##x)
int main(void) {
    SDL_version v;SDL_GetVersion(&v);callback_success=v.major==2&&v.minor==32&&v.patch==70;
    SDL_setenv("WRATH_KEYBOARD","0",1);
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI,"0");SDL_SetHint(SDL_HINT_JOYSTICK_MFI,"0");
    SDL_SetHintWithPriority(SDL_HINT_JOYSTICK_HIDAPI_PS5,"0",SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_JOYSTICK_HIDAPI_XBOX,"0",SDL_HINT_OVERRIDE);
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");xbox_InputInit();
    if(SDL_NumJoysticks()){puts("SKIP: physical controllers present");xbox_InputShutdown();return 77;}
    Motor remote_motor={0},first_motor={0},second_motor={0},digital_motor={0};
    Uint32 remote_buttons=BUTTON(A)|BUTTON(B)|BUTTON(DPAD_LEFT)|BUTTON(DPAD_RIGHT);
    SDL_Joystick *remote=attach("Synthetic TV remote",0,remote_buttons,&remote_motor);
    assert(!SDL_JoystickSetVirtualButton(remote,SDL_CONTROLLER_BUTTON_A,1));
    XBOX_INPUT_STATE before=state(0);assert(before.Gamepad.bAnalogButtons[XBOX_BUTTON_A]==255);
    XBOX_VIBRATION vibration={1234,5678};assert(!xbox_InputSetState(0,&vibration));assert(remote_motor.low==1234);
    Uint32 stick=(1u<<SDL_CONTROLLER_AXIS_LEFTX)|(1u<<SDL_CONTROLLER_AXIS_LEFTY);
    SDL_Joystick *first=attach("Generic analog gamepad",stick,BUTTON(X),&first_motor);
    assert(!SDL_JoystickSetVirtualAxis(first,SDL_CONTROLLER_AXIS_LEFTX,16000));
    XBOX_INPUT_STATE promoted=state(0);
    assert(promoted.Gamepad.sThumbLX==16000);assert(promoted.Gamepad.bAnalogButtons[XBOX_BUTTON_A]==0);
    assert(promoted.dwPacketNumber>before.dwPacketNumber);assert(state(0).dwPacketNumber==promoted.dwPacketNumber);
    assert(remote_motor.low==0&&remote_motor.high==0);assert(state(1).Gamepad.bAnalogButtons[XBOX_BUTTON_A]==255);
    assert(!xbox_InputSetState(0,&vibration));assert(first_motor.low==1234&&remote_motor.low==0);
    SDL_Joystick *second=attach("Second generic gamepad",stick,BUTTON(Y),&second_motor);
    assert(!SDL_JoystickSetVirtualAxis(second,SDL_CONTROLLER_AXIS_LEFTX,-12000));
    assert(state(0).Gamepad.sThumbLX==16000);assert(state(2).Gamepad.sThumbLX==-12000);
    /* Removing the primary promotes the next real pad over the earlier remote. */
    detach(first);assert(state(0).Gamepad.sThumbLX==-12000);assert(state(1).Gamepad.bAnalogButtons[XBOX_BUTTON_A]==255);
    assert(!xbox_InputSetState(0,&vibration));assert(second_motor.low==1234&&remote_motor.low==0);
    detach(second);state(1); /* Remote remains usable without a primary gamepad. */
    detach(remote);assert(!xbox_InputIsConnected(0));assert(!xbox_InputIsConnected(1));
    remote=attach("Another TV remote",0,remote_buttons,&remote_motor);state(0);
    SDL_Joystick *digital=attach("Accessible digital gamepad",0,BUTTON(A)|BUTTON(B)|BUTTON(START),&digital_motor);
    assert(!SDL_JoystickSetVirtualButton(digital,2,1)); /* Compact virtual A/B/Start mapping. */
    assert(state(0).Gamepad.wButtons&XBOX_GAMEPAD_START);assert(state(1).Gamepad.wButtons==0);
    detach(digital);detach(remote);xbox_InputShutdown();
    puts("PASS: remote/gamepad promotion, generic analog/digital capability, stable real pads, packets, input release, rumble and reconnect");return 0;
}
