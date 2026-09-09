#!/usr/bin/env python3
"""Actual Android sampler callback, generated GL wrappers and pthread RPC."""
from pathlib import Path
import importlib.util, os, shutil, subprocess, tempfile
HERE=Path(__file__).resolve().parents[1]
ROOT=HERE.parents[1]
EXTRA=r'''
#include "sampler_queries.inc"
static unsigned sampler_active,sampler_events[16],sampler_count;
static unsigned sampler_error=0x502;
static GLenum sampler_get_error(void) { GLenum e=sampler_error;sampler_error=0;return e; }
static void sampler_enable(GLenum cap) {
    assert(cap==GL_MULTISAMPLE);sampler_events[sampler_count++]=99;
}
static void sampler_active_texture(GLenum unit) {
    assert(!wumpa_gl_rpc_active() || wumpa_gl_rpc_owner());
    assert(current==context);assert(unit>=GL_TEXTURE0 && unit<GL_TEXTURE0+4);
    sampler_active=unit-GL_TEXTURE0;sampler_events[sampler_count++]=100+sampler_active;
}
static void sampler_integer(GLenum name,GLint *value) {
    assert(name==GL_SAMPLER_BINDING);assert(current==context);
    sampler_events[sampler_count++]=200+sampler_active;*value=17+(int)sampler_active*11;
}
static void check_sampler_queries(void *unused) {
    (void)unused;
    struct { unsigned before;GLint values[4];unsigned after; } result={0x12345678,{0},0xabcdef12};
    sampler_count=0;sampler_active=2;
    glEnable(GL_MULTISAMPLE); /* Must flush before the first query. */
    wumpa_shader_sampler_queries(result.values);
    assert(result.before==0x12345678 && result.after==0xabcdef12);
    assert(sampler_count==9 && sampler_events[0]==99);
    for(unsigned i=0;i<4;++i) {
        assert(result.values[i]==17+(int)i*11);
        assert(sampler_events[1+i*2]==100+i && sampler_events[2+i*2]==200+i);
    }
    assert(sampler_active==3 && sampler_error==0x502);
    assert(wumpa_state_count==0);
}
static void prepare_sampler_checks(void) {
    wumpa_glGetError=sampler_get_error;
    wumpa_glEnable=sampler_enable;
    wumpa_glActiveTexture=sampler_active_texture;wumpa_glGetIntegerv=sampler_integer;
}
'''
def main():
    # Replay every title patch on an isolated in-repository ignored copy, so the
    # source anchor is checked against real ordering and parent Git cannot hide it.
    build=ROOT/'build';build.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='sampler-query-fixture-',dir=build) as name:
        work=Path(name);shutil.copytree(ROOT/'src',work/'title')
        env=dict(os.environ,GIT_CEILING_DIRECTORIES=str(build))
        for patch in sorted((HERE/'patches').glob('title-*.patch')):
            subprocess.run(['git','apply','--check',str(patch)],cwd=work/'title',env=env,check=True)
            subprocess.run(['git','apply',str(patch)],cwd=work/'title',env=env,check=True)
        source=(work/'title/shader_bridge.inc').read_text()
        assert source.count('wumpa_shader_sampler_queries(previous_samplers);')==1
        assert '#include "sampler_queries.inc"' in source
    spec=importlib.util.spec_from_file_location('rpc_fixture',HERE/'tests/test_gl_rpc_ordering.py')
    fixture=importlib.util.module_from_spec(spec);spec.loader.exec_module(fixture)
    fixture.FIXTURE=fixture.FIXTURE.replace('int main(void) {',EXTRA+'\nint main(void) {\n    setenv("WRATH_EGL_DEFER_STATE","1",1);')
    anchor='    pthread_t a,b;assert(!pthread_create(&a,NULL,caller,(void *)(uintptr_t)0));'
    assert fixture.FIXTURE.count(anchor)==1
    fixture.FIXTURE=fixture.FIXTURE.replace(anchor,
        '    prepare_sampler_checks();check_sampler_queries(NULL);\n'
        '    wumpa_gl_rpc_call(check_sampler_queries,NULL);\n'+anchor)
    anchor='    assert(current==context && bound && !backup_cb && !restore_cb);'
    fixture.FIXTURE=fixture.FIXTURE.replace(anchor,anchor+'\n    check_sampler_queries(NULL);')
    fixture.main()
    print('PASS: sampler query patch replay, queued order, four outputs/canaries, final active unit, RPC caller/owner/direct modes')
if __name__=='__main__':main()
