/* Original Xbox 4361 public DirectSound ABI -> native buffer engine.
 * Confirmed addresses/layouts and deliberate limitations: AUDIO-INTEGRATION.md.
 * No guest CPU interpreter, device registers, or native pointers in guest RAM. */
#include "audio/dsound_xbox.h"
#include "apu/apu.h"
#include "apu/apu_xaudio2.h"
#include <pthread.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern _Thread_local uint32_t g_eax, g_esp;
extern ptrdiff_t g_xbox_mem_offset;
extern size_t g_xbox_map_size, g_xbox_total_ram;
extern uint32_t xbox_HeapAlloc(uint32_t bytes, uint32_t alignment);
extern void xbox_HeapFree(uint32_t address);
typedef void (*recomp_func_t)(void);
#define AUDIO_OBJECTS 512
#define AUDIO_BADFORMAT ((HRESULT)0x88780064u)
static pthread_mutex_t s_lock = PTHREAD_MUTEX_INITIALIZER;
static MCPXAPUState *s_apu;
static IDirectSound8 *s_device;
static uint32_t s_device_guest, s_device_refs;
static struct Buffer {
    uint32_t guest;
    IDirectSoundBuffer8 *native;
    uint8_t format[20];
} s_buffers[AUDIO_OBJECTS];

static void *ptr(uint32_t address) { return (void *)((uintptr_t)address + g_xbox_mem_offset); }
static int range(uint32_t address, size_t bytes)
{
    size_t limit = g_xbox_total_ram ? g_xbox_total_ram : g_xbox_map_size;
    return address >= 0x10000u && address < limit && bytes <= limit - address;
}
static uint32_t read32(uint32_t address) { uint32_t v; memcpy(&v, ptr(address), 4); return v; }
static uint16_t read16(uint32_t address) { uint16_t v; memcpy(&v, ptr(address), 2); return v; }
static void write32(uint32_t address, uint32_t value) { memcpy(ptr(address), &value, 4); }
static uint32_t arg(unsigned index) { return read32(g_esp + 4 + index * 4); }
static void finish(unsigned bytes, HRESULT result) { g_eax = (uint32_t)result; g_esp += bytes + 4; }
static struct Buffer *buffer(uint32_t guest)
{
    for (unsigned i = 0; i < AUDIO_OBJECTS; ++i)
        if (s_buffers[i].native && s_buffers[i].guest == guest) return &s_buffers[i];
    return NULL;
}
static HRESULT unsupported(const char *name)
{
    /* One line per operation, bounded even if the title retries every frame. */
    static const char *warned[32]; static unsigned count;
    for (unsigned i = 0; i < count; ++i) if (warned[i] == name) return E_NOTIMPL;
    if (count < 32) {
        warned[count++] = name;
        fprintf(stderr, "[wrath audio] %s unsupported; guest return 0x%08X\n", name, read32(g_esp));
    }
    return E_NOTIMPL;
}
static int read_format(uint32_t guest, XBOX_WAVEFORMATEX *native, uint8_t serialized[20])
{
    if (!range(guest, 18)) return 0;
    unsigned extra = read16(guest + 16);
    if (extra > 2 || !range(guest, 18 + extra)) return 0;
    memset(serialized, 0, 20); memcpy(serialized, ptr(guest), 18 + extra);
    memset(native, 0, sizeof(*native));
    native->wFormatTag = read16(guest); native->nChannels = read16(guest + 2);
    native->nSamplesPerSec = read32(guest + 4); native->nAvgBytesPerSec = read32(guest + 8);
    native->nBlockAlign = read16(guest + 12); native->wBitsPerSample = read16(guest + 14);
    native->cbSize = extra;
    _Static_assert(sizeof(*native) >= 20, "native ADPCM extension capacity");
    if (extra) memcpy((uint8_t *)native + 18, serialized + 18, extra);
    return 1;
}
static void stop_if_unused(void)
{
    if (s_device_refs) return;
    for (unsigned i = 0; i < AUDIO_OBJECTS; ++i) if (s_buffers[i].native) return;
    if (s_apu) { mcpx_apu_shutdown(s_apu); s_apu = NULL; }
    s_device = NULL;
}
static void create_device(void)
{
    uint32_t output = arg(1);
    if (!range(output, 4)) { finish(12, E_INVALIDARG); return; }
    write32(output, 0);
    if (arg(0) || arg(2)) { finish(12, E_INVALIDARG); return; }
    if (!s_device_guest) s_device_guest = xbox_HeapAlloc(16, 16);
    if (!s_device_guest) { finish(12, E_OUTOFMEMORY); return; }
    if (!s_device) {
        HRESULT hr = xbox_DirectSoundCreate(NULL, &s_device, NULL);
        if (hr < 0) { finish(12, hr); return; }
        s_apu = mcpx_apu_init_standalone(NULL); /* Software PCM producer only. */
        if (!s_apu || !xa2_is_active()) {
            if (s_apu) mcpx_apu_shutdown(s_apu);
            s_apu = NULL; s_device = NULL; finish(12, E_FAIL); return;
        }
    }
    ++s_device_refs;
    write32(s_device_guest, 0x41554444u); write32(s_device_guest + 4, s_device_refs);
    write32(output, s_device_guest); finish(12, S_OK);
}
static void create_buffer(void)
{
    uint32_t desc = arg(0), output = arg(1);
    if (!range(output, 4)) { finish(8, E_INVALIDARG); return; }
    write32(output, 0);
    if (!s_device || !range(desc, 24) || read32(desc) != 24) { finish(8, E_INVALIDARG); return; }
    if (read32(desc + 16) || read32(desc + 20)) {
        finish(8, unsupported("buffer mixbin routing")); return;
    }
    struct Buffer *b = NULL;
    for (unsigned i = 0; i < AUDIO_OBJECTS; ++i) if (!s_buffers[i].native) { b = &s_buffers[i]; break; }
    if (!b) { finish(8, E_OUTOFMEMORY); return; }
    XBOX_WAVEFORMATEX format; uint8_t serialized[20];
    if (!read_format(read32(desc + 12), &format, serialized)) { finish(8, AUDIO_BADFORMAT); return; }
    DSBUFFERDESC native = {0};
    native.dwSize = sizeof(native); native.dwFlags = read32(desc + 4);
    native.dwBufferBytes = read32(desc + 8); native.lpwfxFormat = &format;
    if (!b->guest) b->guest = xbox_HeapAlloc(32, 16);
    if (!b->guest) { finish(8, E_OUTOFMEMORY); return; }
    HRESULT hr = s_device->lpVtbl->CreateSoundBuffer(s_device, &native, &b->native, NULL);
    if (hr >= 0) {
        memcpy(b->format, serialized, 20);
        memset(ptr(b->guest), 0, 32); write32(b->guest, 0x41554442u);
        write32(output, b->guest);
    }
    finish(8, hr);
}
static void release_device(void)
{
    if (arg(0) != s_device_guest || !s_device_refs) { finish(4, 0); return; }
    --s_device_refs; write32(s_device_guest + 4, s_device_refs);
    stop_if_unused(); finish(4, s_device_refs);
}
static void release_buffer(void)
{
    struct Buffer *b = buffer(arg(0));
    if (!b) { finish(4, 0); return; }
    ULONG refs = b->native->lpVtbl->Release(b->native);
    if (!refs) { b->native = NULL; memset(ptr(b->guest), 0, 32); }
    stop_if_unused(); finish(4, refs);
}
static void buffer_data(void)
{
    struct Buffer *b = buffer(arg(0)); uint32_t data = arg(1), bytes = arg(2);
    if (!b || (bytes && !range(data, bytes))) { finish(12, E_INVALIDARG); return; }
    finish(12, b->native->lpVtbl->SetBufferData(b->native, bytes ? ptr(data) : NULL, bytes));
}
static void buffer_format(void)
{
    struct Buffer *b = buffer(arg(0)); XBOX_WAVEFORMATEX format; uint8_t serialized[20];
    if (!b) { finish(8, E_INVALIDARG); return; }
    if (!read_format(arg(1), &format, serialized)) { finish(8, AUDIO_BADFORMAT); return; }
    HRESULT hr = xbox_DirectSoundBufferSetFormat(b->native, &format);
    if (hr >= 0) memcpy(b->format, serialized, 20);
    finish(8, hr);
}
static void buffer_play(void)
{
    struct Buffer *b = buffer(arg(0));
    finish(16, b ? b->native->lpVtbl->Play(b->native, arg(1), arg(2), arg(3)) : E_INVALIDARG);
}
static void buffer_stop(void)
{
    struct Buffer *b = buffer(arg(0)); finish(4, b ? b->native->lpVtbl->Stop(b->native) : E_INVALIDARG);
}
static void buffer_volume(void)
{
    struct Buffer *b = buffer(arg(0));
    finish(8, b ? b->native->lpVtbl->SetVolume(b->native, (int32_t)arg(1)) : E_INVALIDARG);
}
static void buffer_pitch(void)
{
    struct Buffer *b = buffer(arg(0)); int32_t pitch = (int32_t)arg(1);
    /* The title SDK computes pitch = 4096*log2(rate/48000). It is absolute,
     * including for a buffer whose source format is 22050 Hz. */
    if (!b || pitch < -32767 || pitch > 8191) { finish(8, E_INVALIDARG); return; }
    double rate = 48000.0 * exp2((double)pitch / 4096.0);
    if (!isfinite(rate) || rate < 100.0 || rate > 192000.0) { finish(8, E_INVALIDARG); return; }
    finish(8, b->native->lpVtbl->SetFrequency(b->native, (DWORD)lround(rate)));
}

static void buffer_seek(void)
{
    struct Buffer *b = buffer(arg(0));
    finish(8, b ? b->native->lpVtbl->SetCurrentPosition(b->native, arg(1)) : E_INVALIDARG);
}
static void buffer_status(void)
{
    struct Buffer *b = buffer(arg(0)); uint32_t output = arg(1); DWORD value = 0;
    if (!b || !range(output, 4)) { finish(8, E_INVALIDARG); return; }
    HRESULT hr = b->native->lpVtbl->GetStatus(b->native, &value);
    if (hr >= 0) write32(output, value); finish(8, hr);
}
static void buffer_position(void)
{
    struct Buffer *b = buffer(arg(0)); uint32_t play = arg(1), write = arg(2); DWORD p, w;
    if (!b || (play && !range(play, 4)) || (write && !range(write, 4))) { finish(12, E_INVALIDARG); return; }
    HRESULT hr = b->native->lpVtbl->GetCurrentPosition(b->native, &p, &w);
    if (hr >= 0) { if (play) write32(play, p); if (write) write32(write, w); } finish(12, hr);
}
static void effects(void)
{
    uint32_t output = arg(4);
    if (!range(output, 4)) { finish(20, E_INVALIDARG); return; }
    write32(output, 0); finish(20, unsupported("DSP effect image"));
}
static void stream(void)
{
    uint32_t output = arg(1);
    if (!range(output, 4)) { finish(8, E_INVALIDARG); return; }
    write32(output, 0); finish(8, unsupported("ADPCM packet stream"));
}
static void do_work(void)
{
    /* The native producer advances playback independently. Querying actually
     * reaps completed voices; output stream completions are not synthesized. */
    for (unsigned i = 0; i < AUDIO_OBJECTS; ++i) if (s_buffers[i].native) {
        DWORD status; s_buffers[i].native->lpVtbl->GetStatus(s_buffers[i].native, &status);
    }
    g_esp += 4;
}
#define BRIDGE(address, body) void sub_##address(void) { pthread_mutex_lock(&s_lock); body; pthread_mutex_unlock(&s_lock); }
BRIDGE(00137A06, create_device())
BRIDGE(00137A4D, create_buffer())
BRIDGE(00135BE8, release_device())
BRIDGE(00135BFE, release_buffer())
BRIDGE(0013755A, buffer_data())
BRIDGE(00136D21, buffer_format())
BRIDGE(00136664, buffer_play())
BRIDGE(00136688, buffer_stop())
BRIDGE(0013662C, buffer_volume())
BRIDGE(001366DC, buffer_seek())
BRIDGE(001366A0, buffer_status())
BRIDGE(001366BC, buffer_position())
BRIDGE(00136702, do_work())
BRIDGE(00136605, effects())
BRIDGE(00137AA4, stream())
BRIDGE(00135C14, unsupported("full HRTF"); g_esp += 4)
BRIDGE(00136648, buffer_pitch())
BRIDGE(00136D3D, finish(12, unsupported("buffer maximum distance")))
BRIDGE(00136D61, finish(12, unsupported("buffer minimum distance")))
BRIDGE(00136D85, finish(20, unsupported("buffer spatial position")))
BRIDGE(00136CED, finish(8, unsupported("headphone HRTF")))
BRIDGE(00136D09, finish(4, unsupported("deferred spatial settings")))
BRIDGE(00137497, finish(32, unsupported("listener orientation")))
BRIDGE(001374E1, finish(20, unsupported("listener position")))
BRIDGE(00137516, finish(12, unsupported("listener rolloff")))
BRIDGE(0013753A, finish(12, unsupported("I3DL2 listener")))
BRIDGE(001366F8, finish(8, unsupported("stream volume")))
BRIDGE(001366FD, finish(8, unsupported("stream pause")))
#undef BRIDGE

recomp_func_t wrath_audio_lookup(uint32_t address)
{
#define ENTRY(a) case 0x##a: return sub_##a
    switch (address) {
    ENTRY(00137A06); ENTRY(00137A4D); ENTRY(00135BE8); ENTRY(00135BFE);
    ENTRY(0013755A); ENTRY(00136D21); ENTRY(00136664); ENTRY(00136688);
    ENTRY(0013662C); ENTRY(001366DC); ENTRY(001366A0); ENTRY(001366BC);
    ENTRY(00136702); ENTRY(00136605); ENTRY(00137AA4); ENTRY(00135C14);
    ENTRY(00136648); ENTRY(00136D3D); ENTRY(00136D61); ENTRY(00136D85);
    ENTRY(00136CED); ENTRY(00136D09); ENTRY(00137497); ENTRY(001374E1);
    ENTRY(00137516); ENTRY(0013753A); ENTRY(001366F8); ENTRY(001366FD);
    default: return NULL;
    }
#undef ENTRY
}
/* Call after guest workers stop and before SDL/platform teardown. */
void wrath_audio_shutdown(void)
{
    pthread_mutex_lock(&s_lock);
    for (unsigned i = 0; i < AUDIO_OBJECTS; ++i) {
        if (s_buffers[i].native) s_buffers[i].native->lpVtbl->Release(s_buffers[i].native);
        if (s_buffers[i].guest) xbox_HeapFree(s_buffers[i].guest);
        memset(&s_buffers[i], 0, sizeof(s_buffers[i]));
    }
    s_device_refs = 0; stop_if_unused();
    if (s_device_guest) xbox_HeapFree(s_device_guest);
    s_device_guest = 0;
    pthread_mutex_unlock(&s_lock);
}
