#!/usr/bin/env python3
"""Test the real synchronous render RPC with real threads and a fake SDL context."""
from pathlib import Path
import subprocess
import tempfile

HERE = Path(__file__).resolve().parents[1]

SDL = r'''
#pragma once
#include <stdint.h>
typedef struct SDL_Window { int unused; } SDL_Window;
typedef void *SDL_GLContext;
int SDL_GL_MakeCurrent(SDL_Window *, SDL_GLContext);
SDL_GLContext SDL_GL_GetCurrentContext(void);
void SDL_GL_SwapWindow(SDL_Window *);
const char *SDL_GetError(void);
'''

FIXTURE = r'''
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "gl_rpc.h"

static SDL_Window window;
static SDL_GLContext context=(void *)(uintptr_t)123;
static _Thread_local SDL_GLContext current;
static pthread_mutex_t driver_lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_t bound_thread;
static int bound;
static unsigned swaps;
static int (*backup_cb)(void *),(*restore_cb)(void *);
static void *callback_argument;
static _Atomic int fail_bind;
int SDL_GL_MakeCurrent(SDL_Window *w, SDL_GLContext c) {
    assert(w==&window);
    pthread_mutex_lock(&driver_lock);
    if(c && atomic_exchange(&fail_bind,0)) { pthread_mutex_unlock(&driver_lock);return -1; }
    if(c) { assert(c==context);assert(!bound || pthread_equal(bound_thread,pthread_self()));
        bound=1;bound_thread=pthread_self(); }
    else if(current) { assert(bound && pthread_equal(bound_thread,pthread_self()));bound=0; }
    current=c;pthread_mutex_unlock(&driver_lock);return 0;
}
SDL_GLContext SDL_GL_GetCurrentContext(void) { return current; }
void SDL_GL_SwapWindow(SDL_Window *w) { assert(w==&window && current==context);++swaps; }
const char *SDL_GetError(void) { return "injected bind failure"; }
void SDL_WumpaSetGLContextCallbacks(int (*backup)(void *),int (*restore)(void *),void *arg) {
    backup_cb=backup;restore_cb=restore;callback_argument=arg;
}
static unsigned calls, expected[2];
static _Atomic int simultaneous;
typedef struct { unsigned caller,sequence; const char *input; unsigned output; } Borrowed;
static void nested(void *argument) { assert(wumpa_gl_rpc_owner());*(unsigned *)argument=17; }
static void operation(void *argument) {
    Borrowed *value=argument;
    assert(wumpa_gl_rpc_owner());assert(current==context);
    assert(atomic_fetch_add(&simultaneous,1)==0);
    assert(value->sequence==expected[value->caller]++);
    assert(!strcmp(value->input,"borrowed stack bytes"));
    unsigned nested_result=0;wumpa_gl_rpc_call(nested,&nested_result);assert(nested_result==17);
    value->output=value->sequence ^ 0xabc123u;++calls;
    assert(atomic_fetch_sub(&simultaneous,1)==1);
}
static void *caller(void *arg) {
    unsigned identity=(unsigned)(uintptr_t)arg;
    for(unsigned i=0;i<1000;++i) {
        char text[32]="borrowed stack bytes";
        Borrowed payload={identity,i,text,0};
        wumpa_gl_rpc_call(operation,&payload);
        assert(payload.output==(i ^ 0xabc123u));
        memset(text,0xa5,sizeof(text)); /* Caller may reuse memory immediately after completion. */
    }
    return NULL;
}
static void direct(void *argument) { *(unsigned *)argument=42; }
int main(void) {
    assert(!wumpa_gl_rpc_active());unsigned value=0;
    wumpa_gl_rpc_call(direct,&value);assert(value==42);
    assert(!SDL_GL_MakeCurrent(&window,context));
    unsetenv("WRATH_GL_RPC");assert(!wumpa_gl_rpc_start(&window,context));
    assert(!wumpa_gl_rpc_active() && current==context);
    assert(!setenv("WRATH_GL_RPC","1",1));
    assert(!wumpa_gl_rpc_start(&window,context));assert(wumpa_gl_rpc_active());
    assert(!wumpa_gl_rpc_owner());assert(!current);
    pthread_t a,b;assert(!pthread_create(&a,NULL,caller,(void *)(uintptr_t)0));
    assert(!pthread_create(&b,NULL,caller,(void *)(uintptr_t)1));
    assert(!pthread_join(a,NULL));assert(!pthread_join(b,NULL));
    assert(calls==2000 && expected[0]==1000 && expected[1]==1000);
    assert(!wumpa_gl_rpc_swap(&window));assert(swaps==1);
    assert(backup_cb && restore_cb);
    assert(!backup_cb(callback_argument));assert(!bound);
    assert(!restore_cb(callback_argument));assert(bound);
    assert(!wumpa_gl_rpc_swap(&window));assert(swaps==2);
    wumpa_gl_rpc_stop();assert(!wumpa_gl_rpc_active());
    assert(current==context && bound && !backup_cb && !restore_cb);
    value=0;wumpa_gl_rpc_call(direct,&value);assert(value==42);
    atomic_store(&fail_bind,1);
    assert(wumpa_gl_rpc_start(&window,context)==-1);
    assert(!wumpa_gl_rpc_active() && current==context);
    assert(!wumpa_gl_rpc_start(&window,context));
    assert(restore_cb && backup_cb);
    assert(!backup_cb(callback_argument));
    atomic_store(&fail_bind,1);assert(restore_cb(callback_argument)==-1);
    assert(!restore_cb(callback_argument));
    wumpa_gl_rpc_stop();assert(current==context);
    puts("PASS: RPC stack lifetimes, output visibility, 2 callers, nested dispatch, swap, pause/resume, stop");
    return 0;
}
'''

def main():
    with tempfile.TemporaryDirectory() as directory:
        work=Path(directory)
        (work/'SDL.h').write_text(SDL)
        (work/'fixture.c').write_text(FIXTURE)
        binary=work/'fixture'
        subprocess.run(['cc','-std=c11','-O1','-g','-fsanitize=address,undefined','-pthread',
                        '-I'+str(work),'-I'+str(HERE),str(work/'fixture.c'),
                        str(HERE/'gl_rpc.c'),'-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True,timeout=20)

if __name__=='__main__':main()
