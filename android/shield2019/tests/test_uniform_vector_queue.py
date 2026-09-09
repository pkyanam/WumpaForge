#!/usr/bin/env python3
"""Test real generated vector wrappers with an observing GL driver, ASan/UBSan."""
import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[3]
FIXTURE=r'''
#include <assert.h>
#include <stdint.h>
#include "gl_loader.c"
static unsigned events,program_id,components_expected;
static GLint location_expected;
static GLsizei count_expected;
static GLfloat expected[772];
static const GLfloat *borrowed;
static GLenum pending;
static unsigned programs[2048];
int xbox_D3D8GLBeginCall(void) { return 1; }
int xbox_D3D8GLBeginStateCall(void) { return 1; }
int xbox_D3D8GLEnsureCurrent(void) { return 1; }
void xbox_D3D8GLEndCall(int outer) { assert(outer==1); }
void wumpa_gl_rpc_call(void (*fn)(void *),void *arg) { fn(arg); }
void wumpa_gl_rpc_call_named(const char *n,void (*fn)(void *),void *arg) { (void)n;fn(arg); }
void *SDL_GL_GetProcAddress(const char *n) { (void)n;return NULL; }
static void program(GLuint p) { program_id=p; }
static GLenum error(void) { GLenum p=pending;pending=0;return p; }
static void observe(unsigned width,GLint location,GLsizei count,const GLfloat *values) {
    assert(width==components_expected && location==location_expected && count==count_expected);
    programs[events++]=program_id;
    if(borrowed || !values || count<=0 || count>192) {
        assert(values==borrowed); /* Invalid and oversized calls retain original pointer. */
    } else {
        assert(!memcmp(values,expected,(size_t)count*width*sizeof(GLfloat)));
    }
    if(count<0)pending=GL_INVALID_VALUE;
}
static void u2(GLint l,GLsizei c,const GLfloat *v) { observe(2,l,c,v); }
static void u3(GLint l,GLsizei c,const GLfloat *v) { observe(3,l,c,v); }
static void u4(GLint l,GLsizei c,const GLfloat *v) { observe(4,l,c,v); }
static void submit(unsigned w,GLint l,GLsizei c,const GLfloat *v) {
    if(w==2)glUniform2fv(l,c,v);else if(w==3)glUniform3fv(l,c,v);else glUniform4fv(l,c,v);
}
int main(int argc,char **argv) {
    assert(argc==2);assert(!setenv("WRATH_EGL_DEFER_STATE",argv[1],1));
    int defer=argv[1][0]=='1';
    wumpa_glUseProgram=program;wumpa_glGetError=error;
    wumpa_glUniform2fv=u2;wumpa_glUniform3fv=u3;wumpa_glUniform4fv=u4;
    assert(sizeof(wumpa_state_queue)<1024*1024);
    for(unsigned width=2;width<=4;++width) {
        components_expected=width;location_expected=-1;count_expected=192;
        GLfloat values[770];values[0]=123;values[769]=456;
        for(unsigned i=0;i<768;++i)expected[i]=values[i+1]=(GLfloat)i/8;
        unsigned before=events;
        glUseProgram(11);submit(width,-1,192,values+1);
        glUseProgram(22);submit(width,-1,192,values+1);
        assert(events==before+(defer?0:2));
        memset(values+1,0,768*sizeof(GLfloat)); /* Original lifetime ends before flush. */
        pending=GL_INVALID_ENUM;
        assert(glGetError()==GL_INVALID_ENUM && events==before+2);
        assert(programs[before]==11 && programs[before+1]==22);
        assert(values[0]==123 && values[769]==456 && glGetError()==0);
        for(int which=0;which<4;++which) {
            count_expected=which==0?-1:which==1?0:which==2?193:1;
            borrowed=which==3?NULL:values+1;before=events;
            glUseProgram(33);submit(width,-1,count_expected,borrowed);
            assert(events==before+1 && programs[before]==33 && wumpa_state_count==0);
            assert(glGetError()==(count_expected<0?GL_INVALID_VALUE:0));
        }
        borrowed=NULL;count_expected=1;location_expected=5;
        for(unsigned i=0;i<width;++i)expected[i]=(GLfloat)i;
        before=events;
        for(unsigned i=0;i<257;++i)submit(width,5,1,expected);
        assert(events==before+(defer?256:257));
        assert(wumpa_state_count==(defer?1:0));
        glGetError();assert(events==before+257 && !wumpa_state_count);
    }
    return 0;
}
'''
def main():
    spec=importlib.util.spec_from_file_location('prepare',ROOT/'android/shield2019/prepare.py')
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
        (work/'fixture.c').write_text(FIXTURE)
        sdl=original/'deps'/f'SDL-{prepare.SDL_REV}'/'include'
        subprocess.run(['cc','-std=c11','-O1','-fsanitize=address,undefined','-I'+str(work/'gl'),'-I'+str(sdl),'-I'+str(ROOT/'android/shield2019'),str(work/'fixture.c'),'-o',str(work/'fixture')],check=True)
        for enabled in ('0','1'):subprocess.run([str(work/'fixture'),enabled],check=True)
    print('PASS: real vector wrappers own 2/3/4fv payloads, preserve program/error order and -1 locations, bound/fallback/overflow, defer on/off')
if __name__=='__main__':main()
