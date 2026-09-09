#!/usr/bin/env python3
"""Prepare isolated, pinned Android dependencies and a copy of the Mac runtime."""
from pathlib import Path
import hashlib, json, re, shutil, subprocess, tarfile, urllib.request
ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
OUT = ROOT / 'build/shield2019'
SDL_REV = '5d249570393f7a37e037abf22cd6012a4cc56a71'
GL_REV = '1cdd228e34966dd6b95bd203e9f84faba0f371a1'
DEPS = {
    'sdl2.tar.gz': (f'https://github.com/libsdl-org/SDL/archive/{SDL_REV}.tar.gz', '10f1194f8d2e4a73ca1c7c553b3189d0b68ab9ef3544e8d2a268e4429456b373'),
    'glcorearb.h': (f'https://raw.githubusercontent.com/KhronosGroup/OpenGL-Registry/{GL_REV}/api/GL/glcorearb.h', '65fae555a8b3b5709099837001e0c14228b3e45651e2d0fa2ad22649833921e1'),
}
def replace(path, old, new):
    value = path.read_text()
    if value.count(old) != 1:
        raise RuntimeError(f'Source anchor changed: {path}: {old[:65]}')
    path.write_text(value.replace(old,new))
def main():
    deps=OUT/'deps'; deps.mkdir(parents=True,exist_ok=True)
    for name,(url,digest) in DEPS.items():
        file=deps/name
        if not file.exists():
            temporary=file.with_suffix('.download')
            urllib.request.urlretrieve(url,temporary)
            if hashlib.sha256(temporary.read_bytes()).hexdigest()!=digest:
                raise RuntimeError(f'Download checksum mismatch: {name}')
            temporary.replace(file)
        if hashlib.sha256(file.read_bytes()).hexdigest()!=digest:
            raise RuntimeError(f'Existing dependency checksum mismatch: {name}')
    sdl=deps/f'SDL-{SDL_REV}'
    if not sdl.exists():
        with tarfile.open(deps/'sdl2.tar.gz') as archive:
            archive.extractall(deps,filter='data')
    # Source copies are disposable build inputs; the original Mac checkout stays intact.
    runtime_source=OUT/'runtime/src'
    if runtime_source.is_symlink():raise RuntimeError('Unexpected runtime source symlink')
    if runtime_source.exists():shutil.rmtree(runtime_source)
    for source,dest in [(ROOT/'third_party/xboxrecomp/src',OUT/'runtime/src'),(ROOT/'src',OUT/'title')]:
        shutil.copytree(source,dest,dirs_exist_ok=True)
    for patch in sorted((HERE/'patches').glob('runtime-*.patch')):
        subprocess.run(['git','apply','--check',str(patch)],cwd=OUT/'runtime',check=True)
        subprocess.run(['git','apply',str(patch)],cwd=OUT/'runtime',check=True)
    backend=OUT/'runtime/src/d3d/d3d8_gl.c'
    replace(backend,'#include <epoxy/gl.h>','#include <epoxy/gl.h>\n#include "shield_host.h"')
    replace(backend,'SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);\n    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);',
            'SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);\n    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);')
    # SDL2 defaults to RGB 3/3/2, which permits a reduced-precision EGL config.
    # The guest backbuffer and the color regression fixtures require RGBA8.
    replace(backend,'    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);',
            '    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);\n'
            '    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);\n'
            '    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);\n'
            '    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);\n'
            '    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);')
    replace(backend,'    SDL_GL_MakeCurrent(g.window, g.glctx);\n#ifdef __APPLE__',
            '    SDL_GL_MakeCurrent(g.window, g.glctx);\n    if (!wumpa_gl_load()) return D3DERR_INVALIDCALL;\n#ifdef __APPLE__')
    replace(backend,'void xbox_D3D8GLPumpEvents(void)\n{',
            'void xbox_D3D8GLPumpEvents(void)\n{\n    if (!wumpa_is_game_thread()) return;')
    replace(backend,'            output_event(&ev);',
            '            if (ev.type == SDL_RENDER_DEVICE_RESET || SDL_HasEvent(SDL_RENDER_DEVICE_RESET)) {\n'
            '                fprintf(stderr, "[shield] EGL context lost; resource restoration is not implemented. Ending this game process.\\n");\n'
            '                exit(EXIT_FAILURE);\n'
            '            }\n'
            '            output_event(&ev);')
    replace(backend,'    if (!pp || !pPP) return D3DERR_INVALIDCALL;',
            '    if (!pp || !pPP || !wumpa_is_game_thread()) return D3DERR_INVALIDCALL;')
    title=OUT/'title/graphics.c'
    replace(title,'#include <epoxy/gl.h>','#include <epoxy/gl.h>\n#include "shield_host.h"')
    replace(title,'#else\n    return 1;\n#endif\n}', '#else\n    return wumpa_is_game_thread();\n#endif\n}')
    input_runtime=OUT/'runtime/src/input/xinput_device.c'
    replace(input_runtime,'#include <SDL.h>','#include <SDL.h>\n#include "shield_host.h"')
    replace(input_runtime,'    SDL_PumpEvents();','    wumpa_pump_input_events();')
    input_source=OUT/'title/input_bridge.c'
    replace(input_source,'static int main_thread(void)\n{',
            '#include "shield_host.h"\nstatic int main_thread(void)\n{\n    if (!wumpa_is_game_thread()) return 0;')
    main_source=OUT/'title/main.c'
    replace(main_source,'snprintf(save_path, sizeof(save_path), "%s/../saves", resolved);',
            'snprintf(save_path, sizeof(save_path), "%s/saves", getenv("WRATH_STATE_ROOT"));')
    replace(main_source,'snprintf(run_path, sizeof(run_path), "%s/../run", resolved);',
            'snprintf(run_path, sizeof(run_path), "%s/run", getenv("WRATH_STATE_ROOT"));')
    checks=OUT/'checks';checks.mkdir(exist_ok=True)
    fixture=(OUT/'title/graphics.c').read_text()
    assert fixture.count('int main(void)')==1
    fixture=fixture.replace('int main(void)','int wumpa_graphics_checks(void)')
    assert fixture.count('#include "../tools/test_multistream.inc"')==1
    fixture=fixture.replace('#include "../tools/test_multistream.inc"', '#include "test_multistream.inc"')
    (checks/'graphics_check.c').write_text(fixture)
    multistream=(ROOT/'tools/test_multistream.inc').read_text()
    exact_color='    assert(pixel[0]==64 && pixel[1]==128 && pixel[2]==191);'
    assert multistream.count(exact_color)==1
    multistream=multistream.replace(exact_color,
            '    {\n'
            '        int r=-1,g=-1,b=-1,a=-1;\n'
            '        SDL_GL_GetAttribute(SDL_GL_RED_SIZE,&r);\n'
            '        SDL_GL_GetAttribute(SDL_GL_GREEN_SIZE,&g);\n'
            '        SDL_GL_GetAttribute(SDL_GL_BLUE_SIZE,&b);\n'
            '        SDL_GL_GetAttribute(SDL_GL_ALPHA_SIZE,&a);\n'
            '        fprintf(stderr,"[shield] multistream RGBA=%u,%u,%u,%u framebuffer bits=%d,%d,%d,%d\\n",\n'
            '                pixel[0],pixel[1],pixel[2],pixel[3],r,g,b,a);\n'
            '        fflush(stderr);\n'
            '    }\n'+exact_color)
    (checks/'test_multistream.inc').write_text(multistream)
    tools_link=OUT/'tools'
    if tools_link.is_symlink():
        if tools_link.resolve()!=ROOT/'tools':raise RuntimeError('Unexpected tools link')
    elif tools_link.exists():raise RuntimeError('Preserving unexpected tools directory')
    else:tools_link.symlink_to(ROOT/'tools',target_is_directory=True)
    generate_loader(deps/'glcorearb.h')
    manifest={'sdl_revision':SDL_REV,'gl_registry_revision':GL_REV,'dependencies':DEPS,
              'base_source_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
              'patches':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (HERE/'patches').glob('*.patch')}}
    (OUT/'prepared.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(OUT)
def generate_loader(registry):
    dest=OUT/'gl'; (dest/'epoxy').mkdir(parents=True,exist_ok=True)
    (dest/'GL').mkdir(exist_ok=True); shutil.copy2(registry,dest/'GL/glcorearb.h')
    paths=list((OUT/'title').glob('*'))+[OUT/'runtime/src/d3d/d3d8_gl.c']+list((OUT/'runtime/src/d3d').glob('*.inc'))
    paths+=list((ROOT/'tools').glob('*.inc'))
    source='\n'.join(p.read_text() for p in paths if p.suffix in ('.c','.h','.inc'))
    names=sorted(set(re.findall(r'\b(gl[A-Z]\w*)\s*\(',source)) | {'glGetStringi','glGetIntegerv','glGetString'})
    registry_text=registry.read_text()
    header=['/* Generated from pinned Khronos declarations; do not edit. */','#pragma once','#include <GL/glcorearb.h>','#define GL_FLAT 0x1D00 /* Guest shade-mode token; no legacy GL call. */','#define GL_SMOOTH 0x1D01','int wumpa_gl_load(void);','int epoxy_gl_version(void);','int epoxy_has_gl_extension(const char *name);']
    body=['#include <SDL.h>','#include <stdio.h>','#include <string.h>','#include "epoxy/gl.h"']
    for name in names:
        typedef='PFN'+name.upper()+'PROC'
        if typedef not in registry_text: raise RuntimeError(f'Missing GL declaration: {name}')
        header += [f'extern {typedef} wumpa_{name};',f'#define {name} wumpa_{name}']
        body += [f'{typedef} wumpa_{name};']
    body += ['int wumpa_gl_load(void) { int missing=0;']
    for name in names:
        body += [f'    wumpa_{name}=({"PFN"+name.upper()+"PROC"})SDL_GL_GetProcAddress("{name}");',
                 f'    if (!wumpa_{name}) {{ fprintf(stderr,"Missing desktop GL entry: {name}\\n"); ++missing; }}']
    body += ['    return missing==0;','}',
             'int epoxy_gl_version(void) { GLint major=0,minor=0; glGetIntegerv(GL_MAJOR_VERSION,&major); glGetIntegerv(GL_MINOR_VERSION,&minor); return major*10+minor; }',
             'int epoxy_has_gl_extension(const char *name) { GLint n=0; glGetIntegerv(GL_NUM_EXTENSIONS,&n); for(GLint i=0;i<n;i++) { const char *s=(const char*)glGetStringi(GL_EXTENSIONS,(GLuint)i); if(s&&!strcmp(s,name))return 1; } return 0; }']
    (dest/'epoxy/gl.h').write_text('\n'.join(header)+'\n')
    (dest/'gl_loader.c').write_text('\n'.join(body)+'\n')
    print(f'Generated typed loader for {len(names)} desktop GL entry points.')
if __name__=='__main__': main()
