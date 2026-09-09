#!/usr/bin/env python3
"""Exercise actual generated GL wrappers against deterministic driver observers."""
import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]


def main():
    spec = importlib.util.spec_from_file_location('shield_prepare', ROOT / 'android/shield2019/prepare.py')
    prepare = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(prepare)
    original = prepare.OUT
    ndk = Path(os.environ.get('ANDROID_NDK_HOME', str(Path.home() / 'Library/Android/sdk/ndk/27.1.12297006')))
    khr = ndk / 'toolchains/llvm/prebuilt/darwin-x86_64/sysroot/usr/include/KHR/khrplatform.h'
    with tempfile.TemporaryDirectory() as directory:
        work = Path(directory)
        (work / 'title').symlink_to(original / 'title', target_is_directory=True)
        (work / 'runtime').symlink_to(original / 'runtime', target_is_directory=True)
        prepare.OUT = work
        prepare.generate_loader(original / 'deps/glcorearb.h')
        (work / 'gl/KHR').mkdir()
        shutil.copy2(khr, work / 'gl/KHR/khrplatform.h')
        fixture = work / 'fixture.c'
        fixture.write_text(r'''
#include <assert.h>
#include <stdint.h>
#include "gl_loader.c"
static unsigned transitions,clears,depths;
static GLenum sequence[1024];
int xbox_D3D8GLBeginCall(void) { return 1; }
int xbox_D3D8GLBeginStateCall(void) { return 1; }
int xbox_D3D8GLEnsureCurrent(void) { return 1; }
void xbox_D3D8GLEndCall(int outer) { assert(outer==1); }
void wumpa_gl_rpc_call(void (*function)(void *),void *argument) { function(argument); }
void wumpa_gl_rpc_call_named(const char *name,void (*function)(void *),void *argument) { (void)name;function(argument); }
void *SDL_GL_GetProcAddress(const char *name) { (void)name;return NULL; }
static void enabled(GLenum cap) { sequence[transitions++]=cap; }
static void disabled(GLenum cap) { sequence[transitions++]=cap+1; }
static void depth(GLdouble near_value,GLdouble far_value) { (void)near_value;(void)far_value;++depths; }
static void clear(GLbitfield bits) { (void)bits;++clears; }
static GLenum pending_error;
static GLenum error(void) { GLenum result=pending_error;pending_error=0;return result; }
static unsigned query_count,query_before;
static void integer_query(GLenum pname,GLint *output) {
    assert(transitions==query_before+1); /* Prior deferred state must be visible. */
    const GLenum expected[]={GL_VIEWPORT,0xbadu,GL_MAX_TEXTURE_SIZE};
    assert(query_count<3 && pname==expected[query_count++]);
    if(pname==GL_VIEWPORT) { for(int i=0;i<4;++i)output[i]=10+i; }
    else if(pname==0xbadu)pending_error=GL_INVALID_ENUM;
    else *output=4096;
}

static GLuint active_program,bound_sampler;
static unsigned shader_events;
static int shader_order[32];
static void program(GLuint value) { active_program=value;shader_order[shader_events++]=100+(int)value; }
static void sampler_bind(GLuint unit,GLuint sampler) {
    assert(unit==2);bound_sampler=sampler;shader_order[shader_events++]=200+(int)sampler;
}
static void sampler_parameter(GLuint sampler,GLenum pname,GLint value) {
    assert(sampler==7 && bound_sampler==7);
    if(pname==0xdead) { pending_error=GL_INVALID_ENUM;return; }
    assert(pname==GL_TEXTURE_MIN_FILTER && value==GL_NEAREST);
    shader_order[shader_events++]=300;
}
static void uniform_i(GLint location,GLint value) {
    assert(location==5 && value==6);shader_order[shader_events++]=400+(int)active_program;
}
static void uniform_f(GLint location,GLfloat value) {
    assert(location==6 && value==0.25f);shader_order[shader_events++]=500+(int)active_program;
}
static void uniform_2i(GLint location,GLint x,GLint y) {
    assert(location==7 && x==8 && y==9);shader_order[shader_events++]=600+(int)active_program;
}
static void uniform_2f(GLint location,GLfloat x,GLfloat y) {
    assert(location==8 && x==0.5f && y==0.75f);shader_order[shader_events++]=700+(int)active_program;
}
static void uniform_4f(GLint location,GLfloat x,GLfloat y,GLfloat z,GLfloat w) {
    assert(location==9 && x==1 && y==2 && z==3 && w==4);
    shader_order[shader_events++]=800+(int)active_program;
}
static void sampler_pointer(GLuint sampler,GLenum pname,const GLfloat *values) {
    assert(sampler==7 && pname==GL_TEXTURE_BORDER_COLOR && values[0]==0.25f);
    /* Pointer-bearing setter is an immediate barrier after all scalar setters. */
    assert(shader_events==11);shader_order[shader_events++]=900;
}
int main(void) {
    assert(!setenv("WRATH_EGL_DEFER_STATE","1",1));
    wumpa_glEnable=enabled;wumpa_glDisable=disabled;wumpa_glDepthRange=depth;
    wumpa_glClear=clear;wumpa_glGetError=error;
    glEnable(GL_BLEND);glEnable(GL_BLEND);assert(wumpa_state_count==1);
    glDisable(GL_BLEND);glEnable(GL_BLEND);assert(wumpa_state_count==3);
    assert(glGetError()==GL_NO_ERROR && transitions==3);
    assert(sequence[0]==GL_BLEND && sequence[1]==GL_BLEND+1 && sequence[2]==GL_BLEND);
    glEnable(GL_BLEND);assert(wumpa_state_count==1);glGetError();assert(transitions==4);
    glDepthRange(0.0,1.0);glDepthRange(-0.0,1.0);assert(wumpa_state_count==2);
    uint64_t bits=UINT64_C(0x7ff8000000000001);double nan;memcpy(&nan,&bits,8);
    glDepthRange(nan,1);glDepthRange(nan,1);assert(wumpa_state_count==3);
    ++bits;memcpy(&nan,&bits,8);glDepthRange(nan,1);assert(wumpa_state_count==4);
    glGetError();assert(depths==4);
    for(unsigned i=0;i<256;++i) { if(i&1)glEnable(GL_BLEND);else glDisable(GL_BLEND); }
    assert(wumpa_state_count==256);unsigned before=transitions;
    glEnable(GL_BLEND);assert(wumpa_state_count==256 && transitions==before);
    glDisable(GL_BLEND);assert(wumpa_state_count==1 && transitions==before+256);
    glClear(GL_COLOR_BUFFER_BIT);glClear(GL_COLOR_BUFFER_BIT);
    assert(wumpa_state_count==0 && transitions==before+257 && clears==2);
    wumpa_glGetIntegerv=integer_query;query_before=transitions;
    glEnable(GL_DEPTH_TEST);assert(wumpa_state_count==1);
    wumpa_gl_get_integers(NULL,0);assert(wumpa_state_count==1 && transitions==query_before);
    GLint viewport[6]={77,0,0,0,0,88},invalid=123,limit=0;
    const WumpaGLIntegerQuery queries[]={
        {GL_VIEWPORT,viewport+1},{0xbadu,&invalid},{GL_MAX_TEXTURE_SIZE,&limit}};
    wumpa_gl_get_integers(queries,3);
    assert(query_count==3 && wumpa_state_count==0);
    assert(viewport[0]==77 && viewport[5]==88 && invalid==123 && limit==4096);
    for(int i=0;i<4;++i)assert(viewport[i+1]==10+i);
    assert(glGetError()==GL_INVALID_ENUM && glGetError()==GL_NO_ERROR);

    wumpa_glUseProgram=program;wumpa_glBindSampler=sampler_bind;
    wumpa_glSamplerParameteri=sampler_parameter;wumpa_glSamplerParameterfv=sampler_pointer;
    wumpa_glUniform1i=uniform_i;wumpa_glUniform1f=uniform_f;
    wumpa_glUniform2i=uniform_2i;wumpa_glUniform2f=uniform_2f;wumpa_glUniform4f=uniform_4f;
    glUseProgram(1);glUniform1i(5,6);glUniform1i(5,6);
    glUseProgram(2);glUniform1i(5,6); /* Same args, different program: must execute. */
    glBindSampler(2,7);glBindSampler(2,7);
    glSamplerParameteri(7,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glUniform1f(6,0.25f);glUniform2i(7,8,9);glUniform2f(8,0.5f,0.75f);
    glUniform4f(9,1,2,3,4);glUniform4f(9,1,2,3,4);
    glUseProgram(3);
    assert(shader_events==0 && wumpa_state_count==11);
    GLfloat border[4]={0.25f,0,0,0};glSamplerParameterfv(7,GL_TEXTURE_BORDER_COLOR,border);
    const int expected_shader[]={101,401,102,402,207,300,502,602,702,802,103,900};
    assert(shader_events==12 && wumpa_state_count==0);
    assert(!memcmp(shader_order,expected_shader,sizeof(expected_shader)));
    glSamplerParameteri(7,0xdead,0);assert(pending_error==0 && wumpa_state_count==1);
    assert(glGetError()==GL_INVALID_ENUM && wumpa_state_count==0);
    assert(glGetError()==GL_NO_ERROR);
    return 0;
}
''')
        sdl = original / 'deps' / f'SDL-{prepare.SDL_REV}' / 'include'
        binary = work / 'fixture'
        subprocess.run(['cc', '-std=c11', '-O1', '-fsanitize=undefined', '-I'+str(work / 'gl'),
                        '-I'+str(sdl), '-I'+str(ROOT / 'android/shield2019'), str(fixture), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
    print('PASS: scalar sampler/uniform program order, pointer/error barriers, exact duplicates, signed zero/NaNs and capacity')


if __name__ == '__main__':
    main()
