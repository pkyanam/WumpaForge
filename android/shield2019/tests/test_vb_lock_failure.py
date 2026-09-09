#!/usr/bin/env python3
"""Run the actual patched VB lock hook; failure diagnostics must not change output."""
from pathlib import Path
import subprocess,tempfile
ROOT=Path(__file__).resolve().parents[3]
def main():
    with tempfile.TemporaryDirectory(prefix='wumpa-vb-lock-') as directory:
        work=Path(directory);source=work/'graphics.c';source.write_text((ROOT/'src/graphics.c').read_text())
        subprocess.run(['git','apply',str(ROOT/'android/shield2019/patches/title-vb-lock-failure.patch')],cwd=work,check=True)
        text=source.read_text();hook=text[text.index('void sub_00100DD0(void)'):text.index('static void trace_stream_binding(')]
        fixture=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#define RESOURCE_VERTEX_BUFFER 2
#define D3DERR_INVALIDCALL 0x8876086cu
#define GUEST_SWAP_COUNT 0x40u
struct Resource {uint32_t type,references,bindings,data,bytes;};
static struct Resource r={2,1,0,0x3000,8};
static uint32_t args[5],output=0xcafebabe,g_esp=0x4000,result;
static unsigned writes;
static uint32_t arg(unsigned i) {return args[i];}
static struct Resource *resource(uint32_t handle) {return handle==0x1000?&r:NULL;}
static uint32_t read32(uint32_t address) {return address==0x2000?output:0;}
static void write32(uint32_t address,uint32_t value) {assert(address==0x2000);output=value;++writes;}
static void finish(unsigned bytes,uint32_t status) {assert(bytes==20);result=status;}
'''+hook+r'''
static void rejected(void) {sub_00100DD0();assert(result==D3DERR_INVALIDCALL && output==0xcafebabe && writes==0);}
int main(void) {
    args[0]=0x999;args[3]=0x2000;rejected();
    args[0]=0x1000;r.type=1;rejected();r.type=2;
    args[3]=0;rejected();args[3]=0x2000;
    args[1]=9;rejected();args[1]=8;args[2]=1;rejected();
    args[0]=0x999;
    for(unsigned i=0;i<20;++i)rejected();
    args[0]=0x1000;args[1]=3;args[2]=4;
    sub_00100DD0();assert(result==0 && output==0x3003 && writes==1);
    args[1]=0;args[2]=0;sub_00100DD0();assert(result==0 && output==0x3000 && writes==2);
}
'''
        c=work/'fixture.c';c.write_text(fixture);binary=work/'test'
        subprocess.run(['clang','-std=c11','-O1','-fsanitize=undefined','-fno-sanitize-recover=all',str(c),'-o',str(binary)],check=True)
        result=subprocess.run([str(binary)],capture_output=True,text=True,check=True)
        rows=result.stderr.splitlines();assert len(rows)==16,len(rows)
        assert all('[shield vb lock] rejected' in row for row in rows)
        assert 'out_previous=CAFEBABE' in rows[0]
    print('PASS: actual VBLock rejection classes retain HRESULT/output, valid and zero-size locks return guest data, diagnostic cap16')
if __name__=='__main__':main()
