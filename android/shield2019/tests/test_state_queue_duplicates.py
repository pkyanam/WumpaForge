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
void *SDL_GL_GetProcAddress(const char *name) { (void)name;return NULL; }
static void enabled(GLenum cap) { sequence[transitions++]=cap; }
static void disabled(GLenum cap) { sequence[transitions++]=cap+1; }
static void depth(GLdouble near_value,GLdouble far_value) { (void)near_value;(void)far_value;++depths; }
static void clear(GLbitfield bits) { (void)bits;++clears; }
static GLenum error(void) { return GL_NO_ERROR; }
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
    return 0;
}
''')
        sdl = original / 'deps' / f'SDL-{prepare.SDL_REV}' / 'include'
        binary = work / 'fixture'
        subprocess.run(['cc', '-std=c11', '-O1', '-fsanitize=undefined', '-I'+str(work / 'gl'),
                        '-I'+str(sdl), str(fixture), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
    print('PASS: adjacent exact duplicates, state order, signed zero/NaNs, barriers, capacity and non-idempotent calls')


if __name__ == '__main__':
    main()
