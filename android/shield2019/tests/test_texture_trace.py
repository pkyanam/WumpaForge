#!/usr/bin/env python3
"""Exercise the diagnostic helper's exact byte accounting and bounded admission."""
from pathlib import Path
import os, subprocess, tempfile
ROOT=Path(__file__).resolve().parents[3]
def main():
    patch=(ROOT/'android/shield2019/patches/title-zzztexture-trace.patch').read_text()
    added=patch.split('+++ b/texture_trace.inc\n',1)[1]
    helper='\n'.join(line[1:] for line in added.splitlines() if line.startswith('+'))+'\n'
    fixture=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef int32_t HRESULT;
struct Resource {
    uint32_t handle,owner,data,bytes,width,height,format,levels,offsets[13];
    int snapshot_valid,dirty;
    uint8_t *encoded_snapshot;
    uint64_t upload_serial;
};
static uint8_t memory[512],shadow[32];
static struct Resource parent;
static uint32_t g_esp=64;
#define GUEST_SWAP_COUNT 32
static uint32_t read32(uint32_t p) {return p==GUEST_SWAP_COUNT?6:0x12345;}
static void *guest_ptr(uint32_t p) {return memory+p;}
static struct Resource *resource(uint32_t p) {return p==parent.handle?&parent:NULL;}
'''+helper+r'''
int main(void) {
    parent=(struct Resource){.handle=0x1000,.data=128,.bytes=32,.width=4,.height=2,
        .format=6,.levels=2,.offsets={0,8},.snapshot_valid=1,.encoded_snapshot=shadow,.upload_serial=5};
    struct Resource child={.handle=0x2000,.owner=0x1000,.data=136,.bytes=8,.width=2,.height=1,.format=6};
    struct Resource huge=child;huge.bytes=WUMPA_TEXTURE_TRACE_BYTES+1;
    assert(!wumpa_texture_trace_begin(&huge,1).record);
    struct WumpaTextureTrace first=wumpa_texture_trace_begin(&child,1);
    memory[139]=0x7f;
    wumpa_texture_trace_end(first,"resolve",&child,0);
    wumpa_texture_trace_end(wumpa_texture_trace_begin(&child,0),"upload",&child,0);
    for(unsigned i=0;i<120;++i)
        wumpa_texture_trace_end(wumpa_texture_trace_begin(&child,0),"upload",&child,0);
    assert(memory[139]==0x7f && !memcmp(shadow,(uint8_t[32]){0},32));
    assert(parent.snapshot_valid && parent.upload_serial==5 && child.owner==0x1000);
}
'''
    with tempfile.TemporaryDirectory(prefix='wumpa-texture-trace-') as directory:
        work=Path(directory);source=work/'fixture.c';source.write_text(fixture);binary=work/'test'
        subprocess.run(['clang','-std=c11','-O1','-fsanitize=undefined','-fno-sanitize-recover=all',str(source),'-o',str(binary)],check=True)
        def run(value):
            env=dict(os.environ,WRATH_TRACE_TEXTURE_FRAME=value)
            return subprocess.run([str(binary)],env=env,text=True,capture_output=True,check=True).stderr.splitlines()
        rows=run('7');assert len(rows)==96, len(rows)
        assert 'n=1 frame=7 event=resolve' in rows[0]
        assert 'owner=00001000 data=00000088 offset=8 mip=1' in rows[0]
        assert 'before=1' in rows[0] and 'changed=1 first=3 shadow=1 shadow_changed=1 shadow_first=3' in rows[0]
        assert 'before=0' in rows[1] and 'shadow_changed=1 shadow_first=3' in rows[1]
        assert 'n=96 ' in rows[-1]
        for value in ('','0','8','bad','7x','4294967296'):
            assert not run(value),value
    print('PASS: selected frame, exact resolve/snapshot byte differences, parent/mip identity, 96-record/256KiB bounds, no guest/snapshot mutation')
if __name__=='__main__':main()
