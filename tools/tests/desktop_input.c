/* Pure focus snapshots + real SDL backend logical-port lifecycle; no game run. */
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "input/desktop_input.h"

int main(void)
{
    Uint8 keys[SDL_NUM_SCANCODES] = {0};
    XboxDesktopSnapshot s = {.keys=keys, .key_count=SDL_NUM_SCANCODES};
    XBOX_GAMEPAD p = {0};
    keys[SDL_SCANCODE_W] = keys[SDL_SCANCODE_SPACE] = 1;
    s.mouse_buttons = SDL_BUTTON_LMASK | SDL_BUTTON_RMASK | SDL_BUTTON_MMASK;
    s.mouse_dx = INT_MAX; s.mouse_dy = INT_MIN;
    xbox_DesktopMerge(&p, &s);
    XBOX_GAMEPAD zero = {0};
    assert(!memcmp(&p, &zero, sizeof(p))); /* Unfocused keyboard/mouse neutral. */
    s.keyboard_focus = 1;
    xbox_DesktopMerge(&p, &s);
    assert(p.sThumbLY == 32767 && p.bAnalogButtons[XBOX_BUTTON_A] == 255);
    assert(p.bAnalogButtons[XBOX_BUTTON_X] == 0 && p.sThumbRX == 0);
    keys[SDL_SCANCODE_LGUI] = 1;
    memset(&p, 0, sizeof(p));
    xbox_DesktopMerge(&p, &s);
    assert(!memcmp(&p, &zero, sizeof(p))); /* Command+Space is not jump. */
    keys[SDL_SCANCODE_LGUI] = 0;
    s.mouse_focus = 1;
    xbox_DesktopMerge(&p, &s);
    assert(p.bAnalogButtons[XBOX_BUTTON_X] == 255 && p.bAnalogButtons[XBOX_BUTTON_B] == 255);
    assert(p.sThumbRX == 32767 && p.sThumbRY == 32767);
    memset(&p, 0, sizeof(p)); memset(keys, 0, sizeof(keys));
    s.mouse_buttons = 0;
    keys[SDL_SCANCODE_D] = keys[SDL_SCANCODE_W] = 1;
    xbox_DesktopMerge(&p, &s);
    assert(p.sThumbLX == 23170 && p.sThumbLY == 23170); /* Circular full-scale diagonal. */
    keys[SDL_SCANCODE_A] = keys[SDL_SCANCODE_S] = 1;
    p.sThumbLX = 123; p.sThumbLY = -456;
    xbox_DesktopMerge(&p, &s);
    assert(p.sThumbLX == 123 && p.sThumbLY == -456); /* Opposites preserve physical input. */
    memset(keys, 0, sizeof(keys)); memset(&p, 0, sizeof(p));
    keys[SDL_SCANCODE_RETURN] = keys[SDL_SCANCODE_BACKSPACE] = 1;
    keys[SDL_SCANCODE_UP] = keys[SDL_SCANCODE_RIGHT] = 1;
    keys[SDL_SCANCODE_C] = keys[SDL_SCANCODE_X] = keys[SDL_SCANCODE_E] = 1;
    keys[SDL_SCANCODE_Q] = keys[SDL_SCANCODE_R] = 1;
    keys[SDL_SCANCODE_LSHIFT] = keys[SDL_SCANCODE_RCTRL] = 1;
    xbox_DesktopMerge(&p, &s);
    assert(p.wButtons == (XBOX_GAMEPAD_START | XBOX_GAMEPAD_BACK |
                           XBOX_GAMEPAD_DPAD_UP | XBOX_GAMEPAD_DPAD_RIGHT));
    for (unsigned i = 1; i < 8; ++i) assert(p.bAnalogButtons[i] == 255);
    memset(&p, 0, sizeof(p)); s.keyboard_focus = 0;
    xbox_DesktopMerge(&p, &s);
    assert(!memcmp(&p, &zero, sizeof(p))); /* No stale held input after focus loss. */
    s.keyboard_focus = 1; s.key_count = 1; s.mouse_focus = 0;
    xbox_DesktopMerge(&p, &s);
    assert(!memcmp(&p, &zero, sizeof(p))); /* Bounds-check keyboard array. */

    SDL_setenv("WRATH_KEYBOARD", "1", 1);
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "0");
    SDL_SetHintWithPriority(SDL_HINT_JOYSTICK_HIDAPI_PS5, "0", SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_JOYSTICK_HIDAPI_XBOX, "0", SDL_HINT_OVERRIDE);
    SDL_SetHint(SDL_HINT_JOYSTICK_MFI, "0");
    xbox_InputInit();
    if (SDL_NumJoysticks()) { puts("SKIP: physical device present"); xbox_InputShutdown(); return 77; }
    assert(xbox_InputIsConnected(0) && !xbox_InputIsConnected(1));
    XBOX_INPUT_STATE state, next;
    assert(xbox_InputGetState(0, &state) == 0);
    assert(!memcmp(&state.Gamepad, &zero, sizeof(zero)));
    assert(xbox_InputGetState(0, &next) == 0 && next.dwPacketNumber == state.dwPacketNumber);
    XBOX_INPUT_CAPABILITIES caps;
    assert(xbox_InputGetCapabilities(0, 0, &caps) == 0);
    assert(caps.Vibration.wLeftMotorSpeed == 0 && caps.Vibration.wRightMotorSpeed == 0);
    XBOX_VIBRATION vibration = {0, 0};
    assert(xbox_InputSetState(0, &vibration) == 50); /* Keyboard has no motors. */
    /* A controller can attach/detach without replacing the desktop port. */
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    int device = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,
                                          SDL_CONTROLLER_AXIS_MAX, SDL_CONTROLLER_BUTTON_MAX, 0);
    assert(device >= 0 && SDL_IsGameController(device));
    SDL_Joystick *joystick = SDL_JoystickOpen(device); assert(joystick);
    SDL_JoystickSetVirtualAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERLEFT, -32768);
    SDL_JoystickSetVirtualAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, -32768);
    SDL_JoystickSetVirtualButton(joystick, SDL_CONTROLLER_BUTTON_A, 1);
    assert(xbox_InputGetState(0, &state) == 0);
    assert(state.Gamepad.bAnalogButtons[XBOX_BUTTON_A] == 255);
    assert(SDL_JoystickDetachVirtual(device) == 0); SDL_JoystickClose(joystick);
    assert(xbox_InputIsConnected(0) && xbox_InputGetState(0, &next) == 0);
    assert(next.dwPacketNumber != state.dwPacketNumber);
    assert(!memcmp(&next.Gamepad, &zero, sizeof(zero)));
    xbox_InputShutdown();
    SDL_setenv("WRATH_KEYBOARD", "0", 1);
    xbox_InputInit();
    assert(!xbox_InputIsConnected(0));
    xbox_InputShutdown(); SDL_Quit();
    puts("PASS: desktop focus, keys, mouse, axes, merge, neutral logical port and lifecycle");
    return 0;
}
