#pragma once
#include <SDL.h>
/* Callers retain guest TLS and the native graphics mutex. Executor callbacks
 * invoke raw driver functions only, never guest code or the graphics mutex. */
int wumpa_gl_rpc_start(SDL_Window *window,SDL_GLContext context);
int wumpa_gl_rpc_active(void);
int wumpa_gl_rpc_owner(void);
void wumpa_gl_rpc_call(void (*function)(void *),void *argument);
void wumpa_gl_rpc_call_named(const char *name,void (*function)(void *),void *argument);
int wumpa_gl_rpc_swap(SDL_Window *window);
int wumpa_gl_rpc_swap_interval(void);
void wumpa_gl_rpc_stop(void);
/* Isolated SDL extension: callbacks execute under Android_ActivityMutex.
 * They may detach/rebind an existing context, never swap/create a context. */
extern void SDL_WumpaSetGLContextCallbacks(int (*backup)(void *),int (*restore)(void *),void *argument);
