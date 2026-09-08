/* Confirmed Xbox 4361 XAPI input -> native SDL controller bridge.
 * Symbol/caller evidence and guest ABI layouts: docs/INPUT-INTEGRATION.md.
 * These replacements execute on the SDL/main thread; no guest USB code runs. */
#include "input/xinput_xbox.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#ifdef __APPLE__
#include <pthread.h>
#endif

extern _Thread_local uint32_t g_eax, g_esp, g_fs_base;
extern ptrdiff_t g_xbox_mem_offset;
extern size_t g_xbox_map_size, g_xbox_total_ram;
typedef void (*recomp_func_t)(void);
extern recomp_func_t recomp_lookup_kernel(uint32_t address);

#define GUEST_GAMEPAD_TYPE 0x001534ACu
#define GUEST_MU_TYPE      0x00153430u
#define GUEST_NT_SET_EVENT 0x0015AA98u
#define GUEST_TLS_INDEX    0x00435090u
#define INPUT_HANDLES 16
#define INPUT_OK 0u
#define INPUT_BAD_HANDLE 6u
#define INPUT_NO_MEMORY 8u
#define INPUT_UNSUPPORTED 50u
#define INPUT_BAD_ARGUMENT 87u
#define INPUT_DISCONNECTED 1167u

/* Serialize guest fields individually. Native XBOX_INPUT_STATE is padded to
 * 24 bytes; the Xbox code copies a 4-byte packet and exactly 18 gamepad bytes.
 * Feedback is packed: 66-byte header, then left/right WORDs at offsets 66/68.
 * No native pointer or HANDLE is ever copied into guest memory. */
typedef struct InputHandle {
    uint32_t token;
    uint8_t port, connected, auto_poll;
    XBOX_INPUT_STATE state;
} InputHandle;
static InputHandle s_handles[INPUT_HANDLES];
static uint32_t s_serial, s_connected;

static int mapped_range(uint32_t address, size_t bytes)
{
    size_t limit = g_xbox_map_size ? g_xbox_map_size : g_xbox_total_ram;
    /* The runtime TIB is at 0x1000, below the XBE image at 0x10000. */
    return address >= 0x1000u && address < limit && bytes <= limit - address;
}
static int guest_range(uint32_t address, size_t bytes)
{
    return address >= 0x10000u && mapped_range(address, bytes);
}
static void *guest_ptr(uint32_t address)
{
    return (void *)((uintptr_t)address + g_xbox_mem_offset);
}
static uint32_t read32(uint32_t address)
{
    uint32_t value;
    memcpy(&value, guest_ptr(address), 4);
    return value;
}
static void write32(uint32_t address, uint32_t value)
{
    memcpy(guest_ptr(address), &value, 4);
}
static uint16_t read16(uint32_t address)
{
    uint16_t value;
    memcpy(&value, guest_ptr(address), 2);
    return value;
}
static void write16(uint32_t address, uint16_t value)
{
    memcpy(guest_ptr(address), &value, 2);
}
static uint32_t arg(unsigned index) { return read32(g_esp + 4 + index * 4); }
static void finish(unsigned arguments, uint32_t result)
{
    g_eax = result;
    g_esp += 4 + arguments; /* Return address and confirmed stdcall arguments. */
}
static int main_thread(void)
{
#ifdef __APPLE__
    if (!pthread_main_np()) {
        static int warned;
        if (!warned++) fprintf(stderr, "[wrath input] Input call outside SDL/main thread\n");
        return 0;
    }
#endif
    return 1;
}

/* Confirmed SetLastError code at 0xEDD33 stores through FS:[4]'s TLS array,
 * indexed by [0x435090], at block+4. Mirror that store when the TLS exists. */
static void last_error(uint32_t error)
{
    if (!mapped_range(g_fs_base, 8) || !guest_range(GUEST_TLS_INDEX, 4)) return;
    uint32_t tls = read32(g_fs_base + 4), index = read32(GUEST_TLS_INDEX);
    /* Xbox TLS lies below StackBase: the title stores a negative DWORD index
     * (this title uses -5). Match x86 32-bit address arithmetic, then validate
     * the resulting slot; don't reject negative indices as huge unsigned ones. */
    uint32_t slot = tls + index * 4u;
    if (!mapped_range(slot, 4)) return;
    uint32_t block = read32(slot);
    if (mapped_range(block, 8)) write32(block + 4, error);
}

static void refresh_devices(void)
{
    uint32_t connected = 0;
    for (unsigned port = 0; port < XBOX_MAX_CONTROLLERS; ++port)
        if (xbox_InputIsConnected(port)) connected |= 1u << port;
    uint32_t removed = s_connected & ~connected;
    for (unsigned i = 0; i < INPUT_HANDLES; ++i)
        if (s_handles[i].token && (removed & (1u << s_handles[i].port)))
            s_handles[i].connected = 0;
    s_connected = connected;
    if (guest_range(GUEST_GAMEPAD_TYPE, 12)) {
        uint32_t previous = read32(GUEST_GAMEPAD_TYPE);
        write32(GUEST_GAMEPAD_TYPE + 4,
                read32(GUEST_GAMEPAD_TYPE + 4) | (previous ^ connected));
        write32(GUEST_GAMEPAD_TYPE, connected);
    }
}

static InputHandle *handle(uint32_t token)
{
    unsigned slot = token & (INPUT_HANDLES - 1);
    return token && s_handles[slot].token == token ? &s_handles[slot] : NULL;
}
static void write_state(uint32_t output, const XBOX_INPUT_STATE *state)
{
    write32(output, state->dwPacketNumber);
    write16(output + 4, state->Gamepad.wButtons);
    memcpy(guest_ptr(output + 6), state->Gamepad.bAnalogButtons, 8);
    write16(output + 14, (uint16_t)state->Gamepad.sThumbLX);
    write16(output + 16, (uint16_t)state->Gamepad.sThumbLY);
    write16(output + 18, (uint16_t)state->Gamepad.sThumbRX);
    write16(output + 20, (uint16_t)state->Gamepad.sThumbRY);
}

static void input_init(void)
{
    uint32_t count = arg(0), types = arg(1);
    if (!main_thread()) { finish(8, INPUT_UNSUPPORTED); return; }
    if (count > 1024 || (count && !guest_range(types, (size_t)count * 8))) {
        last_error(INPUT_BAD_ARGUMENT);
        finish(8, 0);
        return;
    }
    /* Preallocation entries specify USB resource budgets; SDL owns its host
     * allocations. Devices are enumerated from real host connectivity. */
    for (unsigned i = 0; i < INPUT_HANDLES; ++i) {
        if (s_handles[i].token && s_handles[i].connected) {
            XBOX_VIBRATION stop = {0, 0};
            xbox_InputSetState(s_handles[i].port, &stop);
        }
    }
    memset(s_handles, 0, sizeof(s_handles));
    s_connected = 0;
    if (guest_range(GUEST_GAMEPAD_TYPE, 12)) memset(guest_ptr(GUEST_GAMEPAD_TYPE), 0, 12);
    if (guest_range(GUEST_MU_TYPE, 12)) memset(guest_ptr(GUEST_MU_TYPE), 0, 12);
    xbox_InputInit();
    refresh_devices();
    finish(8, 0); /* XInitDevices is void. */
}

static void input_changes(void)
{
    uint32_t type = arg(0), insertions = arg(1), removals = arg(2);
    if (!main_thread()) { finish(12, 0); return; }
    if (!guest_range(insertions, 4) || !guest_range(removals, 4)) {
        last_error(INPUT_BAD_ARGUMENT);
        finish(12, 0);
        return;
    }
    write32(insertions, 0);
    write32(removals, 0);
    if ((type != GUEST_GAMEPAD_TYPE && type != GUEST_MU_TYPE) || !guest_range(type, 12)) {
        last_error(INPUT_BAD_ARGUMENT);
        finish(12, 0);
        return;
    }
    refresh_devices();
    /* No host memory-unit device is attached: its real connected mask is zero. */
    if (type == GUEST_MU_TYPE) write32(type, 0);
    uint32_t current = read32(type), previous = read32(type + 8);
    uint32_t changed = read32(type + 4);
    uint32_t reinserted = changed & current & previous;
    uint32_t added = (current & ~previous) | reinserted;
    uint32_t removed = (previous & ~current) | reinserted;
    write32(insertions, added);
    write32(removals, removed);
    write32(type + 4, 0);
    write32(type + 8, current);
    finish(12, (added | removed) != 0);
}

static void input_open(void)
{
    uint32_t type = arg(0), port = arg(1), slot = arg(2), polling = arg(3);
    uint32_t error = INPUT_BAD_ARGUMENT;
    if (!main_thread()) { last_error(INPUT_UNSUPPORTED); finish(16, 0); return; }
    if (type != GUEST_GAMEPAD_TYPE || port >= XBOX_MAX_CONTROLLERS || slot != 0 ||
        (polling && !guest_range(polling, 4))) goto failed;
    uint8_t flags = polling ? *(const uint8_t *)guest_ptr(polling) : 1;
    if (flags & ~3u) goto failed;
    refresh_devices();
    error = INPUT_DISCONNECTED;
    if (!(s_connected & (1u << port))) goto failed;
    error = INPUT_NO_MEMORY;
    for (unsigned i = 0; i < INPUT_HANDLES; ++i) {
        InputHandle *h = &s_handles[i];
        if (h->token) continue;
        memset(h, 0, sizeof(*h));
        h->port = (uint8_t)port;
        h->connected = 1;
        h->auto_poll = flags & 1;
        error = xbox_InputGetState(port, &h->state);
        if (error != INPUT_OK) goto failed;
        /* Generation prevents closed handles from aliasing a reused slot. */
        s_serial = (s_serial + 1) & 0xFFFFFu;
        if (!s_serial) s_serial = 1;
        h->token = 0xC1000000u | (s_serial << 4) | i;
        finish(16, h->token);
        return;
    }
failed:
    last_error(error);
    finish(16, 0);
}

static void input_close(void)
{
    uint32_t token = arg(0);
    if (!main_thread()) { finish(4, 0); return; }
    refresh_devices();
    InputHandle *h = handle(token);
    if (h) {
        if (h->connected) {
            XBOX_VIBRATION stop = {0, 0};
            xbox_InputSetState(h->port, &stop);
        }
        memset(h, 0, sizeof(*h));
    }
    finish(4, 0); /* XInputClose is void. */
}

static void input_state(void)
{
    uint32_t token = arg(0), output = arg(1);
    if (!main_thread()) { finish(8, INPUT_UNSUPPORTED); return; }
    if (!guest_range(output, 22)) { finish(8, INPUT_BAD_ARGUMENT); return; }
    refresh_devices();
    InputHandle *h = handle(token);
    uint32_t result = INPUT_DISCONNECTED;
    if (h && h->connected) {
        result = h->auto_poll ? xbox_InputGetState(h->port, &h->state) : INPUT_OK;
        if (result == INPUT_OK) write_state(output, &h->state);
    }
    finish(8, result);
}

/* Reuse the kernel bridge's 32-bit event token translation. A host HANDLE cast
 * would truncate the native pointer. Call the confirmed NtSetEvent import using
 * a temporary guest stack frame, then restore the original caller frame. */
static uint32_t signal_event(uint32_t event)
{
    if (!event) return INPUT_OK;
    if (g_esp < 12 || !guest_range(g_esp - 12, 12) || !guest_range(GUEST_NT_SET_EVENT, 4))
        return INPUT_BAD_ARGUMENT;
    recomp_func_t signal = recomp_lookup_kernel(read32(GUEST_NT_SET_EVENT));
    if (!signal) return INPUT_UNSUPPORTED;
    uint32_t saved_sp = g_esp;
    g_esp -= 12;
    write32(g_esp, 0);
    write32(g_esp + 4, event);
    write32(g_esp + 8, 0);
    signal();
    uint32_t status = g_eax;
    g_esp = saved_sp;
    return (int32_t)status < 0 ? INPUT_BAD_HANDLE : INPUT_OK;
}

static void input_feedback(void)
{
    uint32_t token = arg(0), feedback = arg(1);
    if (!main_thread()) { finish(8, INPUT_UNSUPPORTED); return; }
    if (!guest_range(feedback, 70)) { finish(8, INPUT_BAD_ARGUMENT); return; }
    refresh_devices();
    InputHandle *h = handle(token);
    uint32_t result = INPUT_DISCONNECTED;
    if (h && h->connected) {
        XBOX_VIBRATION vibration = {read16(feedback + 66), read16(feedback + 68)};
        ((uint8_t *)guest_ptr(feedback))[64] = 0; /* Report ID. */
        ((uint8_t *)guest_ptr(feedback))[65] = 6; /* Two header + four motor bytes. */
        result = xbox_InputSetState(h->port, &vibration);
    }
    /* SDL submits rumble synchronously: report its actual completion/error. */
    write32(feedback, result);
    if (result == INPUT_OK) {
        result = signal_event(read32(feedback + 4));
        write32(feedback, result);
    }
    finish(8, result);
}

/* The generated dispatch table also references exact sub_* symbols directly,
 * including manual exclusions. Export wrappers for both dispatch routes. */
void sub_00153C66(void) { input_init(); }
void sub_00154249(void) { input_open(); }
void sub_0015429F(void) { input_close(); }
void sub_001542AB(void) { input_state(); }
void sub_0015431A(void) { input_feedback(); }
void sub_00154AA8(void) { input_init(); }
void sub_00154AAD(void) { input_changes(); }

recomp_func_t wrath_input_lookup(uint32_t address)
{
    switch (address) {
    case 0x00154AA8u: return sub_00154AA8; /* XInitDevices public wrapper. */
    case 0x00153C66u: return sub_00153C66;
    case 0x00154AADu: return sub_00154AAD;
    case 0x00154249u: return sub_00154249;
    case 0x0015429Fu: return sub_0015429F;
    case 0x001542ABu: return sub_001542AB;
    case 0x0015431Au: return sub_0015431A;
    default: return NULL;
    }
}
