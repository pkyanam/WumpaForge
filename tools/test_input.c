/* SDL virtual-device integration test: no physical controllers required. */
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include "input/xinput_xbox.h"

static Uint16 rumble_low, rumble_high;
static int virtual_rumble_success;
static int rumble(void *data, Uint16 low, Uint16 high)
{
    (void)data;
    rumble_low = low;
    rumble_high = high;
    return virtual_rumble_success;
}

static SDL_Joystick *attach(const char *name, Uint16 vendor, Uint16 product)
{
    SDL_VirtualJoystickDesc desc = {0};
    desc.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
    desc.type = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
    desc.naxes = SDL_CONTROLLER_AXIS_MAX;
    desc.nbuttons = SDL_CONTROLLER_BUTTON_MAX;
    desc.vendor_id = vendor;
    desc.product_id = product;
    desc.name = name;
    desc.axis_mask = (1u << SDL_CONTROLLER_AXIS_MAX) - 1;
    desc.button_mask = (1u << SDL_CONTROLLER_BUTTON_MAX) - 1;
    desc.Rumble = rumble;
    int index = SDL_JoystickAttachVirtualEx(&desc);
    assert(index >= 0 && SDL_IsGameController(index));
    SDL_Joystick *joy = SDL_JoystickOpen(index);
    assert(joy);
    assert(SDL_JoystickSetVirtualAxis(joy, SDL_CONTROLLER_AXIS_TRIGGERLEFT, -32768) == 0);
    assert(SDL_JoystickSetVirtualAxis(joy, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, -32768) == 0);
    return joy;
}

static void detach(SDL_Joystick *joy)
{
    SDL_JoystickID id = SDL_JoystickInstanceID(joy);
    int found = 0;
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_JoystickGetDeviceInstanceID(i) == id) {
            assert(SDL_JoystickDetachVirtual(i) == 0);
            found = 1;
            break;
        }
    }
    assert(found);
    SDL_JoystickClose(joy);
}

int main(void)
{
    SDL_setenv("WRATH_KEYBOARD", "0", 1); /* Isolate physical-port semantics. */
    SDL_version version;
    SDL_GetVersion(&version);
    /* sdl2-compat 2.32.70 forwards an SDL2 int callback directly to an SDL3
     * bool callback. Adapt only this fake driver, never runtime success checks.
     * https://github.com/libsdl-org/sdl2-compat/blob/release-2.32.70/src/sdl2_compat.c#L10350 */
    if (version.major == 2 && version.minor == 32 && version.patch == 70) {
        virtual_rumble_success = 1;
        puts("Using sdl2-compat 2.32.70 virtual rumble callback workaround");
    }
    /* Isolate tests from any Bluetooth devices already paired to the host. */
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "0");
    SDL_SetHintWithPriority(SDL_HINT_JOYSTICK_HIDAPI_PS5, "0", SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_JOYSTICK_HIDAPI_XBOX, "0", SDL_HINT_OVERRIDE);
    SDL_SetHint(SDL_HINT_JOYSTICK_MFI, "0");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    xbox_InputInit();
    if (SDL_NumJoysticks() != 0) {
        puts("SKIP: disconnect physical controllers before running virtual test");
        xbox_InputShutdown();
        return 77;
    }
    assert(!xbox_InputIsConnected(0));
    SDL_Joystick *xbox = attach("Virtual Xbox Series controller", 0x045e, 0x0b13);
    SDL_Joystick *ps5 = attach("Virtual DualSense controller", 0x054c, 0x0ce6);
    assert(xbox_InputIsConnected(0));
    assert(xbox_InputIsConnected(1));
    assert(!xbox_InputIsConnected(2));
    XBOX_INPUT_STATE state, same;
    assert(xbox_InputGetState(0, &state) == 0);
    assert(state.Gamepad.sThumbLY == 0 && state.Gamepad.sThumbRY == 0);
    assert(state.Gamepad.bAnalogButtons[XBOX_BUTTON_LTRIGGER] == 0);
    assert(xbox_InputGetState(0, &same) == 0);
    assert(state.dwPacketNumber == same.dwPacketNumber);

    assert(SDL_JoystickSetVirtualButton(xbox, SDL_CONTROLLER_BUTTON_A, 1) == 0);
    assert(SDL_JoystickSetVirtualButton(xbox, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, 1) == 0);
    assert(SDL_JoystickSetVirtualButton(xbox, SDL_CONTROLLER_BUTTON_START, 1) == 0);
    assert(SDL_JoystickSetVirtualAxis(xbox, SDL_CONTROLLER_AXIS_LEFTY, -32768) == 0);
    assert(SDL_JoystickSetVirtualAxis(xbox, SDL_CONTROLLER_AXIS_RIGHTY, 32767) == 0);
    assert(SDL_JoystickSetVirtualAxis(xbox, SDL_CONTROLLER_AXIS_TRIGGERLEFT, 32767) == 0);
    assert(xbox_InputGetState(0, &same) == 0);
    assert(same.dwPacketNumber != state.dwPacketNumber);
    assert(same.Gamepad.bAnalogButtons[XBOX_BUTTON_A] == 255);
    assert(same.Gamepad.bAnalogButtons[XBOX_BUTTON_BLACK] == 255);
    assert(same.Gamepad.bAnalogButtons[XBOX_BUTTON_WHITE] == 0);
    assert(same.Gamepad.bAnalogButtons[XBOX_BUTTON_LTRIGGER] == 255);
    assert(same.Gamepad.sThumbLY == 32767 && same.Gamepad.sThumbRY == -32767);
    assert(same.Gamepad.wButtons == XBOX_GAMEPAD_START);

    assert(SDL_JoystickSetVirtualButton(ps5, SDL_CONTROLLER_BUTTON_X, 1) == 0);
    assert(SDL_JoystickSetVirtualButton(ps5, SDL_CONTROLLER_BUTTON_LEFTSHOULDER, 1) == 0);
    assert(xbox_InputGetState(1, &state) == 0);
    assert(state.Gamepad.bAnalogButtons[XBOX_BUTTON_X] == 255);
    assert(state.Gamepad.bAnalogButtons[XBOX_BUTTON_WHITE] == 255);
    /* Exercise every standardized original button separately on both families. */
    const SDL_GameControllerButton analog[] = {
        SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_B, SDL_CONTROLLER_BUTTON_X,
        SDL_CONTROLLER_BUTTON_Y, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
        SDL_CONTROLLER_BUTTON_LEFTSHOULDER};
    const SDL_GameControllerButton digital[] = {
        SDL_CONTROLLER_BUTTON_DPAD_UP, SDL_CONTROLLER_BUTTON_DPAD_DOWN,
        SDL_CONTROLLER_BUTTON_DPAD_LEFT, SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
        SDL_CONTROLLER_BUTTON_START, SDL_CONTROLLER_BUTTON_BACK,
        SDL_CONTROLLER_BUTTON_LEFTSTICK, SDL_CONTROLLER_BUTTON_RIGHTSTICK};
    SDL_Joystick *devices[] = {xbox, ps5};
    for (unsigned port = 0; port < 2; ++port) {
        for (int b = 0; b < SDL_CONTROLLER_BUTTON_MAX; ++b)
            SDL_JoystickSetVirtualButton(devices[port], b, 0);
        for (unsigned b = 0; b < 6; ++b) {
            SDL_JoystickSetVirtualButton(devices[port], analog[b], 1);
            assert(xbox_InputGetState(port, &state) == 0);
            for (unsigned other = 0; other < 6; ++other)
                assert(state.Gamepad.bAnalogButtons[other] == (other == b ? 255 : 0));
            SDL_JoystickSetVirtualButton(devices[port], analog[b], 0);
        }
        for (unsigned b = 0; b < 8; ++b) {
            SDL_JoystickSetVirtualButton(devices[port], digital[b], 1);
            assert(xbox_InputGetState(port, &state) == 0);
            assert(state.Gamepad.wButtons == (1u << b));
            SDL_JoystickSetVirtualButton(devices[port], digital[b], 0);
        }
        SDL_JoystickSetVirtualAxis(devices[port], SDL_CONTROLLER_AXIS_LEFTX, -32768);
        SDL_JoystickSetVirtualAxis(devices[port], SDL_CONTROLLER_AXIS_RIGHTX, 32767);
        SDL_JoystickSetVirtualAxis(devices[port], SDL_CONTROLLER_AXIS_TRIGGERRIGHT, 0);
        assert(xbox_InputGetState(port, &state) == 0);
        assert(state.Gamepad.sThumbLX == -32768 && state.Gamepad.sThumbRX == 32767);
        assert(state.Gamepad.bAnalogButtons[XBOX_BUTTON_RTRIGGER] >= 126 &&
               state.Gamepad.bAnalogButtons[XBOX_BUTTON_RTRIGGER] <= 128);
    }
    SDL_JoystickSetVirtualButton(ps5, SDL_CONTROLLER_BUTTON_X, 1);
    XBOX_INPUT_CAPABILITIES caps;
    assert(xbox_InputGetCapabilities(1, 0, &caps) == 0);
    assert(caps.Vibration.wLeftMotorSpeed == 65535);
    XBOX_VIBRATION vibration = {12345, 54321};
    DWORD rumble_result = xbox_InputSetState(1, &vibration);
    if (rumble_result) fprintf(stderr, "rumble result %u: %s\n", rumble_result, SDL_GetError());
    assert(rumble_result == 0);
    assert(rumble_low == 12345 && rumble_high == 54321);

    detach(xbox);
    assert(!xbox_InputIsConnected(0));
    assert(xbox_InputIsConnected(1));
    assert(xbox_InputGetState(0, &same) == ERROR_DEVICE_NOT_CONNECTED);
    assert(xbox_InputGetCapabilities(0, 0, &caps) == ERROR_DEVICE_NOT_CONNECTED);
    assert(xbox_InputSetState(0, &vibration) == ERROR_DEVICE_NOT_CONNECTED);
    assert(xbox_InputGetState(1, &same) == 0);
    assert(same.Gamepad.bAnalogButtons[XBOX_BUTTON_X] == 255);
    xbox = attach("Reconnected Xbox controller", 0x045e, 0x0b13);
    assert(xbox_InputIsConnected(0));
    assert(!xbox_InputIsConnected(2)); /* No duplicate of the surviving device. */
    assert(xbox_InputGetState(0, &same) == 0);
    assert(same.Gamepad.bAnalogButtons[XBOX_BUTTON_A] == 0);
    assert(xbox_InputGetState(4, &state) == ERROR_DEVICE_NOT_CONNECTED);
    assert(xbox_InputGetState(0, NULL) == ERROR_DEVICE_NOT_CONNECTED);
    vibration.wLeftMotorSpeed = vibration.wRightMotorSpeed = 0;
    assert(xbox_InputSetState(1, &vibration) == 0);
    assert(rumble_low == 0 && rumble_high == 0);
    detach(ps5);
    detach(xbox);
    xbox_InputShutdown();
    xbox_InputInit();
    assert(!xbox_InputIsConnected(0));
    xbox_InputShutdown();
    SDL_Quit();
    puts("PASS: SDL mappings, axes, packets, hotplug, stable ports, rumble, lifecycle");
    return 0;
}
