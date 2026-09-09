#!/usr/bin/env python3
"""Test the real synchronous render RPC with real threads and a fake SDL context."""
import importlib.util
import os
import shutil
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
void *SDL_GL_GetProcAddress(const char *);
'''

FIXTURE = r'''
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "gl_rpc.h"
#include "gl_loader.c"
int xbox_D3D8GLBeginCall(void) { return 1; }
int xbox_D3D8GLBeginStateCall(void) { return 1; }
int xbox_D3D8GLEnsureCurrent(void) { return 1; }
void xbox_D3D8GLEndCall(int outer) { assert(outer==1); }
void *SDL_GL_GetProcAddress(const char *name) { (void)name;return NULL; }

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
static void buffer_data(GLenum target,GLsizeiptr size,const void *data,GLenum usage) {
    assert(wumpa_gl_rpc_owner() && current==context);
    assert(target==GL_ARRAY_BUFFER && size==5 && usage==GL_STATIC_DRAW);
    assert(!memcmp(data,"hello",5));
}
static void shader_source(GLuint shader,GLsizei count,const GLchar *const *strings,const GLint *lengths) {
    assert(wumpa_gl_rpc_owner() && shader==71 && count==2);
    assert(lengths[0]==3 && lengths[1]==4);
    assert(!memcmp(strings[0],"one",3) && !memcmp(strings[1],"four",4));
}
static void get_integer(GLenum pname,GLint *result) {
    assert(wumpa_gl_rpc_owner() && pname==GL_MAX_TEXTURE_SIZE);*result=4096;
}
static const GLubyte *get_string(GLenum name) {
    assert(wumpa_gl_rpc_owner() && name==GL_VENDOR);return (const GLubyte *)"mock GPU";
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
    wumpa_glBufferData=buffer_data;wumpa_glShaderSource=shader_source;
    wumpa_glGetIntegerv=get_integer;wumpa_glGetString=get_string;
    char bytes[]="hello";glBufferData(GL_ARRAY_BUFFER,5,bytes,GL_STATIC_DRAW);
    memset(bytes,0,sizeof(bytes));
    const GLchar *strings[]={"one","four"};GLint lengths[]={3,4};
    glShaderSource(71,2,strings,lengths);
    GLint result=0;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&result);assert(result==4096);
    assert(!strcmp((const char *)glGetString(GL_VENDOR),"mock GPU"));
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
    spec=importlib.util.spec_from_file_location('shield_prepare',HERE/'prepare.py')
    prepare=importlib.util.module_from_spec(spec);spec.loader.exec_module(prepare)
    original=prepare.OUT
    ndk=Path(os.environ.get('ANDROID_NDK_HOME',str(Path.home()/'Library/Android/sdk/ndk/27.1.12297006')))
    with tempfile.TemporaryDirectory() as directory:
        work=Path(directory)
        (work/'title').symlink_to(original/'title',target_is_directory=True)
        (work/'runtime').symlink_to(original/'runtime',target_is_directory=True)
        prepare.OUT=work;prepare.generate_loader(original/'deps/glcorearb.h')
        (work/'gl/KHR').mkdir()
        shutil.copy2(ndk/'toolchains/llvm/prebuilt/darwin-x86_64/sysroot/usr/include/KHR/khrplatform.h',work/'gl/KHR/khrplatform.h')
        (work/'SDL.h').write_text(SDL)
        (work/'fixture.c').write_text(FIXTURE)
        binary=work/'fixture'
        subprocess.run(['cc','-std=c11','-O1','-g','-fsanitize=address,undefined','-pthread',
                        '-I'+str(work),'-I'+str(work/'gl'),'-I'+str(HERE),str(work/'fixture.c'),
                        str(HERE/'gl_rpc.c'),'-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True,timeout=20)

if __name__=='__main__':main()
