/* Native context handoff regression; no game assets or generated CPU code.
 * Main and loading-worker threads share the real drawable and GL objects. */
#include "d3d/d3d8_xbox.h"
#include <SDL.h>
#include <epoxy/gl.h>
#include <OpenGL/OpenGL.h>
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>

extern int xbox_D3D8GLAcquire(void);
extern void xbox_D3D8GLRelease(void);
extern void xbox_D3D8GLPumpEvents(void);
extern GLuint xbox_D3D8GLBackBuffer(unsigned index);
static IDirect3DDevice8 *device;
static GLuint texture;
static atomic_uint inside, passes;

static void render(unsigned worker, unsigned iteration)
{
    assert(xbox_D3D8GLAcquire());
    assert(atomic_fetch_add(&inside, 1) == 0);
    assert(CGLGetCurrentContext());
    /* Nested helpers must leave the outer SDK call's context attached. */
    assert(xbox_D3D8GLAcquire());
    xbox_D3D8GLRelease();
    assert(CGLGetCurrentContext());
    assert(glIsTexture(texture));
    glBindFramebuffer(GL_FRAMEBUFFER, xbox_D3D8GLBackBuffer(0));
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(worker ? 1.f : 0.f, worker ? 0.f : 1.f, iteration / 255.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    unsigned char pixel[4] = {0};
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glReadPixels(8, 8, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    assert(pixel[0] == (worker ? 255 : 0));
    assert(pixel[1] == (worker ? 0 : 255));
    assert(pixel[2] == iteration && pixel[3] == 255);
    if (iteration < 3) assert(device->lpVtbl->Present(device, NULL, NULL, NULL, NULL) == 0);
    xbox_D3D8GLPumpEvents(); /* Worker path must not call SDL event/window APIs. */
    assert(glGetError() == GL_NO_ERROR);
    atomic_fetch_add(&passes, 1);
    assert(atomic_fetch_sub(&inside, 1) == 1);
    xbox_D3D8GLRelease();
    assert(CGLGetCurrentContext() == NULL);
}
static void *worker_main(void *unused)
{
    (void)unused;
    IDirect3DDevice8 *invalid = NULL;
    D3DPRESENT_PARAMETERS pp = {0};
    assert(xbox_D3D8GLAcquire());
    assert(xbox_Direct3DCreate8(4361)->lpVtbl->CreateDevice(
        xbox_Direct3DCreate8(4361), 0, 0, NULL, 0, &pp, &invalid) < 0);
    xbox_D3D8GLRelease();
    for (unsigned i = 0; i < 24; ++i) render(1, i);
    return NULL;
}
int main(void)
{
    assert(pthread_main_np());
    assert(xbox_D3D8GLAcquire());
    D3DPRESENT_PARAMETERS pp = {0};
    pp.BackBufferWidth = 64; pp.BackBufferHeight = 64; pp.Windowed = 1;
    IDirect3D8 *factory = xbox_Direct3DCreate8(4361);
    assert(factory->lpVtbl->CreateDevice(factory, 0, 0, NULL, 0, &pp, &device) == 0);
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    unsigned char texel[4] = {42, 91, 17, 255};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, texel);
    xbox_D3D8GLRelease();
    assert(CGLGetCurrentContext() == NULL);
    pthread_t worker;
    Uint64 start = SDL_GetPerformanceCounter();
    assert(pthread_create(&worker, NULL, worker_main, NULL) == 0);
    for (unsigned i = 0; i < 24; ++i) render(0, i);
    /* No main dispatch servicing while joining: worker presentation must finish. */
    assert(pthread_join(worker, NULL) == 0);
    assert(atomic_load(&passes) == 48);
    assert(xbox_D3D8GLAcquire());
    glBindTexture(GL_TEXTURE_2D, texture);
    unsigned char readback[4] = {0};
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, readback);
    for (unsigned i = 0; i < 4; ++i) assert(readback[i] == texel[i]);
    glDeleteTextures(1, &texture);
    xbox_D3D8GLRelease();
    double elapsed = (SDL_GetPerformanceCounter() - start) / (double)SDL_GetPerformanceFrequency();
    /* Six presents have five inter-present intervals; the driver may return
     * immediately, but the shared monotonic schedule must still cap the rate. */
    assert(elapsed >= 5.0 / 60.0);
    assert(elapsed < 5.0);
    printf("graphics thread handoff: 48 readbacks, 6 real presents, shared texture intact; %.3fs\n", elapsed);
    SDL_Quit();
    return 0;
}
