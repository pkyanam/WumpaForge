#!/usr/bin/env python3
"""Actual shader wrapper + profile adapter with real RPC and distinct guest TLS."""
from pathlib import Path
import difflib, importlib.util, os, shutil, subprocess, tempfile
HERE=Path(__file__).resolve().parents[1];ROOT=HERE.parents[1]
EXTRA=r'''
typedef int HRESULT;
struct VertexFetch { unsigned first; const unsigned char *indices; int quads; };
static _Thread_local unsigned g_esp=0x9999;
static unsigned read32(unsigned address) { return address ^ 0xabcd; }
static _Thread_local struct { uint64_t draws,draw_time; } g_profile_calls;
PROFILE_ADAPTER
#include "shader_draw_rpc.inc"
static unsigned shader_calls;
static HRESULT shader_draw_impl(GLenum primitive,uint32_t count,const void *vertices,
                                uint32_t stride,const struct VertexFetch *fetch)
{
    assert(primitive==GL_TRIANGLES && stride==4 && fetch->first==37);
    assert(fetch->indices[0]==6 && fetch->quads==1);
    assert(!memcmp(vertices,"abcd",4));++shader_calls;
    assert(shader_guest_return()==(0x1234 ^ 0xabcd));
    if(wumpa_gl_rpc_owner())assert(g_esp==0x9999);
    else assert(g_esp==0x1234);
    if(count==2)return -7; /* Preserve early failures and zero profiling delta. */
    ++g_profile_calls.draws;g_profile_calls.draw_time+=19;
    return 23;
}
#include "shader_draw_rpc_wrap.inc"
static void initialize_shader_owner(void *unused) {
    (void)unused;assert(wumpa_gl_rpc_owner());g_profile_calls.draws=5;g_profile_calls.draw_time=11;
    s_shader_rpc_diagnostic_valid=1;s_shader_rpc_diagnostic_return=0xabcdef;
}
static void inline_shader_owner(void *unused) {
    (void)unused;unsigned char data[]="abcd",indices[]={6};struct VertexFetch fetch={37,indices,1};
    s_shader_rpc_diagnostic_return=0x1234 ^ 0xabcd;
    assert(shader_draw(GL_TRIANGLES,3,data,4,&fetch)==23);
    assert(g_profile_calls.draws==6 && g_profile_calls.draw_time==30);
    g_profile_calls.draws=5;g_profile_calls.draw_time=11;
    s_shader_rpc_diagnostic_return=0xabcdef;
}
static void verify_shader_owner(void *unused) {
    (void)unused;assert(g_profile_calls.draws==5 && g_profile_calls.draw_time==11);
    assert(g_esp==0x9999 && s_shader_rpc_diagnostic_valid && s_shader_rpc_diagnostic_return==0xabcdef);
    s_shader_rpc_diagnostic_valid=0;
}
static void shader_checks(void) {
    g_esp=0x1234;g_profile_calls.draws=7;g_profile_calls.draw_time=13;
    unsigned char data[]="abcd",indices[]={6};struct VertexFetch fetch={37,indices,1};
    setenv("WRATH_GL_RPC_SHADER","1",1);
    if(wumpa_gl_rpc_active())wumpa_gl_rpc_call(initialize_shader_owner,NULL);
    assert(shader_draw(GL_TRIANGLES,3,data,4,&fetch)==23);
    assert(shader_draw(GL_TRIANGLES,2,data,4,&fetch)==-7);
    assert(g_profile_calls.draws==8 && g_profile_calls.draw_time==32);
    assert(g_esp==0x1234 && !s_shader_rpc_diagnostic_valid);
    if(wumpa_gl_rpc_active()) { wumpa_gl_rpc_call(inline_shader_owner,NULL);wumpa_gl_rpc_call(verify_shader_owner,NULL); }
    unsetenv("WRATH_GL_RPC_SHADER");
    assert(shader_draw(GL_TRIANGLES,3,data,4,&fetch)==23);
    assert(g_profile_calls.draws==9 && g_profile_calls.draw_time==51);
    memset(data,0,sizeof(data));memset(indices,0,sizeof(indices));
}
'''
def main():
    build=ROOT/'build';build.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='shader-rpc-test-',dir=build) as name:
        work=Path(name);shutil.copytree(ROOT/'src',work/'title')
        target=work/'runtime/src/d3d';target.mkdir(parents=True)
        shutil.copy2(ROOT/'third_party/xboxrecomp/src/d3d/d3d8_gl.c',target/'d3d8_gl.c')
        env=dict(os.environ,GIT_CEILING_DIRECTORIES=str(build))
        original=None
        for patch in sorted((HERE/'patches').glob('title-*.patch')):
            if patch.name=='title-zzzzzzzz-shader-rpc.patch':original=(work/'title/shader_bridge.inc').read_text()
            subprocess.run(['git','apply','--check',str(patch)],cwd=work/'title',env=env,check=True)
            subprocess.run(['git','apply',str(patch)],cwd=work/'title',env=env,check=True)
        actual=(work/'title/shader_bridge.inc').read_text()
        restored=actual.replace('#include "shader_draw_rpc.inc"\n\n','').replace('shader_guest_return()','read32(g_esp)').replace('shader_draw_impl(','shader_draw(').replace('\n#include "shader_draw_rpc_wrap.inc"\n','')
        assert original==restored,'The original body must be unchanged except diagnostic reads and function name'
        patch=HERE/'patches/runtime-shader-draw-profile.patch'
        subprocess.run(['git','apply',str(patch)],cwd=work/'runtime',env=env,check=True)
        backend=(target/'d3d8_gl.c').read_text()
        start=backend.index('void xbox_D3D8GLProfileHostCall(')
        end=backend.index('/* Some macOS drivers',start)
        adapter=backend[start:end]
    spec=importlib.util.spec_from_file_location('rpc_fixture',HERE/'tests/test_gl_rpc_ordering.py')
    fixture=importlib.util.module_from_spec(spec);spec.loader.exec_module(fixture)
    fixture.FIXTURE=fixture.FIXTURE.replace('int main(void) {',EXTRA.replace('PROFILE_ADAPTER',adapter)+'\nint main(void) {')
    anchor='    pthread_t a,b;assert(!pthread_create(&a,NULL,caller,(void *)(uintptr_t)0));'
    fixture.FIXTURE=fixture.FIXTURE.replace(anchor,'    shader_checks();\n'+anchor)
    anchor='    assert(current==context && bound && !backup_cb && !restore_cb);'
    fixture.FIXTURE=fixture.FIXTURE.replace(anchor,anchor+'\n    shader_checks();assert(shader_calls==7);')
    fixture.main()
    print('PASS: unchanged shader body, all title patches, actual profile adapter, success/failure, borrowed args, distinct guest TLS, diagnostic restoration, owner delta isolation, disabled/inactive modes')
if __name__=='__main__':main()
