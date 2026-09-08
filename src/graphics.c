/* Initial Xbox 4361 -> native OpenGL bridge. See docs/D3D-INTEGRATION.md.
 * Compiled host SDK replacements serialize GPU/resource access across game
 * threads; SDL window creation and event handling stay on the Cocoa main thread. */
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

extern _Thread_local uint32_t g_eax, g_esp, g_ecx, g_edx;
extern ptrdiff_t g_xbox_mem_offset;
typedef void (*recomp_func_t)(void);
#ifdef WRATH_GRAPHICS_SMOKE_TEST
static void wrath_vblank_set_refresh(uint32_t hz) { (void)hz; }
static void wrath_vblank_notify_swap(void) {}
#else
extern void wrath_vblank_set_refresh(uint32_t hz);
extern void wrath_vblank_notify_swap(void);
#endif

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
static UINT s_width, s_height, s_target_width, s_target_height;
static int s_depth_available;
static HRESULT initialize_depth_surface(uint32_t format);
static void initialize_render_defaults(void);
static void update_viewport_constants(void);
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
extern int xbox_D3D8GLAcquire(void);
extern void xbox_D3D8GLRelease(void);
extern void xbox_D3D8GLPumpEvents(void);
static _Thread_local int s_graphics_held;
/* arg() can be evaluated repeatedly inside one SDK entry. Acquire exactly once;
 * helpers do not call back into guest SDK entries before finish(). */
static int graphics_thread(void)
{
    if (!s_graphics_held) {
        if (!xbox_D3D8GLAcquire()) {
            fprintf(stderr,"[wrath graphics] failed native graphics context acquisition\n");
            return 0;
        }
        s_graphics_held=1;
        xbox_D3D8GLPumpEvents();
    }
    return 1;
}
static uint32_t arg(unsigned index)
{
    if (!graphics_thread()) abort();
    return read32(g_esp + 4 + index * 4);
}
static void finish(unsigned bytes, uint32_t result)
{
    g_eax=result;
    g_esp+=4+bytes;
    if (s_graphics_held) { s_graphics_held=0; xbox_D3D8GLRelease(); }
}
static int window_thread(void)
{
#ifdef __APPLE__
    return pthread_main_np()!=0;
#else
    return 1;
#endif
}

#include "graphics_timing_probe.inc"

/* HRESULT WINAPI Direct3D_CreateDevice(adapter,type,window,flags,pp,out). */
void sub_000FD6E0(void)
{
    uint32_t pp_address = arg(4), output = arg(5), behavior = arg(3);
    if (!graphics_thread() || !window_thread() || !pp_address || !output || s_device) {
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
    s_depth_available = pp.EnableAutoDepthStencil != 0;
    s_width = pp.BackBufferWidth;
    s_height = pp.BackBufferHeight;
    s_target_width = s_width; s_target_height = s_height;
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
    /* Single-sample native targets use the retail unit raster scale. Viewport
     * methods alias physical shader constants58/59; initialize their inputs. */
    const float depth_scale=16777215.0f, raster_scale[2]={1.0f,1.0f};
    const float screen_offset[2]={0.53125f,0.53125f}; /* 1001D0 + XBE10B000. */
    memcpy(guest_ptr(GUEST_DEVICE+0x450),&depth_scale,sizeof(depth_scale));
    memcpy(guest_ptr(GUEST_DEVICE+0x458),raster_scale,sizeof(raster_scale));
    memcpy(guest_ptr(GUEST_DEVICE+0x9E8),screen_offset,sizeof(screen_offset));
    update_viewport_constants();
    /* Retail SetTransform FEA20 writes ten row-major matrices at device+750.
     * The original CreateDevice initializes these to identity. */
    for (unsigned transform=0; transform<10; ++transform)
        for (unsigned element=0; element<16; ++element)
            write32(GUEST_DEVICE+0x750+transform*64+element*4,
                    element%5==0 ? 0x3F800000u : 0);
    /* Retail initializer104330 reads this table from the loaded title image.
     * FE170 caches each DWORD; later game calls change only selected states. */
    const uint8_t *texture_defaults = guest_ptr(0x10BD9C);
    for (unsigned stage=0; stage<4; ++stage) {
        for (unsigned state=0; state<32; ++state)
            write32(0x10EC18+stage*128+state*4, texture_defaults[state]);
        write32(0x10EC18+stage*128+28*4, stage);
    }
    write32(0x10EC18+12*4, 4); /* stage0 MODULATE */
    write32(0x10EC18+16*4, 2); /* stage0 SELECTARG1 */
    if (s_depth_available && initialize_depth_surface(in.depth_format)<0) {
        fprintf(stderr,"[wrath graphics] failed native depth surface initialization\n"); abort();
    }
    initialize_render_defaults();
    write32(output, GUEST_DEVICE);
    wrath_vblank_set_refresh(in.refresh_hz);
    fprintf(stderr, "[wrath graphics] native device %ux%u, guest handle 0x%08X\n",
            s_width, s_height, GUEST_DEVICE);
    finish(24, 0);
}


/* The 4361 SetRenderStateSimple SDK helper uses ECX=single NV097 method
 * packet and EDX=value, not stack arguments. Translate identified API state
 * commands directly; do not acknowledge a fictitious GPU/pushbuffer. */
static void state_error(uint32_t method, uint32_t value)
{
    fprintf(stderr, "[wrath graphics] unsupported render state method 0x%X value 0x%X, guest return 0x%X\n",
            method, value, read32(g_esp));
    abort();
}
static void native_state(D3DRENDERSTATETYPE state, uint32_t value)
{
    if (s_device->lpVtbl->SetRenderState(s_device, state, value) < 0)
        state_error(state, value);
}
static void gl_toggle(GLenum capability, uint32_t enabled)
{
    if (enabled) glEnable(capability); else glDisable(capability);
}
static uint32_t blend_factor(uint32_t value)
{
    switch (value) {
    case 0: return D3DBLEND_ZERO; case 1: return D3DBLEND_ONE;
    case 0x300: return D3DBLEND_SRCCOLOR; case 0x301: return D3DBLEND_INVSRCCOLOR;
    case 0x302: return D3DBLEND_SRCALPHA; case 0x303: return D3DBLEND_INVSRCALPHA;
    case 0x304: return D3DBLEND_DESTALPHA; case 0x305: return D3DBLEND_INVDESTALPHA;
    case 0x306: return D3DBLEND_DESTCOLOR; case 0x307: return D3DBLEND_INVDESTCOLOR;
    case 0x308: return D3DBLEND_SRCALPHASAT;
    default: return 0;
    }
}
static int compare_valid(uint32_t value) { return value >= GL_NEVER && value <= GL_ALWAYS; }
static int stencil_valid(uint32_t value)
{
    return value == GL_KEEP || value == GL_ZERO || value == GL_REPLACE ||
           value == GL_INCR || value == GL_DECR || value == GL_INVERT ||
           value == GL_INCR_WRAP || value == GL_DECR_WRAP;
}
static GLenum s_stencil_fail = GL_KEEP, s_stencil_zfail = GL_KEEP, s_stencil_pass = GL_KEEP;
static GLenum s_stencil_func = GL_ALWAYS;
static uint32_t s_stencil_ref, s_stencil_mask = ~0u;
static float s_offset_scale, s_offset_bias;
/* Original 4361 initializer104330 copies states57..145 from10BDC0,
 * except the two reserved slots, then derives ZENABLE from its depth surface.
 * Copy the guest cache without executing unsupported hardware-writing setters.
 * Only synchronize native state whose semantics this bridge already supports. */
static void initialize_render_defaults(void)
{
    for (unsigned state=57; state<=145; ++state) {
        if (state==116 || state==135) continue;
        uint32_t value=state==124 ? (uint32_t)(s_depth_available!=0) :
            read32(0x10BDC0+(state-57)*4);
        write32(0x10EE18+state*4,value);
    }
    uint32_t zfunc=read32(0x10EEFC), afunc=read32(0x10EF00);
    uint32_t src=read32(0x10EF10), dst=read32(0x10EF14);
    if (!compare_valid(zfunc) || !compare_valid(afunc) ||
        !blend_factor(src) || !blend_factor(dst)) state_error(0x10BDC0,zfunc);
    native_state(D3DRS_ZFUNC,zfunc-GL_NEVER+1);
    native_state(D3DRS_ALPHAFUNC,afunc-GL_NEVER+1);
    native_state(D3DRS_SRCBLEND,blend_factor(src));
    native_state(D3DRS_DESTBLEND,blend_factor(dst));
    native_state(D3DRS_ZENABLE,s_depth_available!=0);
    native_state(D3DRS_TEXTUREFACTOR,read32(0x10F01C));
    glDepthFunc(zfunc);
    gl_toggle(GL_DEPTH_TEST,s_depth_available!=0);
    glBlendFunc(src,dst);
    gl_toggle(GL_DITHER,read32(0x10EF1C));
    /* Other native defaults already agree (write mask, disabled blending,
     * alpha/stencil tests, LEQUAL, smooth fill). Preserve measured cull winding:
     * guest CULLMODE=GL_CCW maps to native D3DCULL_CCW with GL_FRONTFACE=GL_CW.
     * Guest lighting/material defaults are mirrored, not claimed implemented. */
}
void sub_000FD830(void)
{
    uint32_t packet = g_ecx, value = g_edx, method = packet & 0xFFFF;
    unsigned guest_state = 0;
    if (!s_device || !graphics_thread() || (packet & 0xFFFF0000) != 0x40000)
        state_error(packet, value);
    switch (method) {
    case 0x354: /* ZFUNC, GL enum on Xbox vs D3DCMP enum in host API. */
        if (!compare_valid(value)) state_error(method,value);
        native_state(D3DRS_ZFUNC,value-GL_NEVER+1); glDepthFunc(value); guest_state=57; break;
    case 0x33C:
        if (!compare_valid(value)) state_error(method,value);
        native_state(D3DRS_ALPHAFUNC,value-GL_NEVER+1); guest_state=58; break;
    case 0x304:
        native_state(D3DRS_ALPHABLENDENABLE,value!=0); gl_toggle(GL_BLEND,value); guest_state=59; break;
    case 0x300:
        native_state(D3DRS_ALPHATESTENABLE,value!=0); guest_state=60; break;
    case 0x340: native_state(D3DRS_ALPHAREF,value&255); guest_state=61; break;
    case 0x344: case 0x348: {
        uint32_t factor=blend_factor(value);
        if (!factor) state_error(method,value);
        native_state(method==0x344?D3DRS_SRCBLEND:D3DRS_DESTBLEND,factor);
        GLint old; glGetIntegerv(method==0x344?GL_BLEND_DST_RGB:GL_BLEND_SRC_RGB,&old);
        glBlendFunc(method==0x344?value:(GLenum)old,method==0x348?value:(GLenum)old);
        guest_state=method==0x344?62:63; break;
    }
    case 0x35C:
        native_state(D3DRS_ZWRITEENABLE,value!=0); glDepthMask(value!=0); guest_state=64; break;
    case 0x310: gl_toggle(GL_DITHER,value); guest_state=65; break;
    case 0x37C:
        if (value!=GL_SMOOTH && value!=GL_FLAT) state_error(method,value);
        native_state(D3DRS_SHADEMODE,value==GL_FLAT?1:2); guest_state=66; break;
    case 0x358: {
        uint32_t mask=((value>>16)&1)|(((value>>8)&1)<<1)|((value&1)<<2)|(((value>>24)&1)<<3);
        native_state(D3DRS_COLORWRITEENABLE,mask);
        glColorMask(mask&1,mask&2,mask&4,mask&8); guest_state=67; break;
    }
    case 0x374: case 0x378:
        if (!stencil_valid(value)) state_error(method,value);
        if (method==0x374) s_stencil_zfail=value; else s_stencil_pass=value;
        glStencilOp(s_stencil_fail,s_stencil_zfail,s_stencil_pass);
        guest_state=method==0x374?68:69; break;
    case 0x364:
        if (!compare_valid(value)) state_error(method,value);
        s_stencil_func=value; guest_state=70; goto stencil_function;
    case 0x368: s_stencil_ref=value; guest_state=71; goto stencil_function;
    case 0x36C: s_stencil_mask=value; guest_state=72;
    stencil_function:
        glStencilFunc(s_stencil_func,(GLint)s_stencil_ref,s_stencil_mask); break;
    case 0x360: glStencilMask(value); guest_state=73; break;
    case 0x350:
        if (value!=GL_FUNC_ADD && value!=GL_FUNC_SUBTRACT && value!=GL_FUNC_REVERSE_SUBTRACT && value!=GL_MIN && value!=GL_MAX)
            state_error(method,value);
        glBlendEquation(value); guest_state=74; break;
    case 0x34C:
        glBlendColor(((value>>16)&255)/255.0f,((value>>8)&255)/255.0f,(value&255)/255.0f,(value>>24)/255.0f);
        guest_state=75; break;
    case 0x384: case 0x388: {
        float f; memcpy(&f,&value,4); if (!isfinite(f)) state_error(method,value);
        if (method==0x384) s_offset_scale=f; else s_offset_bias=f;
        glPolygonOffset(s_offset_scale,s_offset_bias); guest_state=method==0x384?77:78; break;
    }
    case 0x330: gl_toggle(GL_POLYGON_OFFSET_POINT,value); guest_state=79; break;
    case 0x334: gl_toggle(GL_POLYGON_OFFSET_LINE,value); guest_state=80; break;
    case 0x338: gl_toggle(GL_POLYGON_OFFSET_FILL,value); guest_state=81; break;
    default: state_error(method,value);
    }
    /* Callers also mirror this cache. Keep direct SDK helper calls coherent. */
    write32(0x10EE18+guest_state*4,value);
    finish(0,0);
}
void sub_000FDAD0(void) /* SetRenderState_CullMode(value), Xbox 0 / GL_CW / GL_CCW. */
{
    uint32_t value=arg(0);
    if (!s_device || !graphics_thread() || (value && value!=GL_CW && value!=GL_CCW)) state_error(0x39C,value);
    native_state(D3DRS_CULLMODE,value==0?D3DCULL_NONE:value==GL_CW?D3DCULL_CW:D3DCULL_CCW);
    gl_toggle(GL_CULL_FACE,value); glCullFace(GL_BACK); glFrontFace(value==GL_CW?GL_CCW:GL_CW);
    write32(0x10F018,value); finish(4,0);
}
void sub_000FDB40(void) /* SetRenderState_FrontFace(value). */
{
    uint32_t value=arg(0);
    if (!s_device || !graphics_thread() || (value!=GL_CW && value!=GL_CCW)) state_error(0x3A0,value);
    /* CullMode identifies the removed winding. The original setter recalculates
     * GL_FRONT/GL_BACK to preserve that winding when FrontFace changes. */
    write32(0x10F014,value); finish(4,0);
}

void sub_000FDDF0(void) /* SetRenderState_FillMode(GL_POINT/GL_LINE/GL_FILL), ret4. */
{
    uint32_t value=arg(0);
    if (!s_device || !graphics_thread() || (value!=GL_POINT && value!=GL_LINE && value!=GL_FILL)) state_error(0x38C,value);
    /* GL core supports a single polygon mode for both faces. */
    if (read32(0x10F000) && read32(0x10EFFC)!=value) state_error(0x390,read32(0x10EFFC));
    native_state(D3DRS_FILLMODE,value-GL_POINT+D3DFILL_POINT);
    glPolygonMode(GL_FRONT_AND_BACK,value);
    write32(0x10EFF8,value); finish(4,0);
}
void sub_000FDF60(void) /* SetTextureStageState_TexCoordIndex(stage,value), ret8. */
{
    uint32_t stage=arg(0), value=arg(1);
    /* Explicit coordinate sets pass through; programmed vertex shaders supply
     * their own oT0..oT3 values. Generated fixed coordinates remain unsupported. */
    if (!s_device || !graphics_thread() || stage>=4 || value>=8) state_error(0x1964+stage*4,value);
    HRESULT result=s_device->lpVtbl->SetTextureStageState(s_device,stage,D3DTSS_TEXCOORDINDEX,value);
    if (result<0) state_error(0x1964+stage*4,value);
    write32(0x10EC88+stage*128,value);
    *(uint8_t *)guest_ptr(0x1B11D1+stage)=(uint8_t)(value+9);
    write32(GUEST_DEVICE+0x454,read32(GUEST_DEVICE+0x454)&~(1u<<stage));
    write32(0x10EC10,read32(0x10EC10)|0x47F);
    finish(8,0);
}


void sub_000FE660(void) /* SetRenderState_StencilEnable(value), ret4. */
{
    uint32_t value=arg(0);
    if (!s_device || !graphics_thread()) state_error(0x32C,value);
    /* Original SDK also updates early-Z optimizations; native GL owns those. */
    gl_toggle(GL_STENCIL_TEST,value&&s_depth_available);
    native_state(D3DRS_STENCILENABLE,value!=0);
    write32(0x10F00C,value); finish(4,0);
}
void sub_000FE6F0(void) /* SetRenderState_StencilFail(GL stencil op), ret4. */
{
    uint32_t value=arg(0);
    if (!s_device || !graphics_thread() || !stencil_valid(value)) state_error(0x370,value);
    s_stencil_fail=value;
    glStencilOp(s_stencil_fail,s_stencil_zfail,s_stencil_pass);
    write32(0x10F010,value); finish(4,0);
}

void sub_000FE5C0(void) /* SetRenderState_ZEnable(value), 0=off / 1=Z / 2=W. */
{
    uint32_t value=arg(0);
    if (!s_device || !graphics_thread() || value>1) state_error(0x30C,value);
    native_state(D3DRS_ZENABLE,value&&s_depth_available);
    gl_toggle(GL_DEPTH_TEST,value&&s_depth_available);
    write32(0x10F008,value); finish(4,0);
}

/* DWORD/void WINAPI SetViewport(const XboxViewport*), same 24-byte layout. */
void sub_000FF860(void)
{
    uint32_t address = arg(0);
    D3DVIEWPORT8 vp;
    if (!s_device || !graphics_thread() || !address) {
        finish(4, (uint32_t)D3DERR_INVALIDCALL);
        return;
    }
    memcpy(&vp, guest_ptr(address), sizeof(vp));
    /* SDK clamps the viewport to its currently bound render target. */
    if (vp.X > s_target_width) vp.X = s_target_width;
    if (vp.Y > s_target_height) vp.Y = s_target_height;
    if (vp.Width > s_target_width - vp.X) vp.Width = s_target_width - vp.X;
    if (vp.Height > s_target_height - vp.Y) vp.Height = s_target_height - vp.Y;
    if (!isfinite(vp.MinZ) || !isfinite(vp.MaxZ)) {
        finish(4, (uint32_t)D3DERR_INVALIDCALL);
        return;
    }
    HRESULT result = s_device->lpVtbl->SetViewport(s_device, &vp);
    if (result >= 0) {
        s_viewport = vp;
        glViewport(vp.X, s_target_height - vp.Y - vp.Height, vp.Width, vp.Height);
        memcpy(guest_ptr(GUEST_VIEWPORT), &vp, sizeof(vp));
        update_viewport_constants();
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
            glScissor(left, (GLint)s_target_height - bottom, right - left, bottom - top);
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
    if (!s_device || !graphics_thread() || (count && !rects) || count > 65536 ||
        (flags & ~0xF3u) || !isfinite(depth)) {
        finish(24, (uint32_t)D3DERR_INVALIDCALL);
        return;
    }
    if (!s_depth_available) flags &= ~3u;
    uint32_t native_flags = ((flags & 0xF0) ? D3DCLEAR_TARGET : 0) |
                            ((flags & 1) ? D3DCLEAR_ZBUFFER : 0) |
                            ((flags & 2) ? D3DCLEAR_STENCIL : 0);
    int whole_target = !count && s_viewport.X == 0 && s_viewport.Y == 0 &&
        s_viewport.Width == s_target_width && s_viewport.Height == s_target_height;
    /* Use native API for its supported whole-target clear. Stencil write masks
     * and partial channels require the bridge's exact GL path. */
    HRESULT result = D3D_OK;
    if (whole_target && (flags & 0xF0) == 0xF0 && !(flags & 2))
        result = s_device->lpVtbl->Clear(s_device, 0, NULL, native_flags, color, depth, stencil);
    else
        clear_rectangles(count, rects, flags, color, depth, stencil);
    finish(24, (uint32_t)result);
}


/* Opt-in diagnostic: save real native backbuffers at selected presentations.
 * At most16 captures per run; this never injects game pixels. */
static void capture_frame(uint32_t frame)
{
    static int configured;
    static uint32_t requested[16], attempted;
    static unsigned count;
    static const char *single_path, *directory;
    if (!configured) {
        configured=1;
        const char *number=getenv("WRATH_CAPTURE_FRAME");
        const char *numbers=getenv("WRATH_CAPTURE_FRAMES");
        single_path=getenv("WRATH_CAPTURE_PATH");
        directory=getenv("WRATH_CAPTURE_DIRECTORY");
        if (numbers || directory) {
            if (number || single_path || !numbers || !*numbers || !directory || directory[0]!='/') goto invalid_capture;
            while (*numbers) {
                char *end=NULL;
                unsigned long value=strtoul(numbers,&end,10);
                if (count==16 || end==numbers || !value || value>UINT32_MAX ||
                    (*end && *end!=',') || (*end==',' && !end[1])) goto invalid_capture;
                for (unsigned i=0;i<count;++i) if (requested[i]==value) goto invalid_capture;
                requested[count++]=(uint32_t)value;
                numbers=*end?end+1:end;
            }
        } else if (number || single_path) {
            char *end=NULL;
            unsigned long value=number?strtoul(number,&end,10):0;
            if (!number || !*number || !end || *end || !value || value>UINT32_MAX || !single_path || single_path[0]!='/') goto invalid_capture;
            requested[count++]=(uint32_t)value;
        }
    }
    unsigned selected;
    for (selected=0;selected<count;++selected)
        if (frame==requested[selected] && !(attempted&(1u<<selected))) break;
    if (selected==count) return;
    attempted|=1u<<selected;
    char generated_path[4096];
    const char *path=single_path;
    if (directory) {
        int n=snprintf(generated_path,sizeof(generated_path),"%s/frame-%06u.bmp",directory,frame);
        if (n<0 || (size_t)n>=sizeof(generated_path)) goto invalid_capture;
        path=generated_path;
    }
    size_t pitch=(size_t)s_width*4, bytes=pitch*s_height;
    uint8_t *pixels=malloc(bytes), *row=malloc(pitch);
    if (!pixels || !row) {
        fprintf(stderr,"[wrath graphics] capture allocation failed\n"); free(pixels); free(row); return;
    }
    GLint old_fbo,old_buffer,old_pack,old_alignment,old_row,old_skip_rows,old_skip_pixels;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&old_fbo); glGetIntegerv(GL_READ_BUFFER,&old_buffer);
    glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING,&old_pack); glGetIntegerv(GL_PACK_ALIGNMENT,&old_alignment);
    glGetIntegerv(GL_PACK_ROW_LENGTH,&old_row); glGetIntegerv(GL_PACK_SKIP_ROWS,&old_skip_rows);
    glGetIntegerv(GL_PACK_SKIP_PIXELS,&old_skip_pixels);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,0); glReadBuffer(GL_BACK);
    glBindBuffer(GL_PIXEL_PACK_BUFFER,0); glPixelStorei(GL_PACK_ALIGNMENT,4);
    glPixelStorei(GL_PACK_ROW_LENGTH,0); glPixelStorei(GL_PACK_SKIP_ROWS,0); glPixelStorei(GL_PACK_SKIP_PIXELS,0);
    glReadPixels(0,0,s_width,s_height,GL_BGRA,GL_UNSIGNED_BYTE,pixels);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,old_fbo); glReadBuffer(old_buffer);
    glBindBuffer(GL_PIXEL_PACK_BUFFER,old_pack); glPixelStorei(GL_PACK_ALIGNMENT,old_alignment);
    glPixelStorei(GL_PACK_ROW_LENGTH,old_row); glPixelStorei(GL_PACK_SKIP_ROWS,old_skip_rows);
    glPixelStorei(GL_PACK_SKIP_PIXELS,old_skip_pixels);
    for (unsigned y=0;y<s_height/2;++y) {
        uint8_t *top=pixels+y*pitch, *bottom=pixels+(s_height-1-y)*pitch;
        memcpy(row,top,pitch); memcpy(top,bottom,pitch); memcpy(bottom,row,pitch);
    }
    SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormatFrom(pixels,(int)s_width,(int)s_height,32,(int)pitch,SDL_PIXELFORMAT_BGRA32);
    if (!surface || SDL_SaveBMP(surface,path)<0)
        fprintf(stderr,"[wrath graphics] capture frame%u failed: %s\n",frame,SDL_GetError());
    else
        fprintf(stderr,"[wrath graphics] captured actual frame%u %ux%u to %s\n",frame,s_width,s_height,path);
    if (surface) SDL_FreeSurface(surface);
    free(row); free(pixels);
    return;
invalid_capture:
    count=0;
    fprintf(stderr,"[wrath graphics] capture requires FRAME+absolute PATH or up to16 comma-separated FRAMES+absolute DIRECTORY\n");
}

/* DWORD WINAPI Swap(flags): return observed swap count, not HRESULT. */
void sub_00100C40(void)
{
    uint32_t flags = arg(0);
    if (!s_device || !graphics_thread()) {
        finish(4, (uint32_t)D3DERR_INVALIDCALL);
        return;
    }
    if (!flags) flags = 5; /* SDK's default, observed at 0x00100C50. */
    /* Bit 2 submits a new swap; flag-only wait operations do not present. */
    if (flags & 4) {
        capture_frame(read32(GUEST_SWAP_COUNT)+1);
        uint64_t profile_start=wrath_profile_present_begin();
        HRESULT result = s_device->lpVtbl->Swap(s_device, flags);
        if (result < 0) {
            finish(4, (uint32_t)result);
            return;
        }
        write32(GUEST_SWAP_COUNT, read32(GUEST_SWAP_COUNT) + 1);
        wrath_vblank_notify_swap();
        wrath_profile_present_end(profile_start);
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
#define RESOURCE_DEPTH_SURFACE 4
#define RESOURCE_CUBE_TEXTURE 5
#define RESOURCE_PALETTE 6
#define RESOURCE_INDEX_BUFFER 7
static struct Resource {
    uint32_t handle, data, bytes, width, height, levels, format, pitch, owner;
    uint32_t offsets[13], pitches[13], sizes[13];
    unsigned type, references, bindings;
    GLenum framebuffer;
    GLuint target_fbo, target_texture;
    GLuint cube_gl;
    GLuint palette_gl[3]; /* P8 stage0 uses the native texture; other stages vary. */
    uint64_t palette_revision, p8_revision;
    uint64_t p8_uploaded[4], p8_palette_revision[4];
    uint32_t p8_palette_handle[4];
    uint32_t face_stride;
    int linear, dirty;
    uint8_t *encoded_snapshot;
    int snapshot_valid;
    uint64_t upload_serial;
    IDirect3DTexture8 *texture;
    IDirect3DVertexBuffer8 *vertex_buffer;
} s_resources[RESOURCE_LIMIT];
static uint32_t s_texture_handles[4], s_stream_handle, s_stream_stride, s_fvf;
static uint32_t s_palette_handles[4];
static uint64_t s_palette_revision;
/* Exact encoded-byte shadows avoid decoding unchanged guest storage. Their
 * total host allocation is capped; exhaustion preserves the uncached path. */
#define TEXTURE_SNAPSHOT_BUDGET (64u*1024u*1024u)
static size_t s_texture_snapshot_bytes;
static int texture_snapshot_matches(const struct Resource *r)
{
    return r->snapshot_valid && r->encoded_snapshot &&
        !memcmp(r->encoded_snapshot,guest_ptr(r->data),r->bytes);
}
static const uint8_t *texture_snapshot_capture(struct Resource *r)
{
    r->snapshot_valid=0;
    if (!r->encoded_snapshot && r->bytes &&
        r->bytes<=TEXTURE_SNAPSHOT_BUDGET-s_texture_snapshot_bytes) {
        r->encoded_snapshot=malloc(r->bytes);
        if (r->encoded_snapshot) s_texture_snapshot_bytes+=r->bytes;
    }
    if (!r->encoded_snapshot) return guest_ptr(r->data);
    memcpy(r->encoded_snapshot,guest_ptr(r->data),r->bytes);
    return r->encoded_snapshot;
}

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
    uint32_t kind = (r->type == RESOURCE_TEXTURE || r->type == RESOURCE_CUBE_TEXTURE) ? 0x40000 : (r->type == RESOURCE_SURFACE || r->type == RESOURCE_DEPTH_SURFACE) ? 0x50000 : 0;
    if (r->type == RESOURCE_INDEX_BUFFER) kind = 0x10000;
    if (r->type == RESOURCE_PALETTE) kind = 0x30000 | (r->format << 30);
    write32(r->handle, 0x1000000 | kind | (r->references & 0xFFFF) | (r->bindings << 19));
}
static void release_resource(struct Resource *r)
{
    if (r->references || r->bindings) { update_common(r); return; }
    if (r->encoded_snapshot) { s_texture_snapshot_bytes-=r->bytes; free(r->encoded_snapshot); }
    if (r->target_fbo) glDeleteFramebuffers(1, &r->target_fbo);
    if (r->target_texture) glDeleteTextures(1, &r->target_texture);
    if (r->cube_gl) glDeleteTextures(1, &r->cube_gl);
    glDeleteTextures(3, r->palette_gl);
    if (r->texture) r->texture->lpVtbl->Release(r->texture);
    if (r->vertex_buffer) r->vertex_buffer->lpVtbl->Release(r->vertex_buffer);
    uint32_t owner = r->owner;
    if (!owner && r->type != RESOURCE_INDEX_BUFFER) xbox_HeapFree(r->data);
    xbox_HeapFree(r->handle);
    memset(r, 0, sizeof(*r));
    if (owner) {
        struct Resource *parent = resource(owner);
        if (parent && parent->references) {
            if (parent->type == RESOURCE_CUBE_TEXTURE) parent->dirty = 1;
            --parent->references; release_resource(parent);
        }
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
    case 0: case 0x19: case 0x0B: return 1;
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
/* Cube faces are stored in Xbox face order (+X,-X,+Y,-Y,+Z,-Z), with each
 * complete mip chain padded to 128 bytes. Pixel data remains guest-owned. */
extern GLuint xbox_D3D8GLTextureName(IDirect3DTexture8 *texture);
static HRESULT upload_texture_images(struct Resource *r, GLenum target, GLuint name, unsigned faces,
                                     const uint32_t *colors, unsigned color_count)
{
    if (!r || !name || !graphics_thread()) return D3DERR_INVALIDCALL;
    if (r->format == 0x0B && (!colors || !color_count)) return D3DERR_INVALIDCALL;
    /* P8 output also depends on the stage palette: its caller validates that
     * key. Other formats require byte equality even if no Lock marked dirty. */
    if (r->format != 0x0B && texture_snapshot_matches(r)) { r->dirty=0; return 0; }
    if (r->format == 0x0B) {
        for (unsigned level = 0; level < r->levels; ++level) {
            const uint8_t *indices = guest_ptr(r->data + r->offsets[level]);
            for (uint32_t i = 0; i < r->sizes[level]; ++i)
                if (indices[i] >= color_count) return D3DERR_INVALIDCALL;
        }
    }
    uint64_t profile_start=wrath_profile_begin(),profile_bytes=0;
    uint32_t *pixels = malloc((size_t)r->width * r->height * 4);
    if (!pixels) return (HRESULT)0x8007000E;
    const uint8_t *encoded=texture_snapshot_capture(r);
    GLint old_texture, old_unpack, old_alignment, old_row, old_rows, old_columns, old_swap;
    glGetIntegerv(target == GL_TEXTURE_CUBE_MAP ? GL_TEXTURE_BINDING_CUBE_MAP : GL_TEXTURE_BINDING_2D, &old_texture);
    glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &old_unpack);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &old_alignment);
    glGetIntegerv(GL_UNPACK_ROW_LENGTH, &old_row);
    glGetIntegerv(GL_UNPACK_SKIP_ROWS, &old_rows);
    glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &old_columns);
    glGetIntegerv(GL_UNPACK_SWAP_BYTES, &old_swap);
    glBindTexture(target, name);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4); glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0); glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_FALSE);
    int linear, bpp = format_info(r->format, &linear);
    for (unsigned face = 0; face < faces; ++face) for (unsigned level = 0; level < r->levels; ++level) {
        uint32_t width = r->width >> level, height = r->height >> level;
        if (!width) width = 1; if (!height) height = 1;
        profile_bytes+=(uint64_t)width*height*4;
        const uint8_t *source = encoded + face * r->face_stride + r->offsets[level];
        for (unsigned y = 0; y < height; ++y) for (unsigned x = 0; x < width; ++x) {
            uint32_t color;
            if (!bpp) {
                unsigned block_size = r->format == 0x0C ? 8 : 16;
                unsigned block = (y / 4) * ((width + 3) / 4) + x / 4;
                color = dxt_color(source + block * block_size, (y % 4) * 4 + x % 4, r->format);
            } else {
                uint32_t packed = 0;
                uint32_t offset = linear ? y * r->pitches[level] + x * bpp : morton_index(x, y, width, height) * bpp;
                memcpy(&packed, source + offset, (size_t)bpp);
                color = r->format == 0x0B ? colors[packed] : uncompressed_color(packed, r->format);
            }
            pixels[y * width + x] = color;
        }
        glTexImage2D(target == GL_TEXTURE_CUBE_MAP ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + face : GL_TEXTURE_2D,
                     (GLint)level, GL_RGBA8, width, height, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, pixels);
    }
    glTexParameteri(target, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, (GLint)r->levels - 1);
    if (target == GL_TEXTURE_CUBE_MAP) {
        glTexParameteri(target, GL_TEXTURE_MIN_FILTER, r->levels > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
        glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    }
    GLenum error = glGetError();
    glBindTexture(target, old_texture); glBindBuffer(GL_PIXEL_UNPACK_BUFFER, old_unpack);
    glPixelStorei(GL_UNPACK_ALIGNMENT, old_alignment); glPixelStorei(GL_UNPACK_ROW_LENGTH, old_row);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, old_rows); glPixelStorei(GL_UNPACK_SKIP_PIXELS, old_columns);
    glPixelStorei(GL_UNPACK_SWAP_BYTES, old_swap);
    free(pixels);
    if (error != GL_NO_ERROR) {
        fprintf(stderr, "[wrath graphics] texture upload failed GL0x%X\n", error);
        return D3DERR_INVALIDCALL;
    }
    wrath_profile_upload(profile_start,profile_bytes);
    r->snapshot_valid=r->encoded_snapshot!=NULL; ++r->upload_serial;
    r->dirty = 0; return 0;
}
static HRESULT upload_cube_texture(struct Resource *r)
{
    if (!r || r->type != RESOURCE_CUBE_TEXTURE || !graphics_thread()) return D3DERR_INVALIDCALL;
    if (!r->cube_gl) glGenTextures(1, &r->cube_gl);
    return upload_texture_images(r, GL_TEXTURE_CUBE_MAP, r->cube_gl, 6, NULL, 0);
}
void sub_000FE9F0(void) /* CreateCubeTexture(edge,levels,usage,format,pool,out) */
{
    uint32_t edge = arg(0), levels = arg(1), format = arg(3), out = arg(5);
    int linear, bpp = format_info(format, &linear);
    struct Resource *r = new_resource();
    if (!s_device || !graphics_thread() || !out || !r || bpp < 0 || linear || !edge ||
        edge > 4096 || (edge & (edge - 1))) { finish(24, (uint32_t)D3DERR_INVALIDCALL); return; }
    write32(out, 0);
    unsigned max_levels = log2_size(edge) + 1;
    if (!levels) levels = max_levels;
    if (levels > max_levels) { finish(24, (uint32_t)D3DERR_INVALIDCALL); return; }
    r->type = RESOURCE_CUBE_TEXTURE; r->width = r->height = edge;
    r->levels = levels; r->format = format; r->references = 1; r->dirty = 1;
    uint32_t chain_size = 0;
    for (unsigned level = 0; level < levels; ++level) {
        uint32_t w = edge >> level; if (!w) w = 1;
        r->offsets[level] = chain_size;
        r->pitches[level] = bpp ? w * bpp : ((w + 3) / 4) * (format == 0x0C ? 8 : 16);
        r->sizes[level] = r->pitches[level] * (bpp ? w : (w + 3) / 4);
        chain_size += r->sizes[level];
    }
    r->face_stride = (chain_size + 127) & ~127u;
    r->bytes = r->face_stride * 6; r->pitch = r->pitches[0];
    if (r->bytes > 64 * 1024 * 1024) { memset(r, 0, sizeof(*r)); finish(24, 0x8007000E); return; }
    r->handle = xbox_HeapAlloc(20, 16); r->data = xbox_HeapAlloc(r->bytes, 128);
    if (!r->handle || !r->data) {
        if (r->handle) xbox_HeapFree(r->handle);
        if (r->data) xbox_HeapFree(r->data);
        memset(r, 0, sizeof(*r)); finish(24, 0x8007000E); return;
    }
    memset(guest_ptr(r->data), 0, r->bytes); update_common(r);
    write32(r->handle + 4, r->data); write32(r->handle + 8, 0);
    uint32_t encoded = 0x2D | (format << 8) | (levels << 16) |
                       (log2_size(edge) << 20) | (log2_size(edge) << 24);
    if (arg(2) & 0x10000) encoded &= ~8u; /* BORDER_SOURCE_TEXTURE usage. */
    write32(r->handle + 12, encoded); write32(r->handle + 16, 0);
    write32(out, r->handle); finish(24, 0);
}
void sub_00103CB0(void) /* GetCubeMapSurface(texture,face,level,out) */
{
    struct Resource *parent = resource(arg(0)), *r = new_resource();
    uint32_t face = arg(1), level = arg(2), out = arg(3);
    if (!parent || parent->type != RESOURCE_CUBE_TEXTURE || face >= 6 ||
        level >= parent->levels || !out || !r) { finish(16, (uint32_t)D3DERR_INVALIDCALL); return; }
    write32(out, 0); r->handle = xbox_HeapAlloc(24, 16);
    if (!r->handle) { finish(16, 0x8007000E); return; }
    r->type = RESOURCE_SURFACE; r->owner = parent->handle; r->references = 1;
    r->width = r->height = parent->width >> level; if (!r->width) r->width = r->height = 1;
    r->levels = 1; r->format = parent->format;
    r->data = parent->data + face * parent->face_stride + parent->offsets[level];
    r->bytes = parent->sizes[level]; r->pitch = parent->pitches[level];
    r->pitches[0] = r->pitch; r->sizes[0] = r->bytes;
    ++parent->references; parent->dirty = 1; update_common(parent); update_common(r);
    write32(r->handle + 4, r->data); write32(r->handle + 8, 0);
    /* 4361's 106080 preserves the low20 format bits, including cube/level
     * count, while replacing U/V logs for the chosen surface level. */
    write32(r->handle + 12, (read32(parent->handle + 12) & 0xFFFFF) |
            (log2_size(r->width) << 20) | (log2_size(r->height) << 24));
    write32(r->handle + 16, 0); write32(r->handle + 20, parent->handle);
    write32(out, r->handle); finish(16, 0);
}
static HRESULT upload_texture(struct Resource *r)
{
    if (!r || r->type != RESOURCE_TEXTURE || !r->texture) return D3DERR_INVALIDCALL;
    return upload_texture_images(r, GL_TEXTURE_2D, xbox_D3D8GLTextureName(r->texture), 1, NULL, 0);
}

/* An indexed image may be simultaneously bound to multiple stages with
 * different palettes. Expand each actual stage to its own RGBA texture before
 * native filtering; never interpolate integer palette indices. */
static GLuint texture_gl_name(struct Resource *r, unsigned stage)
{
    if (!r || !r->texture || stage >= 4) return 0;
    if (r->format == 0x0B && stage) {
        if (!r->palette_gl[stage - 1]) glGenTextures(1, &r->palette_gl[stage - 1]);
        return r->palette_gl[stage - 1];
    }
    return xbox_D3D8GLTextureName(r->texture);
}
static HRESULT upload_texture_stage(struct Resource *r, unsigned stage)
{
    if (!r || stage >= 4) return D3DERR_INVALIDCALL;
    if (r->format != 0x0B) return upload_texture(r);
    struct Resource *palette = resource(s_palette_handles[stage]);
    if (!palette || palette->type != RESOURCE_PALETTE) {
        fprintf(stderr, "[wrath graphics] P8 texture 0x%X has no palette at stage %u\n", r->handle, stage);
        return D3DERR_INVALIDCALL;
    }
    /* Retained CPU pointers may change indices or palette colors without Lock.
     * Compare both exact byte sequences; a revision alone cannot prove reuse. */
    if (!texture_snapshot_matches(r)) ++r->p8_revision;
    if (!texture_snapshot_matches(palette)) {
        texture_snapshot_capture(palette);
        palette->snapshot_valid=palette->encoded_snapshot!=NULL;
        palette->palette_revision=++s_palette_revision;
    }
    r->dirty=0;
    if (r->p8_uploaded[stage] == r->p8_revision &&
        r->p8_palette_handle[stage] == palette->handle &&
        r->p8_palette_revision[stage] == palette->palette_revision) return 0;
    r->dirty = 1;
    HRESULT result = upload_texture_images(r, GL_TEXTURE_2D, texture_gl_name(r, stage), 1,
                                          palette->snapshot_valid?(const uint32_t *)palette->encoded_snapshot:guest_ptr(palette->data), palette->bytes / 4);
    if (result >= 0) {
        r->p8_uploaded[stage] = r->p8_revision;
        r->p8_palette_handle[stage] = palette->handle;
        r->p8_palette_revision[stage] = palette->palette_revision;
    }
    return result;
}

void sub_00100E30(void) /* CreatePalette(sizeEnum,out), ret8. */
{
    uint32_t size = arg(0), out = arg(1);
    struct Resource *r = new_resource();
    if (!s_device || !r || size > 3 || !out) { finish(8, (uint32_t)D3DERR_INVALIDCALL); return; }
    write32(out, 0);
    r->bytes = 1024u >> size;
    r->handle = xbox_HeapAlloc(12, 16); r->data = xbox_HeapAlloc(r->bytes, 64);
    if (!r->handle || !r->data) {
        if (r->handle) xbox_HeapFree(r->handle);
        if (r->data) xbox_HeapFree(r->data);
        memset(r, 0, sizeof(*r)); finish(8, 0x8007000E); return;
    }
    r->type = RESOURCE_PALETTE; r->format = size; r->references = 1; r->palette_revision = ++s_palette_revision;
    memset(guest_ptr(r->data), 0, r->bytes);
    update_common(r); write32(r->handle + 4, r->data); write32(r->handle + 8, 0);
    write32(out, r->handle); finish(8, 0);
}
void sub_00100D40(void) /* Shared palette/index-buffer Lock(handle,out,flags), ret12. */
{
    uint32_t handle = arg(0), out = arg(1);
    struct Resource *r = resource(handle);
    if (!r || !out || (r->type != RESOURCE_PALETTE && r->type != RESOURCE_INDEX_BUFFER)) {
        finish(12, (uint32_t)D3DERR_INVALIDCALL); return;
    }
    /* Native draws copy guest bytes while holding the device lock, so no
     * outstanding hardware read requires the original resource-idle wait. */
    if (r->type == RESOURCE_PALETTE) r->palette_revision = ++s_palette_revision;
    write32(out, r->data); finish(12, 0);
}
void sub_000FFE10(void) /* SetPalette(stage,handle), ret8. */
{
    uint32_t stage = arg(0), handle = arg(1);
    struct Resource *r = resource(handle);
    if (!s_device || stage >= 4 || (handle && (!r || r->type != RESOURCE_PALETTE))) {
        finish(8, (uint32_t)D3DERR_INVALIDCALL); return;
    }
    if (r) r->palette_revision = ++s_palette_revision;
    if (handle != s_palette_handles[stage]) {
        if (r) { ++r->bindings; update_common(r); }
        struct Resource *old = resource(s_palette_handles[stage]);
        if (old) { --old->bindings; release_resource(old); }
        s_palette_handles[stage] = handle;
    }
    write32(GUEST_DEVICE + 0xA88 + stage * 4, handle);
    finish(8, 0);
}

void sub_000FE9C0(void) /* CreateTexture(w,h,levels,usage,format,pool,out), ret28 */
{
    uint32_t width = arg(0), height = arg(1), levels = arg(2), format = arg(4), output = arg(6);
    int linear, bpp = format_info(format, &linear);
    struct Resource *r = new_resource();
    if (!s_device || !graphics_thread() || !output || !r || bpp < 0 || !width || !height ||
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
    if (!r || (r->type != RESOURCE_TEXTURE && r->type != RESOURCE_CUBE_TEXTURE) || level >= r->levels || !out) { finish(12, (uint32_t)D3DERR_INVALIDCALL); return; }
    uint32_t w = r->width >> level, h = r->height >> level;
    if (!w) w = 1; if (!h) h = 1;
    uint32_t desc[7] = {r->format, 1, 0, r->sizes[level], 0, w, h};
    memcpy(guest_ptr(out), desc, sizeof(desc)); finish(12, 0);
}
void sub_00103DD0(void)
{
    struct Resource *r = resource(arg(0));
    finish(4, r && (r->type == RESOURCE_TEXTURE || r->type == RESOURCE_CUBE_TEXTURE) ? r->levels : 0);
}
void sub_00103A90(void)
{
    struct Resource *r = resource(arg(0));
    if (!r || r->references == 0xFFFF) { fprintf(stderr, "[wrath graphics] invalid AddRef 0x%X\n", arg(0)); finish(4, 0); return; }
    ++r->references; update_common(r); finish(4, r->references);
}
static void diagnose_invalid_release(uint32_t handle)
{
    static unsigned failures;
    if (failures++ >= 16) return;
    fprintf(stderr,"[wrath graphics] invalid Release0x%X caller0x%X esp0x%X eax0x%X ecx0x%X edx0x%X stack=",
            handle,read32(g_esp),g_esp,g_eax,g_ecx,g_edx);
    for (unsigned i=0;i<8;++i) fprintf(stderr," %08X",read32(g_esp+i*4));
#ifndef WRATH_GRAPHICS_SMOKE_TEST
    extern _Thread_local uint32_t g_ebx,g_esi,g_edi,g_ebp;
    fprintf(stderr," ebx0x%X esi0x%X edi0x%X ebp0x%X",g_ebx,g_esi,g_edi,g_ebp);
#endif
    if (handle>=0x1000u && handle<=0x4000000u-24) {
        fputs(" header=",stderr);
        for (unsigned i=0;i<6;++i) fprintf(stderr," %08X",read32(handle+i*4));
    }
    fputc('\n',stderr);
}
void sub_00103AD0(void)
{
    struct Resource *r = resource(arg(0));
    if (!r || !r->references) { diagnose_invalid_release(arg(0)); finish(4, 0); return; }
    uint32_t references = --r->references; release_resource(r); finish(4, references);
}
void sub_000FFC90(void)
{
    uint32_t stage = arg(0), handle = arg(1); struct Resource *r = resource(handle);
    if (!s_device || !graphics_thread() || stage >= 4 || (handle && (!r || (r->type != RESOURCE_TEXTURE && r->type != RESOURCE_CUBE_TEXTURE)))) {
        finish(8, (uint32_t)D3DERR_INVALIDCALL); return;
    }
    HRESULT result = 0;
    wrath_profile_bind(handle,s_texture_handles[stage]);
    if (r) { r->dirty = 1; if (r->format != 0x0B) result = r->type==RESOURCE_CUBE_TEXTURE ? upload_cube_texture(r) : upload_texture(r); }
    if (result >= 0) result = s_device->lpVtbl->SetTexture(s_device, stage,
                                   r && r->type==RESOURCE_TEXTURE ? (IDirect3DBaseTexture8 *)r->texture : NULL);
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
    if (!s_device || !graphics_thread() || !size || size > 64 * 1024 * 1024 || !out || !r) {
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
static void trace_stream_binding(uint32_t stream, uint32_t handle, uint32_t stride, HRESULT result)
{
    static unsigned traced;
    if (!stream || traced>=32 || !getenv("WRATH_TRACE_STREAMS")) return;
    ++traced;
    fprintf(stderr,"[wrath streams] bind%u stream%u handle0x%X stride%u result0x%X caller0x%X swaps%u\n",
            traced,stream,handle,stride,(unsigned)result,read32(g_esp),read32(GUEST_SWAP_COUNT));
}
void sub_00102580(void)
{
    uint32_t stream = arg(0), handle = arg(1), stride = arg(2); struct Resource *r = resource(handle);
    if (!s_device || stream != 0 || (handle && (!r || r->type != RESOURCE_VERTEX_BUFFER || !stride))) {
        trace_stream_binding(stream,handle,stride,D3DERR_INVALIDCALL);
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
    trace_stream_binding(stream,handle,stride,result);
    finish(12, (uint32_t)result);
}

#include "shader_bridge.inc"

void sub_001026F0(void) /* SetShaderConstantMode(mode), ret4; supported192-constant bank. */
{
    uint32_t mode=arg(0);
    if (!s_device || !graphics_thread() || mode!=0) state_error(0x1026F0,mode);
    /* The native fixed vertex shader owns its matrices/uniforms; it needs no
     * NV2A constant-bank upload. Preserve the observed guest mode/dirty fields. */
    write32(GUEST_DEVICE+8,read32(GUEST_DEVICE+8)&~0x200u);
    write32(GUEST_DEVICE+0x2018,0);
    write32(0x10EC10,read32(0x10EC10)|0x1600);
    finish(4,0);
}
void sub_00102BB0(void) /* SetPixelShader(handle), ret4; NULL selects fixed texture stages. */
{
    uint32_t handle=arg(0);
    if (!s_device || !graphics_thread()) state_error(0x102BB0,handle);
    if (handle) { shader_set_pixel(handle); finish(4,0); return; }
    s_pixel_handle=0;
    HRESULT result=s_device->lpVtbl->SetPixelShader(s_device,0);
    if (result<0) state_error(0x102BB0,handle);
    native_state(D3DRS_TEXTUREFACTOR,read32(0x10F01C));
    write32(GUEST_DEVICE+0x370,0);
    uint32_t dirty=0x4800;
    if (read32(GUEST_DEVICE+0x374)) dirty|=0x2000;
    write32(0x10EC10,read32(0x10EC10)|dirty);
    finish(4,0);
}

void sub_00102940(void) /* SetVertexShader: even FVF codes vs odd program handles. */
{
    uint32_t fvf = arg(0);
    if (!s_device || !graphics_thread()) state_error(0x102940,fvf);
    if (fvf&1) { shader_set_vertex(fvf); s_fvf=fvf; finish(4,0); return; }
    s_vertex_handle=0;
    if (
        ((fvf & 0xE) != D3DFVF_XYZ && (fvf & 0xE) != D3DFVF_XYZRHW)) {
        s_fvf = 0;
        fprintf(stderr, "[wrath graphics] unsupported vertex program/FVF 0x%X\n", fvf);
        finish(4, (uint32_t)D3DERR_INVALIDCALL); return;
    }
    s_fvf = fvf;
    finish(4, (uint32_t)s_device->lpVtbl->SetVertexShader(s_device, fvf));
}
static void apply_fixed_transforms(void)
{
    /* Xbox enum VIEW0/PROJECTION1/WORLD6 differs from the host D3D enum.
     * Preserve the original setter and its dirty/depth calculations; consume
     * its authoritative matrix cache when the native fixed pipeline draws. */
    const unsigned guest_states[3]={6,0,1};
    const D3DTRANSFORMSTATETYPE native_states[3]={D3DTS_WORLD,D3DTS_VIEW,D3DTS_PROJECTION};
    for (unsigned i=0;i<3;++i) {
        D3DMATRIX matrix;
        memcpy(&matrix,guest_ptr(GUEST_DEVICE+0x750+guest_states[i]*64),sizeof(matrix));
        if (s_device->lpVtbl->SetTransform(s_device,native_states[i],&matrix)<0)
            state_error(0xFEA20,guest_states[i]);
    }
}
#include "graphics_probe.inc"

static HRESULT draw_vertices_data(uint32_t type, uint32_t count, const void *vertices, uint32_t stride)
{
    wrath_profile_draw((uint64_t)count*stride);
    if (!count) return 0;
    if (!s_device || !graphics_thread() || !vertices || !stride || stride > 1024 || count > 1024 * 1024 || (uint64_t)count * stride > 64 * 1024 * 1024 || !s_fvf)
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
        if (r) { HRESULT result = r->type==RESOURCE_CUBE_TEXTURE ? upload_cube_texture(r) : upload_texture_stage(r,i); if (result < 0) return result; }
    }
    void *converted = NULL;
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
    if (s_vertex_handle || s_pixel_handle) {
        GLenum primitive=type==1?GL_POINTS:type==2?GL_LINES:type==3?GL_LINE_STRIP:type==6?GL_TRIANGLE_STRIP:type==7?GL_TRIANGLE_FAN:GL_TRIANGLES;
        unsigned probe=probe_begin();
        HRESULT result=shader_draw(primitive,count,vertices,stride);
        probe_end(probe,result);
        free(converted); return result;
    }
    for (unsigned stage=1; stage<4; ++stage) {
        struct Resource *r=resource(s_texture_handles[stage]);
        if (r && r->format==0x0B) {
            fprintf(stderr,"[wrath graphics] fixed pipeline P8 stage%u requires multistage binding\n",stage);
            free(converted); return D3DERR_INVALIDCALL;
        }
    }
    apply_fixed_transforms();
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
    unsigned probe=probe_begin();
    HRESULT result = s_device->lpVtbl->DrawPrimitiveUP(s_device, native, primitives, vertices, stride);
    probe_end(probe,result);
    free(converted); return result;
}
static HRESULT draw_vertices(uint32_t type, uint32_t count, uint32_t data, uint32_t stride)
{
    if (count && (!data || (uint64_t)data + (uint64_t)count * stride > 64u * 1024 * 1024))
        return D3DERR_INVALIDCALL;
    return draw_vertices_data(type, count, data ? guest_ptr(data) : NULL, stride);
}
void sub_001019C0(void) /* DrawVerticesUP(type,vertexCount,data,stride) */
{
    finish(16, (uint32_t)draw_vertices(arg(0), arg(1), arg(2), arg(3)));
}
void sub_00101B20(void) /* DrawVertices(type,startVertex,vertexCount) */
{
    uint32_t start = arg(1), count = arg(2); struct Resource *r = resource(s_stream_handle);
    if (!r || !s_stream_stride || (uint64_t)(start + (uint64_t)count) * s_stream_stride > r->bytes) {
        finish(12, (uint32_t)D3DERR_INVALIDCALL); return;
    }
    finish(12, (uint32_t)draw_vertices(arg(0), count, r->data + start * s_stream_stride, s_stream_stride));
}

/* Xbox surfaces can refer to the live native drawable or to guest texture
 * storage. Framebuffer CopyRects always moves actual rendered pixels. */
static uint32_t s_backbuffer_handle, s_frontbuffer_handle, s_depthbuffer_handle;
static HRESULT initialize_depth_surface(uint32_t format)
{
    /* Native SDL drawable has D24S8; the title requests its Xbox linear format. */
    if (format!=0x2A) return D3DERR_INVALIDCALL;
    struct Resource *r=new_resource(); if (!r) return (HRESULT)0x8007000E;
    r->width=s_width; r->height=s_height; r->format=format; r->linear=1;
    r->pitch=(s_width*4+63)&~63u; r->bytes=r->pitch*s_height;
    r->handle=xbox_HeapAlloc(24,16); r->data=xbox_HeapAlloc(r->bytes,128);
    if (!r->handle || !r->data) {
        if (r->handle) xbox_HeapFree(r->handle); if (r->data) xbox_HeapFree(r->data);
        memset(r,0,sizeof(*r)); return (HRESULT)0x8007000E;
    }
    r->type=RESOURCE_DEPTH_SURFACE; r->levels=1; r->bindings=1;
    r->sizes[0]=r->bytes; r->pitches[0]=r->pitch;
    update_common(r); write32(r->handle+4,r->data); write32(r->handle+8,0);
    write32(r->handle+12,0x10021|(format<<8));
    write32(r->handle+16,(r->width-1)|((r->height-1)<<12)|((r->pitch/64-1)<<24));
    write32(r->handle+20,0); s_depthbuffer_handle=r->handle;
    write32(GUEST_DEVICE+0x2074,r->handle);
    return 0;
}
void sub_000FF830(void) /* GetDepthStencilSurface(out), ret4. */
{
    uint32_t out=arg(0);
    if (!s_device || !graphics_thread() || !out) { finish(4,(uint32_t)D3DERR_INVALIDCALL); return; }
    struct Resource *r=resource(read32(GUEST_DEVICE+0x2074));
    write32(out,r?r->handle:0);
    if (!r) { finish(4,0x88760866); return; } /* D3DERR_NOTFOUND, observed FF850. */
    ++r->references; update_common(r); finish(4,0);
}
static HRESULT prepare_texture_target(struct Resource *r);
static HRESULT resolve_texture_target(struct Resource *r);
void sub_000FEF20(void) /* SetRenderTarget(color,depth), ret8. NULL color keeps current. */
{
    uint32_t color=arg(0),depth=arg(1);
    if (!color) color=read32(GUEST_DEVICE+0x2070);
    struct Resource *r=resource(color), *z=resource(depth);
    int window = r && r->framebuffer == GL_BACK;
    struct Resource *parent = r ? resource(r->owner) : NULL;
    int texture = r && !r->framebuffer && parent && parent->type == RESOURCE_TEXTURE;
    int linear, bpp = r ? format_info(r->format, &linear) : -1;
    if (!s_device || !graphics_thread() || !r || r->type!=RESOURCE_SURFACE ||
        (!window && (!texture || bpp <= 0)) ||
        (depth && (!window || !z || z->type!=RESOURCE_DEPTH_SURFACE || depth!=s_depthbuffer_handle))) {
        fprintf(stderr,"[wrath graphics] unsupported SetRenderTarget color0x%X depth0x%X\n",color,depth);
        finish(8,(uint32_t)D3DERR_INVALIDCALL); return;
    }
    uint32_t old_color = read32(GUEST_DEVICE+0x2070);
    struct Resource *old = resource(old_color);
    if (old_color != color) {
        HRESULT result = old && old->target_fbo ? resolve_texture_target(old) : 0;
        if (result >= 0 && texture) result = prepare_texture_target(r);
        if (result < 0) { finish(8,(uint32_t)result); return; }
        ++r->bindings; update_common(r);
        if (old && old->bindings) { --old->bindings; release_resource(old); }
    } else if (texture && !r->target_fbo) {
        HRESULT result = prepare_texture_target(r);
        if (result < 0) { finish(8,(uint32_t)result); return; }
    }
    glBindFramebuffer(GL_FRAMEBUFFER,window ? 0 : r->target_fbo);
    glDrawBuffer(window ? GL_BACK : GL_COLOR_ATTACHMENT0);
    glReadBuffer(window ? GL_BACK : GL_COLOR_ATTACHMENT0);
    s_target_width=r->width; s_target_height=r->height;
    write32(GUEST_DEVICE+0x2070,color); write32(GUEST_DEVICE+0x2074,depth);
    s_depth_available=depth!=0;
    native_state(D3DRS_ZENABLE,s_depth_available&&read32(0x10F008));
    gl_toggle(GL_DEPTH_TEST,s_depth_available&&read32(0x10F008));
    gl_toggle(GL_STENCIL_TEST,s_depth_available&&read32(0x10F00C));
    s_viewport=(D3DVIEWPORT8){0,0,r->width,r->height,0,1};
    s_device->lpVtbl->SetViewport(s_device,&s_viewport);
    glViewport(0,0,r->width,r->height);
    memcpy(guest_ptr(GUEST_VIEWPORT),&s_viewport,sizeof(s_viewport));
    update_viewport_constants();
    finish(8,0);
}

void sub_000FF450(void) /* GetBackBuffer(index,type,out), ret12 */
{
    int32_t index = (int32_t)arg(0); uint32_t out = arg(2);
    if (!s_device || !graphics_thread() || !out || (index != 0 && index != -1) || arg(1) != 0) {
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
        r->width = s_width; r->height = s_height; r->levels = 1; r->bindings = index == 0 ? 2 : 1;
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
    if (r->framebuffer || (r->target_fbo && read32(GUEST_DEVICE+0x2070)==r->handle)) {
        GLint old_fbo, old_buffer, old_pack, old_alignment, old_row, old_rows, old_pixels;
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &old_fbo);
        glGetIntegerv(GL_READ_BUFFER, &old_buffer);
        glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &old_pack);
        glGetIntegerv(GL_PACK_ALIGNMENT, &old_alignment);
        glGetIntegerv(GL_PACK_ROW_LENGTH, &old_row);
        glGetIntegerv(GL_PACK_SKIP_ROWS, &old_rows);
        glGetIntegerv(GL_PACK_SKIP_PIXELS, &old_pixels);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, r->target_fbo);
        glReadBuffer(r->framebuffer ? r->framebuffer : GL_COLOR_ATTACHMENT0); glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
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
/* Keep guest texture storage coherent when leaving a render target. This first
 * implementation resolves actual GPU pixels at target switches; shader sampling
 * then uses the ordinary texture upload path. It avoids stale guest uploads and
 * keeps row orientation explicit without a shader-specific texture flip. */
static HRESULT resolve_texture_target(struct Resource *r)
{
    int linear, bpp=format_info(r->format,&linear);
    if (bpp<=0) return D3DERR_INVALIDCALL;
    uint32_t *pixels=malloc((size_t)r->width*r->height*4);
    if (!pixels) return (HRESULT)0x8007000E;
    HRESULT result=surface_pixels(r,pixels);
    if (result>=0) {
        uint8_t *data=guest_ptr(r->data);
        for (unsigned y=0;y<r->height;++y) for (unsigned x=0;x<r->width;++x) {
            uint32_t value=encode_color(pixels[y*r->width+x],r->format);
            uint32_t offset=linear ? y*r->pitch+x*bpp : morton_index(x,y,r->width,r->height)*bpp;
            memcpy(data+offset,&value,bpp);
        }
        struct Resource *parent=resource(r->owner);
        if (parent) parent->dirty=1;
    }
    free(pixels); return result;
}
static HRESULT prepare_texture_target(struct Resource *r)
{
    uint32_t *pixels=malloc((size_t)r->width*r->height*4);
    if (!pixels) return (HRESULT)0x8007000E;
    HRESULT result=surface_pixels(r,pixels);
    if (result<0) { free(pixels); return result; }
    for (unsigned y=0;y<r->height/2;++y) for (unsigned x=0;x<r->width;++x) {
        unsigned a=y*r->width+x,b=(r->height-1-y)*r->width+x;
        uint32_t swap=pixels[a]; pixels[a]=pixels[b]; pixels[b]=swap;
    }
    GLint old_read,old_draw,old_tex,old_unpack,old_alignment,old_row,old_rows,old_pixels;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&old_read); glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&old_draw);
    glGetIntegerv(GL_TEXTURE_BINDING_2D,&old_tex); glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&old_unpack);
    glGetIntegerv(GL_UNPACK_ALIGNMENT,&old_alignment); glGetIntegerv(GL_UNPACK_ROW_LENGTH,&old_row);
    glGetIntegerv(GL_UNPACK_SKIP_ROWS,&old_rows); glGetIntegerv(GL_UNPACK_SKIP_PIXELS,&old_pixels);
    if (!r->target_texture) glGenTextures(1,&r->target_texture);
    if (!r->target_fbo) glGenFramebuffers(1,&r->target_fbo);
    glBindTexture(GL_TEXTURE_2D,r->target_texture); glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);
    glPixelStorei(GL_UNPACK_ALIGNMENT,4); glPixelStorei(GL_UNPACK_ROW_LENGTH,0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS,0); glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,r->width,r->height,0,GL_BGRA,GL_UNSIGNED_INT_8_8_8_8_REV,pixels);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER,r->target_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,r->target_texture,0);
    glDrawBuffer(GL_COLOR_ATTACHMENT0); glReadBuffer(GL_COLOR_ATTACHMENT0);
    result=glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE ? 0 : D3DERR_INVALIDCALL;
    glBindFramebuffer(GL_READ_FRAMEBUFFER,old_read); glBindFramebuffer(GL_DRAW_FRAMEBUFFER,old_draw);
    glBindTexture(GL_TEXTURE_2D,old_tex); glBindBuffer(GL_PIXEL_UNPACK_BUFFER,old_unpack);
    glPixelStorei(GL_UNPACK_ALIGNMENT,old_alignment); glPixelStorei(GL_UNPACK_ROW_LENGTH,old_row);
    glPixelStorei(GL_UNPACK_SKIP_ROWS,old_rows); glPixelStorei(GL_UNPACK_SKIP_PIXELS,old_pixels);
    free(pixels); return result;
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
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, destination->target_fbo);
    GLint old_draw_buffer; glGetIntegerv(GL_DRAW_BUFFER, &old_draw_buffer);
    glDrawBuffer(destination->framebuffer ? destination->framebuffer : GL_COLOR_ATTACHMENT0);
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
    if (!s_device || !graphics_thread() || !source || !destination || source->type != RESOURCE_SURFACE ||
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
        if (destination->framebuffer || (destination->target_fbo && read32(GUEST_DEVICE+0x2070)==destination->handle)) {
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

#include "index_bridge.inc"
#include "immediate_bridge.inc"

recomp_func_t wrath_graphics_lookup(uint32_t address)
{
    switch (address) {
    case 0x000FD6E0: return sub_000FD6E0;
    case 0x000FD830: return sub_000FD830;
    case 0x000FDAD0: return sub_000FDAD0;
    case 0x000FDDF0: return sub_000FDDF0;
    case 0x000FDF60: return sub_000FDF60;
    case 0x000FDB40: return sub_000FDB40;
    case 0x000FE5C0: return sub_000FE5C0;
    case 0x000FE660: return sub_000FE660;
    case 0x000FE6F0: return sub_000FE6F0;
    case 0x000FF450: return sub_000FF450;
    case 0x000FF830: return sub_000FF830;
    case 0x000FEF20: return sub_000FEF20;
    case 0x000FE9F0: return sub_000FE9F0;
    case 0x00103CB0: return sub_00103CB0;
    case 0x00102AA0: return sub_00102AA0;
    case 0x00102D80: return sub_00102D80;
    case 0x000FF580: return sub_000FF580;
    case 0x000FF860: return sub_000FF860;
    case 0x00100EA0: return sub_00100EA0;
    case 0x00100C40: return sub_00100C40;
    case 0x00100D00: return sub_00100D00;
    case 0x00100D40: return sub_00100D40;
    case 0x00100E30: return sub_00100E30;
    case 0x000FFE10: return sub_000FFE10;
    case 0x000FFEA0: return sub_000FFEA0;
    case 0x00101BC0: return sub_00101BC0;
    case 0x00101E20: return sub_00101E20;
    case 0x00101E60: return sub_00101E60;
    case 0x00101EC0: return sub_00101EC0;
    case 0x00101F00: return sub_00101F00;
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
    case 0x001026F0: return sub_001026F0;
    case 0x00102BB0: return sub_00102BB0;
    case 0x001019C0: return sub_001019C0;
    case 0x00101B20: return sub_00101B20;
    default: return NULL;
    }
}

#ifdef WRATH_GRAPHICS_SMOKE_TEST
/* Standalone native rendering/guest ABI test; no ISO or game assets needed. */
#include <assert.h>
#include <stdlib.h>
_Thread_local uint32_t g_eax, g_esp, g_ecx, g_edx;
ptrdiff_t g_xbox_mem_offset;
static uint32_t test_heap = 0x180000;
uint32_t xbox_HeapAlloc(uint32_t size, uint32_t alignment)
{
    test_heap = (test_heap + alignment - 1) & ~(alignment - 1);
    if (size > 0x400000 - test_heap) return 0;
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
static void test_state(uint32_t method, uint32_t value)
{
    g_ecx=0x40000|method; g_edx=value; call(0xFD830,NULL,0);
    assert(g_eax==0);
}
#include "../tools/test_index_bridge.inc"
#include "../tools/test_immediate_bridge.inc"

static void test_cube_resources(void)
{
    uint32_t create_cube[] = {4,3,0,0x0F,0,0x9000};
    call(0xFE9F0,create_cube,6); assert(g_eax==0);
    uint32_t cube = read32(0x9000); struct Resource *r = resource(cube);
    assert(r && r->type==RESOURCE_CUBE_TEXTURE && r->face_stride==128 && r->bytes==768);
    assert(read32(cube)==0x1040001 && read32(cube+12)==0x02230F2D);
    const uint16_t endpoints[6]={0xF800,0x07E0,0x001F,0xFFE0,0xF81F,0x07FF};
    uint32_t held_surface=0;
    for(unsigned face=0;face<6;face++) for(unsigned level=0;level<3;level++) {
        uint32_t get[]={cube,face,level,0x9010}; call(0x103CB0,get,4); assert(g_eax==0);
        uint32_t surface=read32(0x9010), data=read32(surface+4);
        assert(data==r->data+face*128+level*16 && read32(surface+20)==cube);
        assert(read32(surface+12)==(0x00030F2Du|((2-level)<<20)|((2-level)<<24)));
        uint8_t block[16]={255,255};
        uint16_t color=endpoints[(face+level)%6];
        memcpy(block+8,&color,2); memcpy(block+10,&color,2); memcpy(guest_ptr(data),block,16);
        if(face==5 && level==2) held_surface=surface;
        else { uint32_t release[]={surface}; call(0x103AD0,release,1); assert(!resource(surface)); }
    }
    assert(r->references==2);
    uint32_t level_count[]={cube}; call(0x103DD0,level_count,1); assert(g_eax==3);
    uint32_t desc[]={cube,2,0x9020}; call(0x103C20,desc,3);
    assert(g_eax==0 && read32(0x9020+12)==16 && read32(0x9020+20)==1);
    uint32_t bad_face[]={cube,6,0,0x9010}; call(0x103CB0,bad_face,4);
    assert(g_eax==(uint32_t)D3DERR_INVALIDCALL && r->references==2);
    GLuint prior_cube, unpack; glGenTextures(1,&prior_cube); glGenBuffers(1,&unpack);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_CUBE_MAP,prior_cube);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER,unpack); glBufferData(GL_PIXEL_UNPACK_BUFFER,8,NULL,GL_STREAM_DRAW);
    glPixelStorei(GL_UNPACK_ROW_LENGTH,9); glPixelStorei(GL_UNPACK_SKIP_ROWS,2);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS,3); glPixelStorei(GL_UNPACK_ALIGNMENT,8);
    glPixelStorei(GL_UNPACK_SWAP_BYTES,GL_TRUE);
    assert(upload_cube_texture(r)==0 && !r->dirty && glIsTexture(r->cube_gl));
    GLint old; glGetIntegerv(GL_ACTIVE_TEXTURE,&old); assert(old==GL_TEXTURE2);
    glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP,&old); assert((GLuint)old==prior_cube);
    glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&old); assert((GLuint)old==unpack);
    glGetIntegerv(GL_UNPACK_ROW_LENGTH,&old); assert(old==9);
    glGetIntegerv(GL_UNPACK_SKIP_ROWS,&old); assert(old==2);
    glGetIntegerv(GL_UNPACK_SKIP_PIXELS,&old); assert(old==3);
    glGetIntegerv(GL_UNPACK_ALIGNMENT,&old); assert(old==8);
    glGetIntegerv(GL_UNPACK_SWAP_BYTES,&old); assert(old==GL_TRUE);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0); glDeleteBuffers(1,&unpack);
    glPixelStorei(GL_UNPACK_ROW_LENGTH,0); glPixelStorei(GL_UNPACK_SKIP_ROWS,0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS,0); glPixelStorei(GL_UNPACK_ALIGNMENT,4);
    glPixelStorei(GL_UNPACK_SWAP_BYTES,GL_FALSE);
    glBindTexture(GL_TEXTURE_CUBE_MAP,r->cube_gl);
    for(unsigned face=0;face<6;face++) for(unsigned level=0;level<3;level++) {
        uint32_t actual[16]={0}; unsigned edge=4>>level;
        glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X+face,level,GL_BGRA,GL_UNSIGNED_INT_8_8_8_8_REV,actual);
        for(unsigned i=0;i<edge*edge;i++) assert(actual[i]==color565(endpoints[(face+level)%6]));
    }
    GLuint cube_gl=r->cube_gl;
    uint32_t release[]={cube}; call(0x103AD0,release,1);
    assert(g_eax==1 && resource(cube) && glIsTexture(cube_gl));
    release[0]=held_surface; call(0x103AD0,release,1);
    assert(!resource(held_surface) && !resource(cube) && !glIsTexture(cube_gl));
    glDeleteTextures(1,&prior_cube); glActiveTexture(GL_TEXTURE0);
    uint32_t create_mips[]={8,4,4,0,6,0,0x9030};
    call(0xFE9C0,create_mips,7); assert(g_eax==0);
    uint32_t mip_handle=read32(0x9030); struct Resource *mip=resource(mip_handle);
    for(unsigned level=0;level<4;level++) {
        unsigned w=8>>level,h=4>>level; if(!h)h=1;
        for(unsigned y=0;y<h;y++) for(unsigned x=0;x<w;x++)
            write32(mip->data+mip->offsets[level]+morton_index(x,y,w,h)*4,
                    0xFF000000u|(level*60u<<16)|(y*40u<<8)|x*20u);
    }
    assert(upload_texture(mip)==0);
    glBindTexture(GL_TEXTURE_2D,xbox_D3D8GLTextureName(mip->texture));
    glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,&old); assert(old==3);
    for(unsigned level=0;level<4;level++) {
        unsigned w=8>>level,h=4>>level; if(!h)h=1; uint32_t actual[32];
        glGetTexImage(GL_TEXTURE_2D,level,GL_BGRA,GL_UNSIGNED_INT_8_8_8_8_REV,actual);
        for(unsigned y=0;y<h;y++) for(unsigned x=0;x<w;x++)
            assert(actual[y*w+x]==(0xFF000000u|(level*60u<<16)|(y*40u<<8)|x*20u));
    }
    release[0]=mip_handle; call(0x103AD0,release,1); assert(!resource(mip_handle));
    assert(glGetError()==GL_NO_ERROR);
}
static void test_palette_resources(void)
{
    uint32_t palettes[2];
    for (unsigned stage = 0; stage < 2; ++stage) {
        uint32_t create[] = {stage ? 3u : 0u, 0xB100};
        call(0x100E30, create, 2); assert(g_eax == 0);
        palettes[stage] = read32(0xB100);
        assert(read32(palettes[stage]) == (0x01030001u | (create[0] << 30)));
        uint32_t lock[] = {palettes[stage], 0xB104, 0xA0};
        call(0x100D40, lock, 3); assert(g_eax == 0);
        uint32_t data = read32(0xB104);
        for (unsigned i = 0; i < (stage ? 32u : 256u); ++i)
            write32(data + i * 4, (stage ? 0x90200000u : 0xF0400000u) | (i << 8) | (31u - (i & 31)));
        uint32_t bind[] = {stage, palettes[stage]}; call(0xFFE10, bind, 2); assert(g_eax == 0);
        assert(read32(GUEST_DEVICE + 0xA88 + stage * 4) == palettes[stage]);
    }
    uint32_t create[] = {8, 4, 0, 0, 0x0B, 0, 0xB108};
    call(0xFE9C0, create, 7); assert(g_eax == 0);
    uint32_t handle = read32(0xB108); struct Resource *r = resource(handle);
    assert(r && r->bytes == 43 && r->levels == 4 && r->pitch == 8);
    for (unsigned level = 0; level < r->levels; ++level) {
        unsigned w = r->width >> level, h = r->height >> level;
        if (!w) w = 1; if (!h) h = 1;
        uint8_t *data = guest_ptr(r->data + r->offsets[level]);
        for (unsigned y = 0; y < h; ++y) for (unsigned x = 0; x < w; ++x)
            data[morton_index(x, y, w, h)] = (uint8_t)((x + y * w + level) & 31);
    }
    for (unsigned stage = 0; stage < 2; ++stage) {
        uint32_t bind[] = {stage, handle}; call(0xFFC90, bind, 2); assert(g_eax == 0);
    }
    for (unsigned stage = 0; stage < 2; ++stage) {
        assert(upload_texture_stage(r, stage) == 0);
        glBindTexture(GL_TEXTURE_2D, texture_gl_name(r, stage));
        for (unsigned level = 0; level < r->levels; ++level) {
            unsigned w = r->width >> level, h = r->height >> level;
            if (!w) w = 1; if (!h) h = 1;
            uint32_t actual[32] = {0};
            glGetTexImage(GL_TEXTURE_2D, level, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, actual);
            for (unsigned y = 0; y < h; ++y) for (unsigned x = 0; x < w; ++x) {
                unsigned i = (x + y * w + level) & 31;
                assert(actual[y*w+x] == ((stage ? 0x90200000u : 0xF0400000u) | (i << 8) | (31-i)));
            }
        }
    }
    assert(texture_gl_name(r, 0) != texture_gl_name(r, 1));
    uint64_t uploaded = r->p8_uploaded[0], palette_revision = r->p8_palette_revision[0];
    assert(upload_texture_stage(r, 0) == 0 && r->p8_uploaded[0] == uploaded && r->p8_palette_revision[0] == palette_revision);
    uint32_t lock[] = {palettes[0], 0xB104, 0}; call(0x100D40, lock, 3);
    write32(read32(0xB104), 0x12345678);
    assert(upload_texture_stage(r, 0) == 0 && r->p8_palette_revision[0] != palette_revision);
    glBindTexture(GL_TEXTURE_2D, texture_gl_name(r, 0));
    uint32_t actual[32]; glGetTexImage(GL_TEXTURE_2D, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, actual);
    assert(actual[0] == 0x12345678);
    for (unsigned stage = 0; stage < 2; ++stage) {
        uint32_t release[] = {palettes[stage]}; call(0x103AD0, release, 1);
        assert(resource(palettes[stage])); /* Device binding keeps palette alive. */
        uint32_t unbind[] = {stage, 0}; call(0xFFE10, unbind, 2);
        assert(!resource(palettes[stage]));
        call(0xFFC90, unbind, 2);
    }
    GLuint variant = r->palette_gl[0];
    uint32_t release[] = {handle}; call(0x103AD0, release, 1);
    assert(!resource(handle) && !glIsTexture(variant));
    assert(glGetError() == GL_NO_ERROR);
}
static void test_shader_bridge(void)
{
    uint32_t target[]={s_backbuffer_handle,0}; call(0xFEF20,target,2); assert(g_eax==0);
    uint32_t off[]={0}; call(0xFDAD0,off,1); call(0xFE5C0,off,1); call(0xFE660,off,1);
    test_state(0x304,0); test_state(0x300,0); test_state(0x358,0x1010101);
    write32(0x10EF60,0); write32(0x10EF8C,0);
    uint32_t create[]={2,2,1,0,6,0,0xB000}; call(0xFE9C0,create,7); assert(g_eax==0);
    uint32_t texture=read32(0xB000),data=read32(texture+4);
    for(unsigned i=0;i<4;++i)write32(data+i*4,0xFF40A020);
    uint32_t bind[]={0,texture}; call(0xFFC90,bind,2); assert(g_eax==0);
    write32(0x10EC18,1);write32(0x10EC1C,1);write32(0x10EC24,1);write32(0x10EC28,1);write32(0x10EC2C,0);
    /* Hand-assembled MOV r0.x,c2 + MOV oPos,v0; MAD oT0,r0.x,c3,v3;
     * ADD oPos,r12,c4. Same semantics tested independently by transform feedback. */
    uint32_t words[12]={
        0,(1u<<25)|(1u<<21)|(2u<<13)|0x1b,(3u<<26)|(0x1bu<<2),(2u<<28)|(8u<<24)|(15u<<12)|(1u<<11)|4,
        0,(4u<<21)|(3u<<13)|(3u<<9),(1u<<26)|(0x1bu<<17)|(3u<<11)|(0x1bu<<2),(2u<<28)|(15u<<12)|(1u<<11)|(9u<<3),
        0,(3u<<21)|(4u<<13)|0x1b,(12u<<28)|(1u<<26)|(0x1bu<<2),(3u<<28)|(15u<<12)|(1u<<11)|1
    };
    uint32_t object=0xC000;memset(guest_ptr(object),0,0x160);
    write32(object,1);write32(object+4,0x10);write32(object+8,3);write32(object+12,13);
    for(unsigned i=0;i<16;++i)write32(object+0x14+i*16+8,2);
    write32(object+0x14+8,0x32);
    write32(object+0x14+3*16+4,20);write32(object+0x14+3*16+8,0x22);
    write32(object+0x114,(12u<<18)|0xB00);memcpy(guest_ptr(object+0x118),words,sizeof(words));
    uint32_t setvs[]={object+1};call(0x102940,setvs,1);assert(g_eax==0);
    memset(guest_ptr(0xD000),0,48);uint32_t vc[]={(uint32_t)-94,0xD000,3};call(0x102AA0,vc,3);assert(g_eax==0);
    Nv2aPixelDef definition={0};definition.combiner_count=1;definition.texture_modes=1;
    definition.rgb_inputs[0]=0x08200000;definition.alpha_inputs[0]=0x18200000;
    definition.rgb_outputs[0]=0xC0;definition.alpha_outputs[0]=0xC0;
    memcpy(guest_ptr(0xE00C),&definition,sizeof(definition));write32(0xE008,0xE00C);
    uint32_t setps[]={0xE000};call(0x102BB0,setps,1);assert(g_eax==0);
    struct {float x,y,z,rhw;uint32_t color;float u,v;} vertices[4]={
        {40,40,0,1,0,0,0},{80,40,0,1,0,1,0},{80,80,0,1,0,1,1},{40,80,0,1,0,0,1}};
    memcpy(guest_ptr(0xF000),vertices,sizeof(vertices));
    uint32_t clear[]={0,0,0xF0,0,0x3F800000,0};call(0x100EA0,clear,6);
    uint32_t draw[]={8,4,0xF000,sizeof(vertices[0])};call(0x1019C0,draw,4);assert(g_eax==0);
    uint8_t pixel[4];glReadPixels(60,240-60,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
    assert(pixel[0]==0x40 && pixel[1]==0xA0 && pixel[2]==0x20 && pixel[3]==255);
    uint64_t age=s_shader_age;
    float constant[4]={0.2f,0.5f,1,1};memcpy(guest_ptr(0xD000),constant,16);
    uint32_t pc[]={0,0xD000,1};call(0x102D80,pc,3);assert(g_eax==0);
    assert(read32(GUEST_DEVICE+0x3EC)==0xFF3380FF);
    assert(!memcmp(guest_ptr(0xE00C),&definition,sizeof(definition)));
    call(0x1019C0,draw,4);assert(s_shader_age==age+1);
    unsigned programs=0;for(unsigned i=0;i<SHADER_CACHE_LIMIT;++i)programs+=s_shader_programs[i].program!=0;
    assert(programs==1); /* Uniform changes reuse compiled programs. */
    /* The same texture on two stages must retain independent repeat/clamp
     * samplers. At u=1.25 stage0 reads red and stage1 reads green; sum is yellow. */
    write32(data,0xFF400000);write32(data+4,0xFF008000);
    write32(data+8,0xFF400000);write32(data+12,0xFF008000);
    bind[0]=0;bind[1]=texture;call(0xFFC90,bind,2);
    bind[0]=1;call(0xFFC90,bind,2);
    uint32_t state1=0x10EC18+128;
    write32(state1,3);write32(state1+4,3);write32(state1+12,1);write32(state1+16,1);write32(state1+20,0);
    write32(object+8,4);write32(object+12,17);write32(object+0x114,(16u<<18)|0xB00);
    write32(object+0x118+11*4,words[11]&~1u);
    uint32_t final_words[4]={0,(1u<<21)|(3u<<9)|0x1b,2u<<26,(15u<<12)|(1u<<11)|(10u<<3)|1};
    memcpy(guest_ptr(object+0x118+12*4),final_words,16);write32(object+0x118+16*4,0);
    call(0x102940,setvs,1);assert(g_eax==0);
    definition.texture_modes=1|(1u<<5);definition.rgb_inputs[0]=0x08200920;
    definition.rgb_outputs[0]=0xC00;memcpy(guest_ptr(0xE00C),&definition,sizeof(definition));call(0x102BB0,setps,1);
    for(unsigned i=0;i<4;++i){vertices[i].u=1.25f;vertices[i].v=.25f;}
    memcpy(guest_ptr(0xF000),vertices,sizeof(vertices));call(0x1019C0,draw,4);assert(g_eax==0);
    glReadPixels(60,240-60,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
    assert(pixel[0]==0x40 && pixel[1]==0x80 && pixel[2]==0);
    GLint sampler_binding;glActiveTexture(GL_TEXTURE0);glGetIntegerv(GL_SAMPLER_BINDING,&sampler_binding);assert(sampler_binding==0);
    glActiveTexture(GL_TEXTURE1);glGetIntegerv(GL_SAMPLER_BINDING,&sampler_binding);assert(sampler_binding==0);
    bind[0]=1;bind[1]=0;call(0xFFC90,bind,2);bind[0]=0;
    call(0x102BB0,off,1);setvs[0]=D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1;call(0x102940,setvs,1);
    bind[1]=0;call(0xFFC90,bind,2);uint32_t release[]={texture};call(0x103AD0,release,1);
    assert(!resource(texture));assert(glGetError()==GL_NO_ERROR);
}
#include "../tools/test_mixed_shader_bridge.inc"
#include "../tools/test_viewport_constants.inc"
#include "../tools/test_texture_snapshot.inc"

int main(void)
{
    /* Independent decoder facts: Morton order and canonical BC endpoints. */
    assert(morton_index(2,1,4,4) == 6 && morton_index(3,3,4,4) == 15);
    const uint8_t dxt1[8] = {0,0xF8,0x1F,0,0,0,0,0};
    assert(dxt_color(dxt1, 0, 0x0C) == 0xFFFF0000);
    const uint8_t transparent[8] = {0,0,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    assert(dxt_color(transparent, 0, 0x0C) == 0);
    assert(uncompressed_color(0x7E0, 5) == 0xFF00FF00);
    assert(xbox_D3D8GLAcquire()); /* Keep context current for direct GPU assertions. */
    void *memory = calloc(1, 0x400000);
    assert(memory);
    g_xbox_mem_offset = (ptrdiff_t)memory;
    /* SDK default-state fixture; the standalone smoke has no loaded XBE. */
    const uint8_t defaults[32] = {
        1,1,1,1,1,0,0,0,1,0,0,0,1,1,2,1,
        1,1,2,1,1,0,0,0,0,0,0,0,0,0,0,0
    };
    memcpy(guest_ptr(0x10BD9C), defaults, sizeof(defaults));
    /* Semantic render defaults needed by this fixture; production reads the XBE.
     * Nonzero reserved canaries prove the initializer skips rather than clears. */
    const struct { unsigned state; uint32_t value; } render_defaults[] = {
        {57,GL_LEQUAL},{58,GL_ALWAYS},{62,GL_ONE},{64,1},{66,GL_SMOOTH},
        {67,0x01010101},{68,GL_KEEP},{69,GL_KEEP},{70,GL_ALWAYS},
        {72,~0u},{73,~0u},{74,GL_FUNC_ADD},{85,0x3F800000},{86,0x3F800000},
        {92,1},{94,1},{95,1},{96,2},{97,1},{100,2},{101,1},
        {116,0xDEADBEEF},{120,GL_FILL},{121,GL_FILL},{124,2},{126,GL_KEEP},
        {127,GL_CW},{128,GL_CCW},{129,~0u},{135,0xDEADBEEF}
    };
    for (unsigned i=0;i<sizeof(render_defaults)/sizeof(render_defaults[0]);++i)
        write32(0x10BDC0+(render_defaults[i].state-57)*4,render_defaults[i].value);
    write32(0x10EE18+116*4,0x12345678);
    write32(0x10EE18+135*4,0x89ABCDEF);

    GuestPresentation pp = {0};
    pp.width = 320; pp.height = 240; pp.format = 6; pp.buffer_count = 2;
    pp.multisample = 0x11; pp.swap_effect = 1; pp.depth_enabled = 1;
    pp.depth_format = 0x2A; pp.interval = 1;
    memcpy(guest_ptr(0x2000), &pp, sizeof(pp));
    const uint32_t create[] = {0, 1, 0, 0x40, 0x2000, 0x3000};
    call(0xFD6E0, create, 6);
    assert(g_eax == 0 && read32(0x3000) == GUEST_DEVICE);
    assert(read32(GUEST_DEVICE_GLOBAL) == GUEST_DEVICE);
    assert(read32(0x10EE18+116*4)==0x12345678);
    assert(read32(0x10EE18+135*4)==0x89ABCDEF);
    assert(read32(0x10F01C)==0xFFFFFFFF && read32(0x10EF00)==GL_ALWAYS);
    assert(read32(0x10EF24)==0x01010101 && read32(0x10F014)==GL_CW);
    assert(read32(0x10F008)==1 && glIsEnabled(GL_DEPTH_TEST));
    assert(!glIsEnabled(GL_DITHER));
    GLint gl_value; DWORD native_value;
    glGetIntegerv(GL_BLEND_SRC_RGB,&gl_value); assert(gl_value==GL_ONE);
    glGetIntegerv(GL_BLEND_DST_RGB,&gl_value); assert(gl_value==GL_ZERO);
    s_device->lpVtbl->GetRenderState(s_device,D3DRS_SRCBLEND,&native_value);
    assert(native_value==D3DBLEND_ONE);
    s_device->lpVtbl->GetRenderState(s_device,D3DRS_DESTBLEND,&native_value);
    assert(native_value==D3DBLEND_ZERO);
    s_device->lpVtbl->GetRenderState(s_device,D3DRS_ALPHAFUNC,&native_value);
    assert(native_value==D3DCMP_ALWAYS);
    s_device->lpVtbl->GetRenderState(s_device,D3DRS_TEXTUREFACTOR,&native_value);
    assert(native_value==0xFFFFFFFF);
    /* Exercise the same initialization boundary without a depth attachment.
     * The table's W-buffer value2 must never leak to the guest or native cache. */
    s_depth_available=0; initialize_render_defaults();
    assert(read32(0x10F008)==0 && !glIsEnabled(GL_DEPTH_TEST));
    s_device->lpVtbl->GetRenderState(s_device,D3DRS_ZENABLE,&native_value);
    assert(native_value==0);
    s_depth_available=1; initialize_render_defaults();
    assert(read32(0x10F008)==1 && glIsEnabled(GL_DEPTH_TEST));
    assert(glGetError()==GL_NO_ERROR);

    for (unsigned stage=0; stage<4; ++stage) {
        assert(read32(0x10EC18+stage*128+16*4)==(stage ? 1u : 2u));
        assert(read32(0x10EC18+stage*128+12*4)==(stage ? 1u : 4u));
        assert(read32(0x10EC18+stage*128+28*4)==stage);
    }
    uint32_t no_state[]={0}, z_on[]={1};
    write32(GUEST_DEVICE+8,read32(GUEST_DEVICE+8)|0x200);
    call(0x1026F0,no_state,1); assert(g_eax==0 && !(read32(GUEST_DEVICE+8)&0x200));
    assert(read32(GUEST_DEVICE+0x2018)==0 && (read32(0x10EC10)&0x1600)==0x1600);
    call(0x102BB0,no_state,1); assert(g_eax==0 && read32(GUEST_DEVICE+0x370)==0);
    assert((read32(0x10EC10)&0x4800)==0x4800);
    call(0xFDAD0,no_state,1); assert(read32(0x10F018)==0 && !glIsEnabled(GL_CULL_FACE));
    call(0xFE5C0,z_on,1); assert(read32(0x10F008)==1 && glIsEnabled(GL_DEPTH_TEST));
    call(0xFE5C0,no_state,1); assert(!glIsEnabled(GL_DEPTH_TEST));
    test_state(0x304,0); assert(!glIsEnabled(GL_BLEND) && read32(0x10EE18+59*4)==0);
    test_state(0x354,GL_LEQUAL);
    test_state(0x35C,1);
    test_state(0x300,0);
    test_state(0x33C,GL_GREATER);
    test_state(0x340,127); assert(read32(0x10EE18+61*4)==127);
    test_state(0x358,0x1010101);
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
    /* Guest SDK blend values are GL enums, unlike the host D3D API enums.
     * Verify alpha blending through the actual native draw, not only caches. */
    clear[0]=0; clear[1]=0; clear[2]=0xF0; clear[3]=0xFF000000;
    call(0x100EA0,clear,6);
    for (unsigned i=0;i<4;++i) vertices[i].color=0x80FF0000;
    memcpy(guest_ptr(0x6000),vertices,sizeof(vertices));
    test_state(0x344,GL_SRC_ALPHA); test_state(0x348,GL_ONE_MINUS_SRC_ALPHA);
    test_state(0x304,1); test_state(0x350,GL_FUNC_ADD);
    call(0x1019C0,draw,4); assert(g_eax==0);
    glReadPixels(60,240-60,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
    assert(pixel[0]>=127 && pixel[0]<=129 && pixel[1]==0 && pixel[2]==0);
    test_state(0x304,0);
    /* Every alpha comparison must discard/retain real native fragments. */
    test_state(0x300,1); test_state(0x340,128);
    const int alpha_pass[3][8]={{0,0,0,0,1,1,1,1},{0,0,1,1,0,0,1,1},{0,1,0,1,0,1,0,1}};
    for (unsigned reference=0;reference<3;++reference) {
        test_state(0x340,127+reference);
        for (unsigned comparison=0;comparison<8;++comparison) {
            test_state(0x33C,GL_NEVER+comparison);
            call(0x100EA0,clear,6); call(0x1019C0,draw,4); assert(g_eax==0);
            glReadPixels(50,240-60,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
            assert(pixel[0]==(alpha_pass[reference][comparison]?255:0));
        }
    }
    test_state(0x300,0);
    uint32_t fill[]={GL_LINE}, coords[]={0,0};
    call(0xFDF60,coords,2); assert(g_eax==0 && read32(0x10EC88)==0);
    assert(*(uint8_t *)guest_ptr(0x1B11D1)==9);
    call(0xFDDF0,fill,1); assert(read32(0x10EFF8)==GL_LINE);
    call(0x100EA0,clear,6); call(0x1019C0,draw,4);
    glReadPixels(50,240-60,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel); assert(pixel[0]==0);
    fill[0]=GL_FILL; call(0xFDDF0,fill,1); call(0x1019C0,draw,4);
    glReadPixels(50,240-60,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel); assert(pixel[0]==255);
    /* Stencil fail operations write the actual native stencil attachment. */
    clear[0]=0; clear[1]=0; clear[2]=0xF2; clear[3]=0xFF000000; clear[5]=0;
    call(0x100EA0,clear,6);
    call(0xFE660,z_on,1); assert(glIsEnabled(GL_STENCIL_TEST) && read32(0x10F00C)==1);
    uint32_t stencil_op[]={GL_REPLACE}; call(0xFE6F0,stencil_op,1);
    test_state(0x364,GL_NEVER); test_state(0x368,3); test_state(0x36C,255); test_state(0x360,255);
    call(0x1019C0,draw,4);
    glReadPixels(50,240-60,1,1,GL_STENCIL_INDEX,GL_UNSIGNED_BYTE,pixel); assert(pixel[0]==3);
    glReadPixels(50,240-60,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel); assert(pixel[0]==0);
    stencil_op[0]=GL_KEEP; call(0xFE6F0,stencil_op,1); assert(read32(0x10F010)==GL_KEEP);
    test_state(0x364,GL_EQUAL); call(0x1019C0,draw,4);
    glReadPixels(50,240-60,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel); assert(pixel[0]==255);
    call(0xFE660,no_state,1); assert(!glIsEnabled(GL_STENCIL_TEST));
    /* Flat color uses the primitive's first vertex; UVs still interpolate. */
    vertices[1].color=0xFF00FF00; vertices[2].color=0xFF0000FF; vertices[3].color=0xFFFFFFFF;
    memcpy(guest_ptr(0x6000),vertices,sizeof(vertices)); test_state(0x37C,GL_FLAT);
    call(0x1019C0,draw,4);
    glReadPixels(50,240-60,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
    assert(pixel[0]==255 && pixel[1]==0 && pixel[2]==0);
    test_state(0x37C,GL_SMOOTH); call(0x1019C0,draw,4);
    glReadPixels(50,240-60,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel); assert(pixel[1]>0 || pixel[2]>0);
    for (unsigned i=0;i<4;++i) vertices[i].color=0x80FF0000;
    memcpy(guest_ptr(0x6000),vertices,sizeof(vertices));
    /* Capture real framebuffer pixels into a guest texture surface, preserving
     * top/bottom orientation, then restore them through native framebuffer blit. */
    clear[0] = 0; clear[1] = 0; clear[2] = 0xF0; clear[3] = 0xFF102030;
    call(0x100EA0, clear, 6);
    D3DRECT band = {0,0,16,4}; memcpy(guest_ptr(0x8000), &band, sizeof(band));
    clear[0] = 1; clear[1] = 0x8000; clear[3] = 0xFFB04020; call(0x100EA0, clear, 6);
    uint32_t get_back[] = {0,0,0x8100}; call(0xFF450, get_back, 3); assert(g_eax == 0);
    uint32_t back = read32(0x8100); assert(read32(GUEST_DEVICE + 0x207C) == back);
    uint32_t get_depth[]={0x8150}; call(0xFF830,get_depth,1); assert(g_eax==0);
    uint32_t depth_handle=read32(0x8150); assert(resource(depth_handle)->type==RESOURCE_DEPTH_SURFACE);
    uint32_t target[]={back,depth_handle}; call(0xFEF20,target,2); assert(g_eax==0);
    call(0xFE5C0,z_on,1); test_state(0x354,GL_LEQUAL); test_state(0x35C,1);
    clear[0]=0; clear[1]=0; clear[2]=0xF1; clear[3]=0xFF000000; clear[4]=0x3E800000;
    call(0x100EA0,clear,6); call(0x1019C0,draw,4);
    glReadPixels(50,240-60,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel); assert(pixel[0]==0);
    target[1]=0; call(0xFEF20,target,2); assert(g_eax==0 && !glIsEnabled(GL_DEPTH_TEST));
    call(0xFF830,get_depth,1); assert(g_eax==0x88760866 && read32(0x8150)==0);
    call(0x1019C0,draw,4);
    glReadPixels(50,240-60,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel); assert(pixel[0]==255);
    target[1]=depth_handle; call(0xFEF20,target,2); assert(g_eax==0);
    uint32_t depth_release[]={depth_handle}; call(0x103AD0,depth_release,1); assert(resource(depth_handle));
    call(0xFE5C0,no_state,1);
    /* A true offscreen target: draw into a swizzled texture, resolve GPU writes
     * to guest storage, and preserve orientation/content across target changes. */
    uint32_t rt_create[]={8,4,1,1,6,0,0x8160}; call(0xFE9C0,rt_create,7); assert(g_eax==0);
    uint32_t rt_texture=read32(0x8160),rt_get[]={rt_texture,0,0x8170};
    call(0x103C30,rt_get,3); assert(g_eax==0);
    uint32_t rt_surface=read32(0x8170),rt_target[]={rt_surface,0};
    call(0xFEF20,rt_target,2); assert(g_eax==0 && s_target_width==8 && s_target_height==4);
    GLint native_vp[4]; glGetIntegerv(GL_VIEWPORT,native_vp);
    assert(native_vp[0]==0 && native_vp[1]==0 && native_vp[2]==8 && native_vp[3]==4);
    clear[0]=0; clear[1]=0; clear[2]=0xF0; clear[3]=0xFF0000FF;
    call(0x100EA0,clear,6); assert(g_eax==0);
    memcpy(guest_ptr(0x8400),vertices,sizeof(vertices));
    for (unsigned i=0;i<4;++i) { vertices[i].color=0xFFFF0000; vertices[i].z=0; }
    vertices[0].x=vertices[3].x=0; vertices[1].x=vertices[2].x=8;
    vertices[0].y=vertices[1].y=0; vertices[2].y=vertices[3].y=2;
    memcpy(guest_ptr(0x6000),vertices,sizeof(vertices)); call(0x1019C0,draw,4); assert(g_eax==0);
    glReadPixels(4,3,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel); assert(pixel[0]==255 && pixel[2]==0);
    glReadPixels(4,0,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel); assert(pixel[0]==0 && pixel[2]==255);
    call(0xFEF20,target,2); assert(g_eax==0 && s_target_width==320 && s_target_height==240);
    uint32_t rt_data=read32(rt_surface+4);
    assert(read32(rt_data+morton_index(4,0,8,4)*4)==0xFFFF0000);
    assert(read32(rt_data+morton_index(4,3,8,4)*4)==0xFF0000FF);
    call(0xFEF20,rt_target,2); assert(g_eax==0);
    glReadPixels(4,3,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel); assert(pixel[0]==255 && pixel[2]==0);
    glReadPixels(4,0,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel); assert(pixel[0]==0 && pixel[2]==255);
    call(0xFEF20,target,2); assert(g_eax==0);
    uint32_t rt_release[]={rt_surface}; call(0x103AD0,rt_release,1); assert(!resource(rt_surface));
    rt_release[0]=rt_texture; call(0x103AD0,rt_release,1); assert(!resource(rt_texture));
    memcpy(vertices,guest_ptr(0x8400),sizeof(vertices)); memcpy(guest_ptr(0x6000),vertices,sizeof(vertices));
    /* Restore the capture pattern after the independent depth tests. */
    clear[0]=0; clear[1]=0; clear[2]=0xF0; clear[3]=0xFF102030; call(0x100EA0,clear,6);
    clear[0]=1; clear[1]=0x8000; clear[3]=0xFFB04020; call(0x100EA0,clear,6);
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
    GLuint capture_pbo; glGenBuffers(1,&capture_pbo); glBindBuffer(GL_PIXEL_PACK_BUFFER,capture_pbo);
    glBufferData(GL_PIXEL_PACK_BUFFER,16,NULL,GL_STREAM_READ); glReadBuffer(GL_FRONT);
    glPixelStorei(GL_PACK_ROW_LENGTH,17); glPixelStorei(GL_PACK_SKIP_ROWS,2);
    glPixelStorei(GL_PACK_SKIP_PIXELS,3); glPixelStorei(GL_PACK_ALIGNMENT,8);
    call(0x100C40, swap, 1);
    GLint capture_pack; glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING,&capture_pack); assert((GLuint)capture_pack==capture_pbo);
    glGetIntegerv(GL_READ_BUFFER,&capture_pack); assert(capture_pack==GL_FRONT);
    glBindBuffer(GL_PIXEL_PACK_BUFFER,0); glDeleteBuffers(1,&capture_pbo); glReadBuffer(GL_BACK);
    glGetIntegerv(GL_PACK_ROW_LENGTH,&capture_pack); assert(capture_pack==17);
    glGetIntegerv(GL_PACK_SKIP_ROWS,&capture_pack); assert(capture_pack==2);
    glGetIntegerv(GL_PACK_SKIP_PIXELS,&capture_pack); assert(capture_pack==3);
    glGetIntegerv(GL_PACK_ALIGNMENT,&capture_pack); assert(capture_pack==8);
    glPixelStorei(GL_PACK_ROW_LENGTH,0); glPixelStorei(GL_PACK_SKIP_ROWS,0);
    glPixelStorei(GL_PACK_SKIP_PIXELS,0); glPixelStorei(GL_PACK_ALIGNMENT,4);
    if (getenv("WRATH_CAPTURE_FRAME") && !strcmp(getenv("WRATH_CAPTURE_FRAME"),"1") && getenv("WRATH_CAPTURE_PATH")) {
        SDL_Surface *bmp=SDL_LoadBMP(getenv("WRATH_CAPTURE_PATH")); assert(bmp && bmp->w==320 && bmp->h==240);
        SDL_Surface *bgra=SDL_ConvertSurfaceFormat(bmp,SDL_PIXELFORMAT_BGRA32,0); assert(bgra);
        uint32_t top,bottom; memcpy(&top,bgra->pixels,4);
        memcpy(&bottom,(uint8_t *)bgra->pixels+7*bgra->pitch,4);
        assert((top&0xFFFFFF)==0xB04020 && (bottom&0xFFFFFF)==0x102030);
        SDL_FreeSurface(bgra); SDL_FreeSurface(bmp);
    }
    assert(g_eax == 1 && read32(GUEST_SWAP_COUNT) == 1);
    swap[0] = 1;
    call(0x100C40, swap, 1);
    assert(g_eax == 1);
    assert(glGetError() == GL_NO_ERROR);
    assert(wrath_graphics_lookup(0xDEADBEEF) == NULL);
    test_texture_snapshot();
    test_cube_resources();
    test_palette_resources();
    test_index_bridge();
    test_immediate_bridge();
    test_shader_bridge();
    test_mixed_shader_bridge();
    test_viewport_constants();
    puts("PASS: native GL, clears, guest ABI, texture/quad, vertex buffer, lifetime, native render states/blending/alpha tests/fill, framebuffer target/depth/copies, swap");
    xbox_D3D8GLRelease();
    SDL_Quit();
    free(memory);
    return 0;
}

#endif
