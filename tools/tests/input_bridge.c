#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "../../src/input_bridge.c"
_Thread_local uint32_t g_eax, g_esp, g_fs_base;
ptrdiff_t g_xbox_mem_offset;
size_t g_xbox_map_size = 0x01000000, g_xbox_total_ram = 0x01000000;
static uint32_t event_received;
static Uint16 low_motor, high_motor;
static int rumble_success;
static int rumble(void *data, Uint16 low, Uint16 high)
{
    (void)data;low_motor=low;high_motor=high;return rumble_success;
}
static void event_bridge(void)
{
    assert(read32(g_esp + 8) == 0);
    event_received = read32(g_esp + 4);
    g_esp += 12;
    g_eax = 0;
}
recomp_func_t recomp_lookup_kernel(uint32_t address)
{
    return address == 0xFE000038 ? event_bridge : NULL;
}
static uint32_t call_guest(uint32_t address, unsigned n, ...)
{
    g_esp = 0x20000;
    write32(g_esp, 0x11223344);
    va_list ap;va_start(ap,n);
    for (unsigned i=0;i<n;++i) write32(g_esp+4+i*4, va_arg(ap,uint32_t));
    va_end(ap);
    recomp_func_t f = wrath_input_lookup(address);assert(f);f();
    assert(g_esp == 0x20000+4+n*4);
    return g_eax;
}
static SDL_Joystick *attach(void)
{
    SDL_VirtualJoystickDesc desc={0};
    desc.version=SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
    desc.type=SDL_JOYSTICK_TYPE_GAMECONTROLLER;
    desc.naxes=SDL_CONTROLLER_AXIS_MAX;
    desc.nbuttons=SDL_CONTROLLER_BUTTON_MAX;
    desc.vendor_id=0x054c;desc.product_id=0x0ce6;
    desc.name="DualSense bridge validation";
    desc.axis_mask=(1u<<SDL_CONTROLLER_AXIS_MAX)-1;
    desc.button_mask=(1u<<SDL_CONTROLLER_BUTTON_MAX)-1;
    desc.Rumble=rumble;
    int index=SDL_JoystickAttachVirtualEx(&desc);assert(index>=0);
    SDL_Joystick *joy=SDL_JoystickOpen(index);assert(joy);
    SDL_JoystickSetVirtualAxis(joy,SDL_CONTROLLER_AXIS_TRIGGERLEFT,-32768);
    SDL_JoystickSetVirtualAxis(joy,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,-32768);
    return joy;
}
int main(void)
{
    assert(SDL_Init(SDL_INIT_GAMECONTROLLER)==0);
    if (SDL_NumJoysticks()) {puts("SKIP: physical device present");return 77;}
    SDL_version version;SDL_GetVersion(&version);
    rumble_success=(version.major==2&&version.minor==32&&version.patch==70)?1:0;
    void *ram=calloc(1,g_xbox_map_size);assert(ram);g_xbox_mem_offset=(ptrdiff_t)ram;
    g_fs_base=0x1000;write32(g_fs_base+4,0x10114);write32(GUEST_TLS_INDEX,(uint32_t)-5);
    write32(0x10100,0x10200);write32(GUEST_NT_SET_EVENT,0xFE000038);
    assert(call_guest(0x154AA8,2,0u,0u)==0);
    assert(call_guest(0x154AAD,3,GUEST_GAMEPAD_TYPE,0x21000u,0x21004u)==0);
    assert(call_guest(0x154249,4,GUEST_GAMEPAD_TYPE,0u,0u,0u)==0);
    assert(read32(0x10204)==INPUT_DISCONNECTED);
    SDL_Joystick *joy=attach();
    assert(call_guest(0x154AAD,3,GUEST_GAMEPAD_TYPE,0x21000u,0x21004u)==1);
    assert(read32(0x21000)==1&&read32(0x21004)==0);
    assert(call_guest(0x154AAD,3,GUEST_GAMEPAD_TYPE,0x21000u,0x21004u)==0);
    assert(call_guest(0x154AAD,3,GUEST_MU_TYPE,0x21000u,0x21004u)==0);
    uint32_t token=call_guest(0x154249,4,GUEST_GAMEPAD_TYPE,0u,0u,0u);assert(token);
    SDL_JoystickSetVirtualButton(joy,SDL_CONTROLLER_BUTTON_A,1);
    SDL_JoystickSetVirtualButton(joy,SDL_CONTROLLER_BUTTON_START,1);
    SDL_JoystickSetVirtualAxis(joy,SDL_CONTROLLER_AXIS_LEFTX,12345);
    memset(guest_ptr(0x22000),0xA5,24);
    assert(call_guest(0x1542AB,2,token,0x22000u)==0);
    assert(read16(0x22004)&XBOX_GAMEPAD_START);
    assert(*(uint8_t*)guest_ptr(0x22006)==255);
    assert(read16(0x2200E)==12345);
    assert(read16(0x22016)==0xA5A5); /* No host padding copied into guest. */
    memset(guest_ptr(0x23000),0,70);write16(0x23042,1234);write16(0x23044,5678);
    write32(0x23004,0xE0000042);
    assert(call_guest(0x15431A,2,token,0x23000u)==0);
    assert(read32(0x23000)==0&&read16(0x23040)==0x0600);
    assert(low_motor==1234&&high_motor==5678&&event_received==0xE0000042);
    assert(call_guest(0x15429F,1,token)==0&&low_motor==0&&high_motor==0);
    assert(call_guest(0x1542AB,2,token,0x22000u)==INPUT_DISCONNECTED);
    uint32_t next=call_guest(0x154249,4,GUEST_GAMEPAD_TYPE,0u,0u,0u);
    assert(next&&next!=token);
    assert(SDL_JoystickDetachVirtual(0)==0);SDL_JoystickClose(joy);
    assert(call_guest(0x1542AB,2,next,0x22000u)==INPUT_DISCONNECTED);
    assert(call_guest(0x154AAD,3,GUEST_GAMEPAD_TYPE,0x21000u,0x21004u)==1);
    assert(read32(0x21000)==0&&read32(0x21004)==1);
    joy=attach();
    assert(call_guest(0x154AAD,3,GUEST_GAMEPAD_TYPE,0x21000u,0x21004u)==1);
    assert(read32(0x21000)==1&&read32(0x21004)==0);
    assert(call_guest(0x1542AB,2,next,0x22000u)==INPUT_DISCONNECTED);
    assert(!wrath_input_lookup(0x153D6D)); /* Rejected ambiguous Close signature. */
    SDL_JoystickDetachVirtual(0);SDL_JoystickClose(joy);xbox_InputShutdown();SDL_Quit();free(ram);
    puts("PASS: confirmed address dispatch, stdcall stack, live enumeration, no MU, 22-byte state, 70-byte feedback, token event, rumble, close generations, disconnect/reconnect");
}
