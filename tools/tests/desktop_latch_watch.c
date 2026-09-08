/* Exercise production SDL watch registration/worker delivery without a game. */
#include <assert.h>
#include <stdio.h>
#include "../../third_party/xboxrecomp/src/input/xinput_device.c"

static int push_tap(void *unused)
{
    (void)unused;
    SDL_Event e = {0};
    e.type = SDL_KEYDOWN; e.key.windowID = 42;
    e.key.keysym.scancode = SDL_SCANCODE_SPACE;
    assert(SDL_PushEvent(&e) == 1);
    e.type = SDL_KEYUP; assert(SDL_PushEvent(&e) == 1);
    return 0;
}
int main(void)
{
    SDL_setenv("WRATH_KEYBOARD", "1", 1);
    xbox_InputInit();
    SDL_Thread *thread = SDL_CreateThread(push_tap, "tap producer", NULL);
    assert(thread); SDL_WaitThread(thread, NULL);
    Uint8 keys[SDL_NUM_SCANCODES] = {0};
    SDL_AtomicLock(&g_desktop_lock);
    xbox_DesktopLatchConsume(&g_desktop_latch, 42, 42, keys);
    SDL_AtomicUnlock(&g_desktop_lock);
    assert(keys[SDL_SCANCODE_SPACE]);
    SDL_Event events[2];
    assert(SDL_PeepEvents(events, 2, SDL_GETEVENT, SDL_KEYDOWN, SDL_KEYUP) == 2);
    assert(events[0].type == SDL_KEYDOWN && events[1].type == SDL_KEYUP);
    memset(keys, 0, sizeof(keys));
    xbox_DesktopLatchConsume(&g_desktop_latch, 42, 42, keys);
    assert(!keys[SDL_SCANCODE_SPACE]);
    xbox_InputShutdown();
    push_tap(NULL);
    xbox_DesktopLatchConsume(&g_desktop_latch, 42, 42, keys);
    assert(!keys[SDL_SCANCODE_SPACE]); /* Shutdown removed production watcher. */
    SDL_Quit();
    puts("PASS: production SDL watch, worker tap delivery, untouched queue, one-shot and shutdown");
    return 0;
}
