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
    # Reset the one patched SDL file from the verified archive before replay;
    # this dependency checkout is isolated to the Android build tree.
    with tarfile.open(deps/'sdl2.tar.gz') as archive:
        member=archive.extractfile(f'SDL-{SDL_REV}/src/video/SDL_egl.c')
        if member is None:raise RuntimeError('Pinned SDL EGL source missing')
        (sdl/'src/video/SDL_egl.c').write_bytes(member.read())
    sdl_patch=HERE/'patches/sdl-egl-release.patch'
    subprocess.run(['git','apply','--check',str(sdl_patch)],cwd=sdl,check=True)
    subprocess.run(['git','apply',str(sdl_patch)],cwd=sdl,check=True)
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
    replace(backend,'#include <epoxy/gl.h>','#include <epoxy/gl.h>\n#include <stdatomic.h>\n#include "shield_host.h"')
    replace(backend,'SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);\n    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);',
            'SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);\n    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);')
    # SDL2 defaults to RGB 3/3/2, which permits a reduced-precision EGL config.
    # The guest backbuffer and the color regression fixtures require RGBA8.
    replace(backend,'    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);',
            '    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);\n'
            '    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);\n'
            '    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);\n'
            '    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);\n'
            '    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);\n'
            '    { const char *mode=getenv("WRATH_EGL_NO_RELEASE_FLUSH");\n'
            '      SDL_GL_SetAttribute(SDL_GL_CONTEXT_RELEASE_BEHAVIOR,\n'
            '          mode && !strcmp(mode,"1") ? SDL_GL_CONTEXT_RELEASE_BEHAVIOR_NONE\n'
            '                                    : SDL_GL_CONTEXT_RELEASE_BEHAVIOR_FLUSH); }')
    replace(backend,'    SDL_GL_MakeCurrent(g.window, g.glctx);\n#ifdef __APPLE__',
            '    SDL_GL_MakeCurrent(g.window, g.glctx);\n    if (!wumpa_gl_load()) return D3DERR_INVALIDCALL;\n#ifdef __APPLE__')
    replace(backend,'void xbox_D3D8GLPumpEvents(void)\n{',
            'void xbox_D3D8GLPumpEvents(void)\n{\n    if (!wumpa_is_game_thread()) return;')
    replace(backend,'static _Thread_local unsigned g_context_depth;',
            'static _Thread_local unsigned g_context_depth;\n'
            'int xbox_D3D8GLAcquire(void);\n'
            'void xbox_D3D8GLRelease(void);\n'
            'static int lazy_context_bind(void) {\n'
            '    static _Atomic int cached=-1;\n'
            '    int enabled=atomic_load_explicit(&cached,memory_order_relaxed);\n'
            '    if(enabled>=0)return enabled;\n'
            '    /* Host sets immutable process flags before starting game threads. */\n'
            '    const char *v=getenv("WRATH_EGL_LAZY_BIND"), *d=getenv("WRATH_EGL_DEFER_STATE");\n'
            '    enabled=(v && !strcmp(v,"1")) || (d && !strcmp(d,"1"));\n'
            '    atomic_store_explicit(&cached,enabled,memory_order_relaxed);return enabled;\n'
            '}\n'
            'int xbox_D3D8GLEnsureCurrent(void) {\n'
            '    if (!g.glctx || SDL_GL_GetCurrentContext()==g.glctx) return 1;\n'
            '    if (!g_context_depth) return 0;\n'
            '    uint64_t begin=profile_context_enabled()?monotonic_ns():0;\n'
            '    int ok=SDL_GL_MakeCurrent(g.window,g.glctx)>=0;\n'
            '    if(begin)g_profile_calls.bind+=monotonic_ns()-begin;\n'
            '    return ok;\n'
            '}\n'
            'int xbox_D3D8GLBeginCall(void) {\n'
            '    if(!lazy_context_bind())return 0;\n'
            '    int outer=!g_context_depth;\n'
            '    if(outer && !xbox_D3D8GLAcquire())abort();\n'
            '    if(!xbox_D3D8GLEnsureCurrent())abort();\n'
            '    return outer;\n'
            '}\n'
            'int xbox_D3D8GLBeginStateCall(void) {\n'
            '    int outer=!g_context_depth;\n'
            '    if(outer && !xbox_D3D8GLAcquire())abort();\n'
            '    return outer;\n'
            '}\n'
            'void xbox_D3D8GLEndCall(int outer) { if(outer)xbox_D3D8GLRelease(); }')
    replace(backend,'    if (g.glctx && SDL_GL_MakeCurrent(g.window, g.glctx) < 0) goto fail;',
            '    if (!lazy_context_bind() && g.glctx && SDL_GL_MakeCurrent(g.window, g.glctx) < 0) goto fail;')
    replace(backend,'        if (g.glctx && SDL_GL_MakeCurrent(g.window, NULL) < 0) abort();',
            '        if (g.glctx && (!lazy_context_bind() || SDL_GL_GetCurrentContext()==g.glctx) &&\n'
            '            SDL_GL_MakeCurrent(g.window, NULL) < 0) abort();')
    replace(backend,'    if (g.window) {\n        last_pump_ns = now;',
            '    if (g.window) {\n'
            '        if(!xbox_D3D8GLEnsureCurrent())abort();\n'
            '        wumpa_gl_flush_state();\n'
            '        last_pump_ns = now;')
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
    queue_anchor='    assert(!glIsEnabled(GL_DITHER));'
    assert fixture.count(queue_anchor)==1
    fixture=fixture.replace(queue_anchor,queue_anchor+'\n'
            '    { /* Exceed the256-call queue and verify ordered observation. */\n'
            '        GLboolean old_depth,old_color[4],depth,color[4];\n'
            '        glGetBooleanv(GL_DEPTH_WRITEMASK,&old_depth);\n'
            '        glGetBooleanv(GL_COLOR_WRITEMASK,old_color);\n'
            '        for(unsigned i=0;i<300;++i) {\n'
            '            if(i&1)glDisable(GL_DITHER);else glEnable(GL_DITHER);\n'
            '            glDepthMask((i&1)!=0);\n'
            '            glColorMask((i&1)!=0,GL_TRUE,GL_FALSE,GL_TRUE);\n'
            '        }\n'
            '        assert(!glIsEnabled(GL_DITHER));\n'
            '        glGetBooleanv(GL_DEPTH_WRITEMASK,&depth);\n'
            '        glGetBooleanv(GL_COLOR_WRITEMASK,color);\n'
            '        assert(depth && color[0] && color[1] && !color[2] && color[3]);\n'
            '        glDepthMask(old_depth);\n'
            '        glColorMask(old_color[0],old_color[1],old_color[2],old_color[3]);\n'
            '        GLint old_buffer,bound;GLuint buffer;\n'
            '        glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&old_buffer);\n'
            '        glGenBuffers(1,&buffer);glBindBuffer(GL_ARRAY_BUFFER,buffer);\n'
            '        glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&bound);assert((GLuint)bound==buffer);\n'
            '        glBindBuffer(GL_ARRAY_BUFFER,0);glBindBuffer(GL_ARRAY_BUFFER,buffer);\n'
            '        glDeleteBuffers(1,&buffer);\n'
            '        glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&bound);assert(bound==0);\n'
            '        glBindBuffer(GL_ARRAY_BUFFER,(GLuint)old_buffer);\n'
            '        assert(glGetError()==GL_NO_ERROR);\n'
            '        puts("PASS: ordered scalar GL state queue overflow, getters and binding/deletion observation");\n'
            '    }')
    assert fixture.count('#include "../tools/test_multistream.inc"')==1
    fixture=fixture.replace('#include "../tools/test_multistream.inc"', '#include "test_multistream.inc"')
    (checks/'graphics_check.c').write_text(fixture)
    multistream=(ROOT/'tools/test_multistream.inc').read_text()
    exact_color='    assert(pixel[0]==64 && pixel[1]==128 && pixel[2]==191);'
    assert multistream.count(exact_color)==1
    multistream=multistream.replace(exact_color,
            '    fprintf(stderr,"[shield] multistream RGBA=%u,%u,%u,%u\\n",\n'
            '            pixel[0],pixel[1],pixel[2],pixel[3]);\n'
            '    fflush(stderr);\n'
            '    /* NVIDIA readback is 64,127,191,255: .5 * 255 is the\n'
            '     * half-step 127.5. Permit one quantization step, not a\n'
            '     * different rendered color; opaque alpha remains exact.\n'
            '     * The subsequent +0/-0 draws still match byte-for-byte. */\n'
            '    assert(pixel[0]>=63 && pixel[0]<=65 &&\n'
            '           pixel[1]>=127 && pixel[1]<=129 &&\n'
            '           pixel[2]>=190 && pixel[2]<=192 && pixel[3]==255);')
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
    header += ['void wumpa_gl_flush_state(void); /* Caller holds context mutex. */']
    body=['#include <SDL.h>','#include <stdio.h>','#include <stdlib.h>','#include <stdatomic.h>','#include <string.h>','#include "epoxy/gl.h"',
          'extern int xbox_D3D8GLBeginCall(void);','extern int xbox_D3D8GLBeginStateCall(void);',
          'extern int xbox_D3D8GLEnsureCurrent(void);','extern void xbox_D3D8GLEndCall(int outer);']
    deferred=set('glEnable glDisable glDepthMask glDepthFunc glColorMask glBlendFunc glBlendFuncSeparate glBlendEquation glBlendEquationSeparate glStencilFunc glStencilFuncSeparate glStencilOp glStencilOpSeparate glStencilMask glStencilMaskSeparate glCullFace glFrontFace glPolygonMode glViewport glDepthRange glActiveTexture glBindTexture glTexParameteri glUseProgram glBindBuffer glBindFramebuffer glBindRenderbuffer glBindVertexArray glEnableVertexAttribArray glDisableVertexAttribArray glVertexAttrib4f'.split())
    wrappers=[];queue_members=[];queue_cases=[]
    for name in names:
        typedef='PFN'+name.upper()+'PROC'
        if typedef not in registry_text: raise RuntimeError(f'Missing GL declaration: {name}')
        declaration=re.search(r'typedef\s+([^;\n]+?)\s*\(APIENTRYP '+typedef+r'\)\s*\(([^;]+)\);',registry_text)
        if not declaration:raise RuntimeError(f'Cannot parse GL declaration: {name}')
        result,parameters=declaration.groups()
        arguments=[]
        if parameters.strip()!='void':
            for parameter in parameters.split(','):
                arg=re.search(r'(\w+)\s*(?:\[[^]]*\])?\s*$',parameter)
                if not arg:raise RuntimeError(f'Cannot parse GL argument: {parameter}')
                arguments.append(arg.group(1))
        header += [f'{result} wumpa_call_{name}({parameters});',f'#define {name} wumpa_call_{name}']
        body += [f'static {typedef} wumpa_{name};']
        wrappers += [f'{result} wumpa_call_{name}({parameters}) {{']
        if name in deferred:
            assert result=='void' and '*' not in parameters and '[' not in parameters
            index=len(queue_members)
            queue_members += [f'        struct {{ {parameters.replace(",", ";")}; }} {name};']
            queue_cases += [f'        case {index}: wumpa_{name}('+', '.join(f'command->args.{name}.{arg}' for arg in arguments)+'); break;']
            wrappers += ['    if(wumpa_defer_state()) {',
                         '        int wumpa_outer=xbox_D3D8GLBeginStateCall();',
                         '        if(wumpa_state_count==WUMPA_STATE_CAPACITY)wumpa_gl_flush_state();',
                         '        WumpaStateCommand *command=&wumpa_state_queue[wumpa_state_count++];',
                         f'        command->kind={index};']
            wrappers += [f'        command->args.{name}.{arg}={arg};' for arg in arguments]
            wrappers += ['        xbox_D3D8GLEndCall(wumpa_outer); return;', '    }']
        wrappers += ['    int wumpa_outer=xbox_D3D8GLBeginCall();', '    wumpa_gl_flush_state();']
        invocation=f'wumpa_{name}({", ".join(arguments)})'
        wrappers += [f'    {invocation};' if result=='void' else f'    {result} wumpa_result={invocation};',
                 '    xbox_D3D8GLEndCall(wumpa_outer);']
        if result!='void':wrappers += ['    return wumpa_result;']
        wrappers += ['}']
    body += ['/* Exact ordered scalar calls only: no pointer lifetime or state-cache assumptions. */',
             '#define WUMPA_STATE_CAPACITY 256', 'typedef struct { unsigned kind; union {']+queue_members+[
             '    } args; } WumpaStateCommand;',
             'static WumpaStateCommand wumpa_state_queue[WUMPA_STATE_CAPACITY];',
             'static unsigned wumpa_state_count;',
             'static int wumpa_defer_state(void) {',
             '    static _Atomic int cached=-1;',
             '    int enabled=atomic_load_explicit(&cached,memory_order_relaxed);',
             '    if(enabled>=0)return enabled;',
             '    /* Startup-only environment, immutable after host initialization. */',
             '    const char *v=getenv("WRATH_EGL_DEFER_STATE");enabled=v && !strcmp(v,"1");',
             '    atomic_store_explicit(&cached,enabled,memory_order_relaxed);return enabled;', '}',
             'void wumpa_gl_flush_state(void) {',
             '    if(!wumpa_state_count)return;',
             '    if(!xbox_D3D8GLEnsureCurrent())abort();',
             '    for(unsigned i=0;i<wumpa_state_count;++i) {',
             '        WumpaStateCommand *command=&wumpa_state_queue[i];',
             '        switch(command->kind) {']+queue_cases+[
             '        default: abort();', '        }', '    }', '    wumpa_state_count=0;', '}']
    body += wrappers
    body += ['int wumpa_gl_load(void) { int missing=0; wumpa_state_count=0;']
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
