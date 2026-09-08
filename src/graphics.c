/* Initial Xbox 4361 -> native OpenGL bridge. See docs/D3D-INTEGRATION.md.
 * These are compiled host replacements for four identified SDK functions.
 * All entry points must execute on the SDL/main thread. */
#include "d3d/d3d8_xbox.h"
#include <SDL.h>
#include <epoxy/gl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#ifdef __APPLE__
#include <pthread.h>
#endif

extern _Thread_local uint32_t g_eax, g_esp;
extern ptrdiff_t g_xbox_mem_offset;
typedef void (*recomp_func_t)(void);

#define D3DERR_INVALIDCALL ((HRESULT)0x8876086Cu)
#define D3D_OK ((HRESULT)0)
#define GUEST_DEVICE 0x0010C110u
#define GUEST_DEVICE_GLOBAL 0x0010EBF0u
#define GUEST_DEVICE_GLOBAL_COPY 0x0010EBF4u
#define GUEST_VIEWPORT (GUEST_DEVICE + 0x9D0u)
#define GUEST_SWAP_COUNT (GUEST_DEVICE + 0x2AC4u)

/* Pointers/handles in this serialized guest structure are always 32 bits. */
typedef struct GuestPresentation {
    uint32_t width, height, format, buffer_count;
    uint32_t multisample, swap_effect, window, windowed;
    uint32_t depth_enabled, depth_format, flags, refresh_hz, interval;
    uint32_t buffer_surfaces[3], depth_surface;
} GuestPresentation;
_Static_assert(sizeof(GuestPresentation) == 68, "Xbox presentation layout");
_Static_assert(offsetof(GuestPresentation, buffer_surfaces) == 52, "Xbox surface offset");
_Static_assert(sizeof(D3DVIEWPORT8) == 24, "Xbox viewport layout");

static IDirect3DDevice8 *s_device;
static UINT s_width, s_height;
static D3DVIEWPORT8 s_viewport;

static void *guest_ptr(uint32_t address)
{
    return (void *)((uintptr_t)address + g_xbox_mem_offset);
}
static uint32_t read32(uint32_t address)
{
    uint32_t value;
    memcpy(&value, guest_ptr(address), sizeof(value));
    return value;
}
static void write32(uint32_t address, uint32_t value)
{
    memcpy(guest_ptr(address), &value, sizeof(value));
}
static uint32_t arg(unsigned index) { return read32(g_esp + 4 + index * 4); }
static void finish(unsigned bytes, uint32_t result)
{
    g_eax = result;
    g_esp += 4 + bytes; /* guest return address plus callee-popped arguments */
}
static int main_thread(void)
{
#ifdef __APPLE__
    if (!pthread_main_np()) {
        fprintf(stderr, "[wrath graphics] SDL/OpenGL call outside main thread\n");
        return 0;
    }
#endif
    return 1;
}

/* HRESULT WINAPI Direct3D_CreateDevice(adapter,type,window,flags,pp,out). */
void sub_000FD6E0(void)
{
    uint32_t pp_address = arg(4), output = arg(5), behavior = arg(3);
    if (!main_thread() || !pp_address || !output || s_device) {
        finish(24, (uint32_t)D3DERR_INVALIDCALL);
        return;
    }
    GuestPresentation in;
    memcpy(&in, guest_ptr(pp_address), sizeof(in));
    write32(output, 0);
    if (in.width == 0 || in.height == 0 || in.width > 4096 || in.height > 4096 ||
        in.buffer_surfaces[0] || in.buffer_surfaces[1] || in.buffer_surfaces[2] ||
        in.depth_surface) {
        fprintf(stderr, "[wrath graphics] unsupported dimensions or supplied surfaces\n");
        finish(24, (uint32_t)D3DERR_INVALIDCALL);
        return;
    }
    D3DPRESENT_PARAMETERS pp = {0};
    pp.BackBufferWidth = in.width;
    pp.BackBufferHeight = in.height;
    pp.BackBufferFormat = (D3DFORMAT)in.format;
    pp.BackBufferCount = in.buffer_count;
    pp.MultiSampleType = (D3DMULTISAMPLE_TYPE)in.multisample;
    pp.SwapEffect = (D3DSWAPEFFECT)in.swap_effect;
    pp.hDeviceWindow = NULL; /* Xbox window tokens are not native Cocoa windows. */
    pp.Windowed = TRUE;
    pp.EnableAutoDepthStencil = in.depth_enabled;
    pp.AutoDepthStencilFormat = (D3DFORMAT)in.depth_format;
    pp.Flags = in.flags;
    pp.FullScreen_RefreshRateInHz = in.refresh_hz;
    pp.FullScreen_PresentationInterval = in.interval;
    xbox_D3D8SetWindowTitle("Crash Bandicoot: The Wrath of Cortex — native development");
    IDirect3D8 *factory = xbox_Direct3DCreate8(4361);
    HRESULT result = factory->lpVtbl->CreateDevice(factory, arg(0), arg(1),
                                                   NULL, behavior, &pp, &s_device);
    if (result < 0 || !s_device) {
        s_device = NULL;
        fprintf(stderr, "[wrath graphics] native CreateDevice failed: 0x%08X\n", (unsigned)result);
        finish(24, result < 0 ? (uint32_t)result : (uint32_t)D3DERR_INVALIDCALL);
        return;
    }
    s_width = pp.BackBufferWidth;
    s_height = pp.BackBufferHeight;
    s_viewport = (D3DVIEWPORT8){0, 0, s_width, s_height, 0.0f, 1.0f};
    s_device->lpVtbl->SetViewport(s_device, &s_viewport);
    /* Reproduce observed SDK globals, never store native pointers in Xbox RAM.
     * Preserve other static device bytes; unbridged SDK subsystems remain WIP. */
    if (!read32(0x10F57C)) write32(0x10F57C, 0x80000);
    if (!read32(0x10F578)) write32(0x10F578, 0x8000);
    write32(GUEST_DEVICE_GLOBAL, GUEST_DEVICE);
    write32(GUEST_DEVICE_GLOBAL_COPY, GUEST_DEVICE);
    write32(GUEST_DEVICE + 8, read32(GUEST_DEVICE + 8) | (behavior & 0x10));
    write32(0x10C550, 1);
    write32(GUEST_SWAP_COUNT, 0);
    memcpy(guest_ptr(GUEST_VIEWPORT), &s_viewport, sizeof(s_viewport));
    write32(output, GUEST_DEVICE);
    fprintf(stderr, "[wrath graphics] native device %ux%u, guest handle 0x%08X\n",
            s_width, s_height, GUEST_DEVICE);
    finish(24, 0);
}

/* DWORD/void WINAPI SetViewport(const XboxViewport*), same 24-byte layout. */
void sub_000FF860(void)
{
    uint32_t address = arg(0);
    D3DVIEWPORT8 vp;
    if (!s_device || !main_thread() || !address) {
        finish(4, (uint32_t)D3DERR_INVALIDCALL);
        return;
    }
    memcpy(&vp, guest_ptr(address), sizeof(vp));
    /* SDK clamps the viewport to its render target. This bridge currently has
     * only the native window render target. */
    if (vp.X > s_width) vp.X = s_width;
    if (vp.Y > s_height) vp.Y = s_height;
    if (vp.Width > s_width - vp.X) vp.Width = s_width - vp.X;
    if (vp.Height > s_height - vp.Y) vp.Height = s_height - vp.Y;
    if (!isfinite(vp.MinZ) || !isfinite(vp.MaxZ)) {
        finish(4, (uint32_t)D3DERR_INVALIDCALL);
        return;
    }
    HRESULT result = s_device->lpVtbl->SetViewport(s_device, &vp);
    if (result >= 0) {
        s_viewport = vp;
        memcpy(guest_ptr(GUEST_VIEWPORT), &vp, sizeof(vp));
    }
    finish(4, (uint32_t)result);
}

/* GL fallback preserves Xbox per-channel clears and viewport/rectangle bounds;
 * the initial upstream native Clear ignores these D3D arguments. */
static void clear_rectangles(uint32_t count, uint32_t rectangles, uint32_t flags,
                             uint32_t color, float depth, uint32_t stencil)
{
    GLboolean old_color_mask[4], old_depth_mask, old_scissor_enabled;
    GLint old_scissor[4], old_stencil_mask, old_back_stencil_mask, old_stencil_clear;
    GLfloat old_color[4];
    GLdouble old_depth;
    glGetBooleanv(GL_COLOR_WRITEMASK, old_color_mask);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &old_depth_mask);
    old_scissor_enabled = glIsEnabled(GL_SCISSOR_TEST);
    glGetIntegerv(GL_SCISSOR_BOX, old_scissor);
    glGetIntegerv(GL_STENCIL_WRITEMASK, &old_stencil_mask);
    glGetIntegerv(GL_STENCIL_BACK_WRITEMASK, &old_back_stencil_mask);
    glGetIntegerv(GL_STENCIL_CLEAR_VALUE, &old_stencil_clear);
    glGetFloatv(GL_COLOR_CLEAR_VALUE, old_color);
    glGetDoublev(GL_DEPTH_CLEAR_VALUE, &old_depth);
    glColorMask((flags & 0x10) != 0, (flags & 0x20) != 0,
                (flags & 0x40) != 0, (flags & 0x80) != 0);
    glDepthMask(GL_TRUE);
    glStencilMask(~0u);
    glClearColor(((color >> 16) & 255) / 255.0f, ((color >> 8) & 255) / 255.0f,
                 (color & 255) / 255.0f, (color >> 24) / 255.0f);
    glClearDepth(depth);
    glClearStencil(stencil);
    glEnable(GL_SCISSOR_TEST);
    GLbitfield bits = ((flags & 0xF0) ? GL_COLOR_BUFFER_BIT : 0) |
                     ((flags & 1) ? GL_DEPTH_BUFFER_BIT : 0) |
                     ((flags & 2) ? GL_STENCIL_BUFFER_BIT : 0);
    for (uint32_t i = 0; i < (count ? count : 1); ++i) {
        int32_t left = s_viewport.X, top = s_viewport.Y;
        int32_t right = left + s_viewport.Width, bottom = top + s_viewport.Height;
        if (count) {
            D3DRECT rect;
            memcpy(&rect, guest_ptr(rectangles + i * sizeof(rect)), sizeof(rect));
            if (rect.x1 > left) left = rect.x1;
            if (rect.y1 > top) top = rect.y1;
            if (rect.x2 < right) right = rect.x2;
            if (rect.y2 < bottom) bottom = rect.y2;
        }
        if (right > left && bottom > top) {
            glScissor(left, (GLint)s_height - bottom, right - left, bottom - top);
            glClear(bits);
        }
    }
    glColorMask(old_color_mask[0], old_color_mask[1], old_color_mask[2], old_color_mask[3]);
    glDepthMask(old_depth_mask);
    glStencilMaskSeparate(GL_FRONT, (GLuint)old_stencil_mask);
    glStencilMaskSeparate(GL_BACK, (GLuint)old_back_stencil_mask);
    glScissor(old_scissor[0], old_scissor[1], old_scissor[2], old_scissor[3]);
    if (!old_scissor_enabled) glDisable(GL_SCISSOR_TEST);
    glClearColor(old_color[0], old_color[1], old_color[2], old_color[3]);
    glClearDepth(old_depth);
    glClearStencil(old_stencil_clear);
}

void sub_00100EA0(void)
{
    uint32_t count = arg(0), rects = arg(1), flags = arg(2), color = arg(3);
    uint32_t depth_bits = arg(4), stencil = arg(5);
    float depth;
    memcpy(&depth, &depth_bits, sizeof(depth));
    if (!s_device || !main_thread() || (count && !rects) || count > 65536 ||
        (flags & ~0xF3u) || !isfinite(depth)) {
        finish(24, (uint32_t)D3DERR_INVALIDCALL);
        return;
    }
    uint32_t native_flags = ((flags & 0xF0) ? D3DCLEAR_TARGET : 0) |
                            ((flags & 1) ? D3DCLEAR_ZBUFFER : 0) |
                            ((flags & 2) ? D3DCLEAR_STENCIL : 0);
    int whole_target = !count && s_viewport.X == 0 && s_viewport.Y == 0 &&
        s_viewport.Width == s_width && s_viewport.Height == s_height;
    /* Use native API for its supported whole-target clear. Stencil write masks
     * and partial channels require the bridge's exact GL path. */
    HRESULT result = D3D_OK;
    if (whole_target && (flags & 0xF0) == 0xF0 && !(flags & 2))
        result = s_device->lpVtbl->Clear(s_device, 0, NULL, native_flags, color, depth, stencil);
    else
        clear_rectangles(count, rects, flags, color, depth, stencil);
    finish(24, (uint32_t)result);
}

/* DWORD WINAPI Swap(flags): return observed swap count, not HRESULT. */
void sub_00100C40(void)
{
    uint32_t flags = arg(0);
    if (!s_device || !main_thread()) {
        finish(4, (uint32_t)D3DERR_INVALIDCALL);
        return;
    }
    if (!flags) flags = 5; /* SDK's default, observed at 0x00100C50. */
    /* Bit 2 submits a new swap; flag-only wait operations do not present. */
    if (flags & 4) {
        HRESULT result = s_device->lpVtbl->Swap(s_device, flags);
        if (result < 0) {
            finish(4, (uint32_t)result);
            return;
        }
        write32(GUEST_SWAP_COUNT, read32(GUEST_SWAP_COUNT) + 1);
    }
    finish(4, read32(GUEST_SWAP_COUNT));
}

/* Resource/draw subset: guest bytes are authoritative; native objects are never
 * serialized into the 12/20-byte Xbox resource headers. */
extern uint32_t xbox_HeapAlloc(uint32_t size, uint32_t alignment);
extern void xbox_HeapFree(uint32_t address);
#define RESOURCE_LIMIT 1024
#define RESOURCE_TEXTURE 1
#define RESOURCE_VERTEX_BUFFER 2
#define RESOURCE_SURFACE 3
static struct Resource {
    uint32_t handle, data, bytes, width, height, levels, format, pitch, owner;
    uint32_t offsets[13], pitches[13], sizes[13];
    unsigned type, references, bindings;
    GLenum framebuffer;
    int linear, dirty;
    IDirect3DTexture8 *texture;
    IDirect3DVertexBuffer8 *vertex_buffer;
} s_resources[RESOURCE_LIMIT];
static uint32_t s_texture_handles[4], s_stream_handle, s_stream_stride, s_fvf;

static struct Resource *resource(uint32_t handle)
{
    for (unsigned i = 0; i < RESOURCE_LIMIT; ++i)
        if (s_resources[i].handle == handle && handle) return &s_resources[i];
    return NULL;
}
static struct Resource *new_resource(void)
{
    for (unsigned i = 0; i < RESOURCE_LIMIT; ++i)
        if (!s_resources[i].handle) return &s_resources[i];
    return NULL;
}
static void update_common(struct Resource *r)
{
    uint32_t kind = r->type == RESOURCE_TEXTURE ? 0x40000 : r->type == RESOURCE_SURFACE ? 0x50000 : 0;
    write32(r->handle, 0x1000000 | kind | (r->references & 0xFFFF) | (r->bindings << 19));
}
static void release_resource(struct Resource *r)
{
    if (r->references || r->bindings) { update_common(r); return; }
    if (r->texture) r->texture->lpVtbl->Release(r->texture);
    if (r->vertex_buffer) r->vertex_buffer->lpVtbl->Release(r->vertex_buffer);
    uint32_t owner = r->owner;
    if (!owner) xbox_HeapFree(r->data);
    xbox_HeapFree(r->handle);
    memset(r, 0, sizeof(*r));
    if (owner) {
        struct Resource *parent = resource(owner);
        if (parent && parent->references) { --parent->references; release_resource(parent); }
    }
}
static int format_info(uint32_t format, int *linear)
{
    *linear = 0;
    switch (format) {
    case 0x12: case 0x1E: *linear = 1; return 4;
    case 0x10: case 0x11: case 0x1C: case 0x1D: *linear = 1; return 2;
    case 0x13: case 0x1F: *linear = 1; return 1;
    case 0x20: *linear = 1; return 2;
    case 6: case 7: return 4;
    case 2: case 3: case 4: case 5: case 0x1A: return 2;
    case 0: case 0x19: return 1;
    case 0x0C: case 0x0E: case 0x0F: return 0; /* Block compressed. */
    default: return -1;
    }
}
static unsigned log2_size(uint32_t value)
{
    unsigned n = 0;
    while (value > 1) { value >>= 1; ++n; }
    return n;
}
static uint32_t morton_index(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    uint32_t result = 0, output = 1;
    for (uint32_t bit = 1; bit < width || bit < height; bit <<= 1) {
        if (bit < width) { if (x & bit) result |= output; output <<= 1; }
        if (bit < height) { if (y & bit) result |= output; output <<= 1; }
    }
    return result;
}
static uint32_t color565(uint16_t value)
{
    uint32_t r = (value >> 11) & 31, g = (value >> 5) & 63, b = value & 31;
    return 0xFF000000 | ((r * 255 / 31) << 16) | ((g * 255 / 63) << 8) | (b * 255 / 31);
}
static uint32_t uncompressed_color(uint32_t value, uint32_t format)
{
    switch (format) {
    case 6: case 0x12: return value;
    case 7: case 0x1E: return value | 0xFF000000;
    case 5: case 0x11: return color565((uint16_t)value);
    case 2: case 3: case 0x10: case 0x1C: {
        uint32_t alpha = (format == 3 || format == 0x1C || (value & 0x8000)) ? 255 : 0;
        return (alpha << 24) | ((((value >> 10) & 31) * 255 / 31) << 16) |
               ((((value >> 5) & 31) * 255 / 31) << 8) | ((value & 31) * 255 / 31);
    }
    case 4: case 0x1D:
        return (((value >> 12) & 15) * 17 << 24) | (((value >> 8) & 15) * 17 << 16) |
               (((value >> 4) & 15) * 17 << 8) | ((value & 15) * 17);
    case 0: case 0x13: return 0xFF000000 | (value & 255) * 0x010101;
    case 0x19: case 0x1F: return ((value & 255) << 24) | 0xFFFFFF;
    case 0x1A: case 0x20: return ((value & 0xFF00) << 16) | (value & 255) * 0x010101;
    default: return 0;
    }
}
static uint32_t dxt_color(const uint8_t *block, unsigned pixel, uint32_t format)
{
    const uint8_t *colors = block + (format == 0x0C ? 0 : 8);
    uint16_t c0, c1; uint32_t selectors;
    memcpy(&c0, colors, 2); memcpy(&c1, colors + 2, 2); memcpy(&selectors, colors + 4, 4);
    uint32_t palette[4] = {color565(c0), color565(c1), 0, 0};
    for (unsigned shift = 0; shift < 24; shift += 8) {
        unsigned a = (palette[0] >> shift) & 255, b = (palette[1] >> shift) & 255;
        if (c0 > c1 || format != 0x0C) {
            palette[2] |= ((2 * a + b) / 3) << shift;
            palette[3] |= ((a + 2 * b) / 3) << shift;
        } else palette[2] |= ((a + b) / 2) << shift;
    }
    palette[2] |= 0xFF000000;
    if (c0 > c1 || format != 0x0C) palette[3] |= 0xFF000000;
    uint32_t result = palette[(selectors >> (pixel * 2)) & 3];
    if (format == 0x0E) {
        unsigned alpha = (block[pixel / 2] >> ((pixel & 1) * 4)) & 15;
        result = (result & 0xFFFFFF) | (alpha * 17 << 24);
    } else if (format == 0x0F) {
        unsigned alpha[8] = {block[0], block[1], 0, 0, 0, 0, 0, 255};
        unsigned n = alpha[0] > alpha[1] ? 7 : 5;
        for (unsigned i = 1; i < n; ++i) alpha[i + 1] = ((n - i) * alpha[0] + i * alpha[1]) / n;
        uint64_t bits = 0;
        for (unsigned i = 0; i < 6; ++i) bits |= (uint64_t)block[i + 2] << (i * 8);
        result = (result & 0xFFFFFF) | (alpha[(bits >> (pixel * 3)) & 7] << 24);
    }
    return result;
}
static HRESULT upload_texture(struct Resource *r)
{
    if (!r->dirty) return 0;
    D3DLOCKED_RECT locked;
    HRESULT result = r->texture->lpVtbl->LockRect(r->texture, 0, &locked, NULL, 0);
    if (result < 0) return result;
    int linear, bpp = format_info(r->format, &linear);
    const uint8_t *source = guest_ptr(r->data);
    for (uint32_t y = 0; y < r->height; ++y) {
        uint32_t *row = (uint32_t *)((uint8_t *)locked.pBits + y * locked.Pitch);
        for (uint32_t x = 0; x < r->width; ++x) {
            if (!bpp) {
                unsigned block_size = r->format == 0x0C ? 8 : 16;
                unsigned block = (y / 4) * ((r->width + 3) / 4) + x / 4;
                row[x] = dxt_color(source + block * block_size, (y % 4) * 4 + x % 4, r->format);
            } else {
                uint32_t offset = linear ? y * r->pitch + x * bpp :
                    morton_index(x, y, r->width, r->height) * bpp;
                uint32_t value = 0; memcpy(&value, source + offset, (size_t)bpp);
                row[x] = uncompressed_color(value, r->format);
            }
        }
    }
    result = r->texture->lpVtbl->UnlockRect(r->texture, 0);
    if (result >= 0) r->dirty = 0;
    return result;
}

void sub_000FE9C0(void) /* CreateTexture(w,h,levels,usage,format,pool,out), ret28 */
{
    uint32_t width = arg(0), height = arg(1), levels = arg(2), format = arg(4), output = arg(6);
    int linear, bpp = format_info(format, &linear);
    struct Resource *r = new_resource();
    if (!s_device || !main_thread() || !output || !r || bpp < 0 || !width || !height ||
        width > 4096 || height > 4096 || (!linear && ((width & (width - 1)) || (height & (height - 1))))) {
        fprintf(stderr, "[wrath graphics] unsupported texture %ux%u format 0x%X\n", width, height, format);
        finish(28, (uint32_t)D3DERR_INVALIDCALL); return;
    }
    write32(output, 0);
    unsigned max_levels = log2_size(width > height ? width : height) + 1;
    if (!levels) levels = linear ? 1 : max_levels;
    if (levels > max_levels || (linear && levels != 1)) { finish(28, (uint32_t)D3DERR_INVALIDCALL); return; }
    r->width = width; r->height = height; r->format = format; r->levels = levels;
    r->linear = linear; r->references = 1; r->type = RESOURCE_TEXTURE; r->dirty = 1;
    for (unsigned level = 0; level < levels; ++level) {
        uint32_t w = width >> level, h = height >> level;
        if (!w) w = 1; if (!h) h = 1;
        r->offsets[level] = r->bytes;
        r->pitches[level] = bpp ? w * bpp : ((w + 3) / 4) * (format == 0x0C ? 8 : 16);
        if (linear) r->pitches[level] = (r->pitches[level] + 63) & ~63u;
        r->sizes[level] = r->pitches[level] * (bpp ? h : (h + 3) / 4);
        r->bytes += r->sizes[level];
    }
    r->pitch = r->pitches[0];
    r->handle = xbox_HeapAlloc(20, 16);
    r->data = xbox_HeapAlloc(r->bytes, 128);
    HRESULT result = 0;
    if (!r->handle || !r->data) result = (HRESULT)0x8007000E;
    else result = s_device->lpVtbl->CreateTexture(s_device, width, height, 1, arg(3),
                                                  D3DFMT_A8R8G8B8, (D3DPOOL)arg(5), &r->texture);
    if (result < 0) {
        if (r->handle) xbox_HeapFree(r->handle);
        if (r->data) xbox_HeapFree(r->data);
        memset(r, 0, sizeof(*r)); finish(28, (uint32_t)result); return;
    }
    memset(guest_ptr(r->data), 0, r->bytes);
    update_common(r); write32(r->handle + 4, r->data); write32(r->handle + 8, 0);
    uint32_t encoded = 0x21 | (format << 8) | (levels << 16);
    if (!linear) encoded |= log2_size(width) << 20 | log2_size(height) << 24;
    write32(r->handle + 12, encoded);
    write32(r->handle + 16, linear ? (width - 1) | ((height - 1) << 12) | ((r->pitch / 64 - 1) << 24) : 0);
    write32(output, r->handle); finish(28, 0);
}
void sub_00103C80(void) /* Texture_LockRect(texture,level,out,rect,flags) */
{
    struct Resource *r = resource(arg(0)); uint32_t level = arg(1), out = arg(2), rectangle = arg(3);
    if (!r || r->type != RESOURCE_TEXTURE || level >= r->levels || !out || (arg(4) & 0x40)) {
        finish(20, (uint32_t)D3DERR_INVALIDCALL); return;
    }
    uint32_t offset = r->offsets[level];
    if (rectangle) {
        D3DRECT rect; memcpy(&rect, guest_ptr(rectangle), sizeof(rect));
        uint32_t w = r->width >> level, h = r->height >> level;
        if (!w) w = 1; if (!h) h = 1;
        if (!r->linear || rect.x1 < 0 || rect.y1 < 0 || rect.x2 < rect.x1 || rect.y2 < rect.y1 ||
            (uint32_t)rect.x2 > w || (uint32_t)rect.y2 > h) { finish(20, (uint32_t)D3DERR_INVALIDCALL); return; }
        int linear; int bpp = format_info(r->format, &linear);
        offset += rect.y1 * r->pitches[level] + rect.x1 * bpp;
    }
    write32(out, r->pitches[level]); write32(out + 4, r->data + offset);
    if (!(arg(4) & 0x80)) r->dirty = 1;
    finish(20, 0);
}
void sub_00103C30(void) /* Texture_GetSurfaceLevel(texture,level,out) */
{
    struct Resource *parent = resource(arg(0)), *r = new_resource();
    uint32_t level = arg(1), out = arg(2);
    if (!parent || parent->type != RESOURCE_TEXTURE || level >= parent->levels || !out || !r) {
        finish(12, (uint32_t)D3DERR_INVALIDCALL); return;
    }
    write32(out, 0);
    r->handle = xbox_HeapAlloc(24, 16);
    if (!r->handle) { finish(12, 0x8007000E); return; }
    r->type = RESOURCE_SURFACE; r->owner = parent->handle; r->references = 1;
    r->width = parent->width >> level; if (!r->width) r->width = 1;
    r->height = parent->height >> level; if (!r->height) r->height = 1;
    r->levels = 1; r->format = parent->format; r->linear = parent->linear;
    r->data = parent->data + parent->offsets[level]; r->bytes = parent->sizes[level];
    r->pitch = parent->pitches[level]; r->pitches[0] = r->pitch; r->sizes[0] = r->bytes;
    ++parent->references; update_common(parent); update_common(r);
    write32(r->handle + 4, r->data); write32(r->handle + 8, 0);
    uint32_t encoded = 0x10021 | (r->format << 8);
    if (!r->linear) encoded |= log2_size(r->width) << 20 | log2_size(r->height) << 24;
    write32(r->handle + 12, encoded);
    write32(r->handle + 16, r->linear ? (r->width - 1) | ((r->height - 1) << 12) | ((r->pitch / 64 - 1) << 24) : 0);
    write32(r->handle + 20, parent->handle);
    write32(out, r->handle); finish(12, 0);
}
void sub_00103C20(void) /* Texture_GetLevelDesc(texture, level, desc) */
{
    struct Resource *r = resource(arg(0)); uint32_t level = arg(1), out = arg(2);
    if (!r || r->type != RESOURCE_TEXTURE || level >= r->levels || !out) { finish(12, (uint32_t)D3DERR_INVALIDCALL); return; }
    uint32_t w = r->width >> level, h = r->height >> level;
    if (!w) w = 1; if (!h) h = 1;
    uint32_t desc[7] = {r->format, 1, 0, r->sizes[level], 0, w, h};
    memcpy(guest_ptr(out), desc, sizeof(desc)); finish(12, 0);
}
void sub_00103DD0(void)
{
    struct Resource *r = resource(arg(0));
    finish(4, r && r->type == RESOURCE_TEXTURE ? r->levels : 0);
}
void sub_00103A90(void)
{
    struct Resource *r = resource(arg(0));
    if (!r || r->references == 0xFFFF) { fprintf(stderr, "[wrath graphics] invalid AddRef 0x%X\n", arg(0)); finish(4, 0); return; }
    ++r->references; update_common(r); finish(4, r->references);
}
void sub_00103AD0(void)
{
    struct Resource *r = resource(arg(0));
    if (!r || !r->references) { fprintf(stderr, "[wrath graphics] invalid Release 0x%X\n", arg(0)); finish(4, 0); return; }
    uint32_t references = --r->references; release_resource(r); finish(4, references);
}
void sub_000FFC90(void)
{
    uint32_t stage = arg(0), handle = arg(1); struct Resource *r = resource(handle);
    if (!s_device || !main_thread() || stage >= 4 || (handle && (!r || r->type != RESOURCE_TEXTURE))) {
        finish(8, (uint32_t)D3DERR_INVALIDCALL); return;
    }
    HRESULT result = 0;
    if (r) { r->dirty = 1; result = upload_texture(r); }
    if (result >= 0) result = s_device->lpVtbl->SetTexture(s_device, stage,
                                   r ? (IDirect3DBaseTexture8 *)r->texture : NULL);
    if (result >= 0 && handle != s_texture_handles[stage]) {
        if (r) { ++r->bindings; update_common(r); }
        struct Resource *old = resource(s_texture_handles[stage]);
        if (old) { --old->bindings; release_resource(old); }
        s_texture_handles[stage] = handle;
        write32(GUEST_DEVICE + 0xA78 + stage * 4, handle);
    }
    finish(8, (uint32_t)result);
}
void sub_00100D70(void) /* CreateVertexBuffer(length,usage,FVF,pool,out) */
{
    uint32_t size = arg(0), out = arg(4); struct Resource *r = new_resource();
    if (!s_device || !main_thread() || !size || size > 64 * 1024 * 1024 || !out || !r) {
        finish(20, (uint32_t)D3DERR_INVALIDCALL); return;
    }
    write32(out, 0); r->handle = xbox_HeapAlloc(12, 16); r->data = xbox_HeapAlloc(size, 128);
    HRESULT result = (!r->handle || !r->data) ? (HRESULT)0x8007000E :
        s_device->lpVtbl->CreateVertexBuffer(s_device, size, arg(1), arg(2), (D3DPOOL)arg(3), &r->vertex_buffer);
    if (result < 0) {
        if (r->handle) xbox_HeapFree(r->handle); if (r->data) xbox_HeapFree(r->data);
        memset(r, 0, sizeof(*r)); finish(20, (uint32_t)result); return;
    }
    r->type = RESOURCE_VERTEX_BUFFER; r->references = 1; r->bytes = size;
    memset(guest_ptr(r->data), 0, size);
    update_common(r); write32(r->handle + 4, r->data); write32(r->handle + 8, 0);
    write32(out, r->handle); finish(20, 0);
}
void sub_00100DD0(void) /* VertexBuffer_Lock(buffer,offset,size,out,flags) */
{
    struct Resource *r = resource(arg(0)); uint32_t offset = arg(1), size = arg(2), out = arg(3);
    if (!r || r->type != RESOURCE_VERTEX_BUFFER || !out || offset > r->bytes || size > r->bytes - offset) {
        finish(20, (uint32_t)D3DERR_INVALIDCALL); return;
    }
    write32(out, r->data + offset); finish(20, 0);
}
void sub_00102580(void)
{
    uint32_t stream = arg(0), handle = arg(1), stride = arg(2); struct Resource *r = resource(handle);
    if (!s_device || stream != 0 || (handle && (!r || r->type != RESOURCE_VERTEX_BUFFER || !stride))) {
        finish(12, (uint32_t)D3DERR_INVALIDCALL); return;
    }
    HRESULT result = s_device->lpVtbl->SetStreamSource(s_device, 0, r ? r->vertex_buffer : NULL, stride);
    if (result >= 0) {
        if (handle != s_stream_handle) {
            if (r) { ++r->bindings; update_common(r); }
            struct Resource *old = resource(s_stream_handle);
            if (old) { --old->bindings; release_resource(old); }
        }
        s_stream_handle = handle; s_stream_stride = stride;
        write32(0x10F280, stride); write32(0x10F288, handle);
        write32(0x10EC10, read32(0x10EC10) | 0x70);
    }
    finish(12, (uint32_t)result);
}
void sub_00102940(void) /* SetVertexShader: even FVF codes vs odd program handles. */
{
    uint32_t fvf = arg(0);
    if (!s_device || !main_thread() || (fvf & 1) ||
        ((fvf & 0xE) != D3DFVF_XYZ && (fvf & 0xE) != D3DFVF_XYZRHW)) {
        s_fvf = 0;
        fprintf(stderr, "[wrath graphics] unsupported vertex program/FVF 0x%X\n", fvf);
        finish(4, (uint32_t)D3DERR_INVALIDCALL); return;
    }
    s_fvf = fvf;
    finish(4, (uint32_t)s_device->lpVtbl->SetVertexShader(s_device, fvf));
}
static HRESULT draw_vertices(uint32_t type, uint32_t count, uint32_t data, uint32_t stride)
{
    if (!count) return 0;
    if (!s_device || !main_thread() || !data || !stride || stride > 1024 || count > 1024 * 1024 || (uint64_t)count * stride > 64 * 1024 * 1024 || !s_fvf)
        return D3DERR_INVALIDCALL;
    D3DPRIMITIVETYPE native; unsigned primitives; int quads = 0;
    switch (type) {
    case 1: native = D3DPT_POINTLIST; primitives = count; break;
    case 2: if (count % 2) return D3DERR_INVALIDCALL; native = D3DPT_LINELIST; primitives = count / 2; break;
    case 3: if (count < 2) return D3DERR_INVALIDCALL; native = D3DPT_LINESTRIP; primitives = count - 1; break;
    case 5: if (count % 3) return D3DERR_INVALIDCALL; native = D3DPT_TRIANGLELIST; primitives = count / 3; break;
    case 6: if (count < 3) return D3DERR_INVALIDCALL; native = D3DPT_TRIANGLESTRIP; primitives = count - 2; break;
    case 7: if (count < 3) return D3DERR_INVALIDCALL; native = D3DPT_TRIANGLEFAN; primitives = count - 2; break;
    case 8: if (count % 4) return D3DERR_INVALIDCALL; native = D3DPT_TRIANGLELIST; primitives = count / 2; quads = 1; break;
    default: return D3DERR_INVALIDCALL;
    }
    for (unsigned i = 0; i < 4; ++i) {
        struct Resource *r = resource(s_texture_handles[i]);
        if (r) { HRESULT result = upload_texture(r); if (result < 0) return result; }
    }
    const void *vertices = guest_ptr(data); void *converted = NULL;
    if (quads) {
        uint32_t output_count = count / 4 * 6;
        converted = malloc((size_t)output_count * stride);
        if (!converted) return (HRESULT)0x8007000E;
        const unsigned order[6] = {0, 1, 2, 0, 2, 3};
        for (unsigned quad = 0; quad < count / 4; ++quad)
            for (unsigned v = 0; v < 6; ++v)
                memcpy((uint8_t *)converted + (quad * 6 + v) * stride,
                       (const uint8_t *)vertices + (quad * 4 + order[v]) * stride, stride);
        count = output_count; vertices = converted;
    }
    if ((s_fvf & 0xE) == D3DFVF_XYZRHW) {
        if (stride < 16 || !s_viewport.Width || !s_viewport.Height) { free(converted); return D3DERR_INVALIDCALL; }
        if (!converted) {
            converted = malloc((size_t)count * stride);
            if (!converted) return (HRESULT)0x8007000E;
            memcpy(converted, vertices, (size_t)count * stride);
        }
        for (unsigned i = 0; i < count; ++i) {
            float p[4]; memcpy(p, (uint8_t *)converted + i * stride, sizeof(p));
            float w = p[3] != 0 ? 1.0f / p[3] : 1.0f;
            p[0] = (2 * (p[0] - s_viewport.X) / s_viewport.Width - 1) * w;
            p[1] = (1 - 2 * (p[1] - s_viewport.Y) / s_viewport.Height) * w;
            float depth_range = s_viewport.MaxZ - s_viewport.MinZ;
            float depth = depth_range != 0 ? (p[2] - s_viewport.MinZ) / depth_range : 0;
            p[2] = (2 * depth - 1) * w; p[3] = w;
            memcpy((uint8_t *)converted + i * stride, p, sizeof(p));
        }
        vertices = converted;
    }
    HRESULT result = s_device->lpVtbl->DrawPrimitiveUP(s_device, native, primitives, vertices, stride);
    free(converted); return result;
}
void sub_001019C0(void) /* DrawVerticesUP(type,vertexCount,data,stride) */
{
    finish(16, (uint32_t)draw_vertices(arg(0), arg(1), arg(2), arg(3)));
}
void sub_00101B20(void) /* DrawVertices(type,startVertex,vertexCount) */
{
    struct Resource *r = resource(s_stream_handle); uint32_t start = arg(1), count = arg(2);
    if (!r || !s_stream_stride || (uint64_t)(start + (uint64_t)count) * s_stream_stride > r->bytes) {
        finish(12, (uint32_t)D3DERR_INVALIDCALL); return;
    }
    finish(12, (uint32_t)draw_vertices(arg(0), count, r->data + start * s_stream_stride, s_stream_stride));
}

/* Xbox surfaces can refer to the live native drawable or to guest texture
 * storage. Framebuffer CopyRects always moves actual rendered pixels. */
static uint32_t s_backbuffer_handle, s_frontbuffer_handle;
void sub_000FF450(void) /* GetBackBuffer(index,type,out), ret12 */
{
    int32_t index = (int32_t)arg(0); uint32_t out = arg(2);
    if (!s_device || !main_thread() || !out || (index != 0 && index != -1) || arg(1) != 0) {
        finish(12, (uint32_t)D3DERR_INVALIDCALL); return;
    }
    uint32_t *slot = index == 0 ? &s_backbuffer_handle : &s_frontbuffer_handle;
    struct Resource *r = resource(*slot);
    if (!r) {
        r = new_resource();
        if (!r) { finish(12, 0x8007000E); return; }
        r->pitch = (s_width * 4 + 63) & ~63u; r->bytes = r->pitch * s_height;
        r->handle = xbox_HeapAlloc(24, 16); r->data = xbox_HeapAlloc(r->bytes, 128);
        if (!r->handle || !r->data) {
            if (r->handle) xbox_HeapFree(r->handle); if (r->data) xbox_HeapFree(r->data);
            memset(r, 0, sizeof(*r)); finish(12, 0x8007000E); return;
        }
        r->type = RESOURCE_SURFACE; r->format = 0x12; r->linear = 1;
        r->width = s_width; r->height = s_height; r->levels = 1; r->bindings = 1;
        r->framebuffer = index == 0 ? GL_BACK : GL_FRONT;
        r->sizes[0] = r->bytes; r->pitches[0] = r->pitch;
        update_common(r); write32(r->handle + 4, r->data); write32(r->handle + 8, 0);
        write32(r->handle + 12, 0x11221);
        write32(r->handle + 16, (r->width - 1) | ((r->height - 1) << 12) | ((r->pitch / 64 - 1) << 24));
        write32(r->handle + 20, 0); *slot = r->handle;
        write32(GUEST_DEVICE + (index == 0 ? 0x207C : 0x2080), r->handle);
        if (index == 0) write32(GUEST_DEVICE + 0x2070, r->handle);
    }
    ++r->references; update_common(r); write32(out, r->handle);
    finish(12, 0);
}
static uint32_t encode_color(uint32_t color, uint32_t format)
{
    unsigned a = color >> 24, r = (color >> 16) & 255, g = (color >> 8) & 255, b = color & 255;
    switch (format) {
    case 6: case 7: case 0x12: case 0x1E: return color;
    case 5: case 0x11: return (r * 31 / 255 << 11) | (g * 63 / 255 << 5) | (b * 31 / 255);
    case 2: case 3: case 0x10: case 0x1C:
        return ((a >= 128 || format == 3 || format == 0x1C) ? 0x8000 : 0) |
               (r * 31 / 255 << 10) | (g * 31 / 255 << 5) | (b * 31 / 255);
    case 4: case 0x1D: return (a >> 4 << 12) | (r >> 4 << 8) | (g >> 4 << 4) | (b >> 4);
    case 0: case 0x13: return (r + g + b) / 3;
    case 0x19: case 0x1F: return a;
    case 0x1A: case 0x20: return (a << 8) | (r + g + b) / 3;
    default: return 0;
    }
}
static HRESULT surface_pixels(struct Resource *r, uint32_t *pixels)
{
    if (r->framebuffer) {
        GLint old_fbo, old_buffer, old_pack, old_alignment, old_row, old_rows, old_pixels;
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &old_fbo);
        glGetIntegerv(GL_READ_BUFFER, &old_buffer);
        glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &old_pack);
        glGetIntegerv(GL_PACK_ALIGNMENT, &old_alignment);
        glGetIntegerv(GL_PACK_ROW_LENGTH, &old_row);
        glGetIntegerv(GL_PACK_SKIP_ROWS, &old_rows);
        glGetIntegerv(GL_PACK_SKIP_PIXELS, &old_pixels);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glReadBuffer(r->framebuffer); glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        glPixelStorei(GL_PACK_ALIGNMENT, 4); glPixelStorei(GL_PACK_ROW_LENGTH, 0);
        glPixelStorei(GL_PACK_SKIP_ROWS, 0); glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
        glReadPixels(0, 0, r->width, r->height, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, pixels);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, old_fbo); glReadBuffer(old_buffer);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, old_pack);
        glPixelStorei(GL_PACK_ALIGNMENT, old_alignment); glPixelStorei(GL_PACK_ROW_LENGTH, old_row);
        glPixelStorei(GL_PACK_SKIP_ROWS, old_rows); glPixelStorei(GL_PACK_SKIP_PIXELS, old_pixels);
        /* GL readback rows start at the bottom; Xbox memory rows start at top. */
        for (unsigned y = 0; y < r->height / 2; ++y)
            for (unsigned x = 0; x < r->width; ++x) {
                unsigned top = y * r->width + x, bottom = (r->height - 1 - y) * r->width + x;
                uint32_t swap = pixels[top]; pixels[top] = pixels[bottom]; pixels[bottom] = swap;
            }
        return 0;
    }
    int linear, bpp = format_info(r->format, &linear);
    if (bpp < 0) return D3DERR_INVALIDCALL;
    const uint8_t *data = guest_ptr(r->data);
    for (unsigned y = 0; y < r->height; ++y)
        for (unsigned x = 0; x < r->width; ++x) {
            uint32_t color;
            if (!bpp) {
                unsigned block = (y / 4) * ((r->width + 3) / 4) + x / 4;
                color = dxt_color(data + block * (r->format == 0x0C ? 8 : 16), (y % 4) * 4 + x % 4, r->format);
            } else {
                unsigned offset = linear ? y * r->pitch + x * bpp : morton_index(x,y,r->width,r->height) * bpp;
                uint32_t value = 0; memcpy(&value, data + offset, bpp); color = uncompressed_color(value, r->format);
            }
            pixels[y * r->width + x] = color;
        }
    return 0;
}
static HRESULT pixels_to_framebuffer(struct Resource *destination, const uint32_t *pixels,
                                     unsigned width, unsigned height, int x, int y)
{
    GLint old_read, old_draw, old_texture, old_unpack, old_alignment, old_row, old_rows, old_pixels;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &old_read); glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &old_draw);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &old_texture); glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &old_unpack);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &old_alignment); glGetIntegerv(GL_UNPACK_ROW_LENGTH, &old_row);
    glGetIntegerv(GL_UNPACK_SKIP_ROWS, &old_rows); glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &old_pixels);
    GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);
    GLuint texture, framebuffer; glGenTextures(1, &texture); glBindTexture(GL_TEXTURE_2D, texture);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0); glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0); glPixelStorei(GL_UNPACK_SKIP_ROWS, 0); glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, pixels);
    glGenFramebuffers(1, &framebuffer); glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    HRESULT result = glCheckFramebufferStatus(GL_READ_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE ? 0 : D3DERR_INVALIDCALL;
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    GLint old_draw_buffer; glGetIntegerv(GL_DRAW_BUFFER, &old_draw_buffer); glDrawBuffer(destination->framebuffer);
    if (result >= 0) {
        glDisable(GL_SCISSOR_TEST);
        glBlitFramebuffer(0, 0, width, height, x, destination->height - y,
                          x + width, destination->height - y - height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }
    glDrawBuffer(old_draw_buffer);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, old_read); glBindFramebuffer(GL_DRAW_FRAMEBUFFER, old_draw);
    glBindTexture(GL_TEXTURE_2D, old_texture); glBindBuffer(GL_PIXEL_UNPACK_BUFFER, old_unpack);
    glPixelStorei(GL_UNPACK_ALIGNMENT, old_alignment); glPixelStorei(GL_UNPACK_ROW_LENGTH, old_row);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, old_rows); glPixelStorei(GL_UNPACK_SKIP_PIXELS, old_pixels);
    if (scissor) glEnable(GL_SCISSOR_TEST);
    glDeleteFramebuffers(1, &framebuffer); glDeleteTextures(1, &texture);
    return result;
}
void sub_000FF580(void) /* CopyRects(source,rectangles,count,destination,points), ret20 */
{
    struct Resource *source = resource(arg(0)), *destination = resource(arg(3));
    uint32_t rectangles = arg(1), count = arg(2), points = arg(4);
    int linear, bpp = destination ? format_info(destination->format, &linear) : -1;
    if (!s_device || !main_thread() || !source || !destination || source->type != RESOURCE_SURFACE ||
        destination->type != RESOURCE_SURFACE || bpp <= 0 || count > 65536 ||
        (uint64_t)source->width * source->height > 16 * 1024 * 1024) {
        fprintf(stderr, "[wrath graphics] unsupported CopyRects surfaces 0x%X -> 0x%X\n", arg(0), arg(3));
        finish(20, (uint32_t)D3DERR_INVALIDCALL); return;
    }
    if (!count) count = 1;
    uint32_t *pixels = malloc((size_t)source->width * source->height * 4);
    if (!pixels) { finish(20, 0x8007000E); return; }
    HRESULT result = surface_pixels(source, pixels);
    for (unsigned i = 0; result >= 0 && i < count; ++i) {
        D3DRECT rect = {0,0,(LONG)source->width,(LONG)source->height};
        int32_t point[2];
        if (rectangles) memcpy(&rect, guest_ptr(rectangles + i * sizeof(rect)), sizeof(rect));
        point[0] = rect.x1; point[1] = rect.y1;
        if (points) memcpy(point, guest_ptr(points + i * sizeof(point)), sizeof(point));
        int64_t width = (int64_t)rect.x2 - rect.x1, height = (int64_t)rect.y2 - rect.y1;
        if (rect.x1 < 0 || rect.y1 < 0 || width < 0 || height < 0 || rect.x2 > (int64_t)source->width ||
            rect.y2 > (int64_t)source->height || point[0] < 0 || point[1] < 0 ||
            point[0] + width > destination->width || point[1] + height > destination->height) {
            result = D3DERR_INVALIDCALL; break;
        }
        if (!width || !height) continue;
        if (destination->framebuffer) {
            uint32_t *region = malloc((size_t)width * height * 4);
            if (!region) { result = (HRESULT)0x8007000E; break; }
            for (int64_t y = 0; y < height; ++y)
                memcpy(region + y * width, pixels + (rect.y1 + y) * source->width + rect.x1, (size_t)width * 4);
            result = pixels_to_framebuffer(destination, region, (unsigned)width, (unsigned)height, point[0], point[1]);
            free(region);
        } else {
            uint8_t *data = guest_ptr(destination->data);
            for (int64_t y = 0; y < height; ++y)
                for (int64_t x = 0; x < width; ++x) {
                    uint32_t color = encode_color(pixels[(rect.y1 + y) * source->width + rect.x1 + x], destination->format);
                    uint32_t dx = point[0] + x, dy = point[1] + y;
                    uint32_t offset = linear ? dy * destination->pitch + dx * bpp :
                        morton_index(dx,dy,destination->width,destination->height) * bpp;
                    memcpy(data + offset, &color, bpp);
                }
            struct Resource *parent = resource(destination->owner);
            if (parent) parent->dirty = 1;
        }
    }
    free(pixels); finish(20, (uint32_t)result);
}

recomp_func_t wrath_graphics_lookup(uint32_t address)
{
    switch (address) {
    case 0x000FD6E0: return sub_000FD6E0;
    case 0x000FF450: return sub_000FF450;
    case 0x000FF580: return sub_000FF580;
    case 0x000FF860: return sub_000FF860;
    case 0x00100EA0: return sub_00100EA0;
    case 0x00100C40: return sub_00100C40;
    case 0x000FE9C0: return sub_000FE9C0;
    case 0x00103C80: return sub_00103C80;
    case 0x00103C20: return sub_00103C20;
    case 0x00103C30: return sub_00103C30;
    case 0x00103DD0: return sub_00103DD0;
    case 0x00103A90: return sub_00103A90;
    case 0x00103AD0: return sub_00103AD0;
    case 0x000FFC90: return sub_000FFC90;
    case 0x00100D70: return sub_00100D70;
    case 0x00100DD0: return sub_00100DD0;
    case 0x00102580: return sub_00102580;
    case 0x00102940: return sub_00102940;
    case 0x001019C0: return sub_001019C0;
    case 0x00101B20: return sub_00101B20;
    default: return NULL;
    }
}

#ifdef WRATH_GRAPHICS_SMOKE_TEST
/* Standalone native rendering/guest ABI test; no ISO or game assets needed. */
#include <assert.h>
#include <stdlib.h>
_Thread_local uint32_t g_eax, g_esp;
ptrdiff_t g_xbox_mem_offset;
static uint32_t test_heap = 0x180000;
uint32_t xbox_HeapAlloc(uint32_t size, uint32_t alignment)
{
    test_heap = (test_heap + alignment - 1) & ~(alignment - 1);
    if (size > 0x200000 - test_heap) return 0;
    uint32_t result = test_heap; test_heap += size; return result;
}
void xbox_HeapFree(uint32_t address) { (void)address; }
static void call(uint32_t address, const uint32_t *args, unsigned n)
{
    g_esp = 0x1000;
    write32(g_esp, 0xABCDEF00);
    for (unsigned i = 0; i < n; ++i) write32(g_esp + 4 + i * 4, args[i]);
    recomp_func_t function = wrath_graphics_lookup(address);
    assert(function);
    function();
    assert(g_esp == 0x1000 + 4 + n * 4);
}
int main(void)
{
    /* Independent decoder facts: Morton order and canonical BC endpoints. */
    assert(morton_index(2,1,4,4) == 6 && morton_index(3,3,4,4) == 15);
    const uint8_t dxt1[8] = {0,0xF8,0x1F,0,0,0,0,0};
    assert(dxt_color(dxt1, 0, 0x0C) == 0xFFFF0000);
    const uint8_t transparent[8] = {0,0,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    assert(dxt_color(transparent, 0, 0x0C) == 0);
    assert(uncompressed_color(0x7E0, 5) == 0xFF00FF00);
    void *memory = calloc(1, 0x200000);
    assert(memory);
    g_xbox_mem_offset = (ptrdiff_t)memory;
    GuestPresentation pp = {0};
    pp.width = 320; pp.height = 240; pp.format = 6; pp.buffer_count = 2;
    pp.multisample = 0x11; pp.swap_effect = 1; pp.depth_enabled = 1;
    pp.depth_format = 0x2A; pp.interval = 1;
    memcpy(guest_ptr(0x2000), &pp, sizeof(pp));
    const uint32_t create[] = {0, 1, 0, 0x40, 0x2000, 0x3000};
    call(0xFD6E0, create, 6);
    assert(g_eax == 0 && read32(0x3000) == GUEST_DEVICE);
    assert(read32(GUEST_DEVICE_GLOBAL) == GUEST_DEVICE);
    uint32_t clear[] = {0, 0, 0xF3, 0xFF204080, 0x3F800000, 0};
    call(0x100EA0, clear, 6);
    assert(g_eax == 0);
    GLubyte pixel[4];
    glReadPixels(1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    assert(pixel[0] == 0x20 && pixel[1] == 0x40 && pixel[2] == 0x80);
    /* Partial channel clear preserves green/blue. */
    clear[2] = 0x10; clear[3] = 0xFFAABBCC;
    call(0x100EA0, clear, 6);
    glReadPixels(1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    assert(pixel[0] == 0xAA && pixel[1] == 0x40 && pixel[2] == 0x80);
    D3DVIEWPORT8 vp = {100, 100, 20, 20, 0, 1};
    memcpy(guest_ptr(0x4000), &vp, sizeof(vp));
    uint32_t viewport[] = {0x4000};
    call(0xFF860, viewport, 1);
    assert(read32(GUEST_VIEWPORT) == 100 && read32(GUEST_VIEWPORT + 8) == 20);
    clear[2] = 0xF0; clear[3] = 0xFF001122;
    call(0x100EA0, clear, 6);
    glReadPixels(105, 240 - 105, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    assert(pixel[0] == 0 && pixel[1] == 0x11 && pixel[2] == 0x22);
    glReadPixels(1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    assert(pixel[0] == 0xAA && pixel[1] == 0x40 && pixel[2] == 0x80);
    /* Serialize a guest texture, populate its swizzled storage, and draw a
     * textured screen-space quad through the guest SDK entry points. */
    vp = (D3DVIEWPORT8){0, 0, 320, 240, 0, 1};
    memcpy(guest_ptr(0x4000), &vp, sizeof(vp)); call(0xFF860, viewport, 1);
    uint32_t create_texture[] = {2, 2, 1, 0, 6, 0, 0x5000};
    call(0xFE9C0, create_texture, 7); assert(g_eax == 0);
    uint32_t texture = read32(0x5000);
    assert(read32(texture) == 0x1040001 && ((read32(texture + 12) >> 8) & 255) == 6);
    uint32_t lock_texture[] = {texture, 0, 0x5010, 0, 0};
    call(0x103C80, lock_texture, 5); assert(g_eax == 0 && read32(0x5010) == 8);
    uint32_t pixels = read32(0x5014);
    for (unsigned i = 0; i < 4; ++i) write32(pixels + i * 4, 0xFF30C060);
    uint32_t bind[] = {0, texture}; call(0xFFC90, bind, 2); assert(g_eax == 0);
    uint32_t shader[] = {D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1};
    call(0x102940, shader, 1); assert(g_eax == 0);
    struct { float x,y,z,rhw; uint32_t color; float u,v; } vertices[4] = {
        {40,40,0.5f,1,0xFFFFFFFF,0,0}, {80,40,0.5f,1,0xFFFFFFFF,1,0},
        {80,80,0.5f,1,0xFFFFFFFF,1,1}, {40,80,0.5f,1,0xFFFFFFFF,0,1}
    };
    memcpy(guest_ptr(0x6000), vertices, sizeof(vertices));
    s_device->lpVtbl->SetRenderState(s_device, D3DRS_CULLMODE, D3DCULL_NONE);
    s_device->lpVtbl->SetRenderState(s_device, D3DRS_ZENABLE, 0);
    uint32_t draw[] = {8, 4, 0x6000, sizeof(vertices[0])};
    call(0x1019C0, draw, 4); assert(g_eax == 0);
    glReadPixels(60, 240 - 60, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    assert(pixel[0] == 0x30 && pixel[1] == 0xC0 && pixel[2] == 0x60);
    uint32_t surface_args[] = {texture, 0, 0x5020};
    call(0x103C30, surface_args, 3); assert(g_eax == 0);
    uint32_t surface = read32(0x5020), release_surface[] = {surface};
    assert(read32(surface + 4) == pixels && read32(surface + 20) == texture);
    call(0x103AD0, release_surface, 1); assert(!resource(surface));
    uint32_t ref[] = {texture}; call(0x103A90, ref, 1); assert(g_eax == 2);
    call(0x103AD0, ref, 1); assert(g_eax == 1);
    call(0x103AD0, ref, 1); assert(g_eax == 0 && resource(texture));
    bind[1] = 0; call(0xFFC90, bind, 2); assert(!resource(texture));
    uint32_t create_vb[] = {sizeof(vertices),0,shader[0],0,0x7000};
    call(0x100D70, create_vb, 5); assert(g_eax == 0);
    uint32_t vb = read32(0x7000), lock_vb[] = {vb,0,sizeof(vertices),0x7010,0};
    call(0x100DD0, lock_vb, 5); assert(g_eax == 0);
    memcpy(guest_ptr(read32(0x7010)), vertices, sizeof(vertices));
    uint32_t stream[] = {0,vb,sizeof(vertices[0])}; call(0x102580, stream, 3); assert(g_eax == 0);
    uint32_t draw_vb[] = {8,0,4}; call(0x101B20, draw_vb, 3); assert(g_eax == 0);
    glReadPixels(60, 240 - 60, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    assert(pixel[0] == 255 && pixel[1] == 255 && pixel[2] == 255);
    ref[0] = vb; call(0x103AD0, ref, 1); stream[1] = 0; call(0x102580, stream, 3);
    assert(!resource(vb));
    /* Capture real framebuffer pixels into a guest texture surface, preserving
     * top/bottom orientation, then restore them through native framebuffer blit. */
    clear[0] = 0; clear[1] = 0; clear[2] = 0xF0; clear[3] = 0xFF102030;
    call(0x100EA0, clear, 6);
    D3DRECT band = {0,0,16,4}; memcpy(guest_ptr(0x8000), &band, sizeof(band));
    clear[0] = 1; clear[1] = 0x8000; clear[3] = 0xFFB04020; call(0x100EA0, clear, 6);
    uint32_t get_back[] = {0,0,0x8100}; call(0xFF450, get_back, 3); assert(g_eax == 0);
    uint32_t back = read32(0x8100); assert(read32(GUEST_DEVICE + 0x207C) == back);
    uint32_t capture_texture_args[] = {16,8,1,0,0x12,0,0x8110};
    call(0xFE9C0, capture_texture_args, 7); assert(g_eax == 0);
    uint32_t capture_texture = read32(0x8110), capture_surface_args[] = {capture_texture,0,0x8120};
    call(0x103C30, capture_surface_args, 3); assert(g_eax == 0);
    uint32_t capture_surface = read32(0x8120);
    D3DRECT area = {0,0,16,8}; memcpy(guest_ptr(0x8200), &area, sizeof(area));
    uint32_t copy[] = {back,0x8200,1,capture_surface,0};
    glPixelStorei(GL_PACK_ROW_LENGTH, 19);
    call(0xFF580, copy, 5); assert(g_eax == 0);
    GLint restored_pack; glGetIntegerv(GL_PACK_ROW_LENGTH, &restored_pack); assert(restored_pack == 19);
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    uint32_t capture_data = read32(capture_surface + 4);
    assert(read32(capture_data) == 0xFFB04020 && read32(capture_data + 7 * 64) == 0xFF102030);
    int32_t destination_point[] = {20,30}; memcpy(guest_ptr(0x8300), destination_point, sizeof(destination_point));
    copy[0] = capture_surface; copy[1] = 0; copy[2] = 0; copy[3] = back; copy[4] = 0x8300;
    call(0xFF580, copy, 5); assert(g_eax == 0);
    glReadPixels(20,240-31,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
    assert(pixel[0] == 0xB0 && pixel[1] == 0x40 && pixel[2] == 0x20);
    glReadPixels(20,240-38,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
    assert(pixel[0] == 0x10 && pixel[1] == 0x20 && pixel[2] == 0x30);
    ref[0] = capture_surface; call(0x103AD0, ref, 1);
    ref[0] = capture_texture; call(0x103AD0, ref, 1);
    assert(!resource(capture_surface) && !resource(capture_texture));
    ref[0] = back; call(0x103AD0, ref, 1); assert(g_eax == 0 && resource(back));
    uint32_t swap[] = {0};
    call(0x100C40, swap, 1);
    assert(g_eax == 1 && read32(GUEST_SWAP_COUNT) == 1);
    swap[0] = 1;
    call(0x100C40, swap, 1);
    assert(g_eax == 1);
    assert(glGetError() == GL_NO_ERROR);
    assert(wrath_graphics_lookup(0xDEADBEEF) == NULL);
    puts("PASS: native GL, clears, guest ABI, texture/quad, vertex buffer, lifetime, framebuffer copies, swap");
    SDL_Quit();
    free(memory);
    return 0;
}

#endif
