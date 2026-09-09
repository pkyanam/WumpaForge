#!/usr/bin/env python3
"""Test the actual diagnostic helper: TLS ring, wrap, once-only dump and log cap."""
from pathlib import Path
import os, subprocess, tempfile
ROOT=Path(__file__).resolve().parents[3]
def main():
    with tempfile.TemporaryDirectory(prefix='wumpa-cs-ring-') as directory:
        work=Path(directory)
        for relative in ('src/kernel/kernel_rtl.c','src/platform/win32_compat.c'):
            dest=work/relative;dest.parent.mkdir(parents=True,exist_ok=True)
            dest.write_text((ROOT/'third_party/xboxrecomp'/relative).read_text())
        subprocess.run(['git','apply',str(ROOT/'android/shield2019/patches/runtime-loading-cs-trace.patch')],cwd=work,check=True)
        text=(work/'src/kernel/kernel_rtl.c').read_text()
        helper=text[text.index('/* Opt-in, bounded diagnostics armed'):text.index('static void xbox_cs_note_owner(')]
        fixture=r'''
#include <stdint.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
typedef void *PRTL_CRITICAL_SECTION;
static uintptr_t g_xbox_mem_offset;
static _Thread_local uint32_t g_xbox_kernel_caller;
static _Thread_local unsigned tid=1007;
static unsigned GetCurrentThreadId(void) {return tid;}
static int gettid(void) {return (int)tid;}
'''+helper+r'''
static void event(uint32_t site,const char *name,long depth,int result) {
    g_xbox_kernel_caller=site;wumpa_cs_trace(name,(void*)(uintptr_t)0x4EA440,depth,result);
}
static void *other(void *unused) {
    (void)unused;tid=2000;
    for(unsigned i=0;i<70;++i)event(0xDEAD,"enter",2,0);
    return NULL;
}
int main(void) {
    event(0x2BF67,"enter",1,0);
    pthread_t thread;if(pthread_create(&thread,NULL,other,NULL))return 2;
    if(pthread_join(thread,NULL))return 3;
    event(0x2BFF6,"leave",1,0);event(0x2BFF6,"unlock",0,0);
    for(unsigned i=0;i<1000;++i) {
        event(0x2BF67,"enter",1,0);event(0x2BFF6,"leave",1,0);event(0x2BFF6,"unlock",0,0);
    }
    for(unsigned i=0;i<70;++i)event(0x100+i,i==69?"unlock":"enter",2,i==69?13:0);
    event(0x2BFF6,"leave",3,0);
    event(0x2BFF6,"unlock",2,0);
    event(0x2BFF6,"leave",3,0);
}
'''
        source=work/'fixture.c';source.write_text(fixture);binary=work/'test'
        subprocess.run(['clang','-std=c11','-O1','-pthread','-fsanitize=undefined','-fno-sanitize-recover=all',str(source),'-o',str(binary)],check=True)
        result=subprocess.run([str(binary)],env=dict(os.environ,WRATH_TRACE_LOADING_CS='outer'),capture_output=True,text=True,check=True)
        rows=result.stderr.splitlines();outer=[row for row in rows if row.startswith('[shield cs]')]
        ring=[row for row in rows if row.startswith('[shield cs ring]')]
        assert len(outer)==3000,len(outer)
        assert len(ring)==65,len(ring)
        assert 'first_bad_outer_leave' in ring[0] and 'depth=3 records=64' in ring[0]
        assert 'i=0 enter guest_tid=1007' in ring[1] and 'site=0x00000107' in ring[1]
        assert 'i=62 unlock' in ring[-2] and 'result=13' in ring[-2]
        assert 'i=63 leave' in ring[-1] and 'site=0x0002BFF6 depth=3' in ring[-1]
        assert not any('DEAD' in row or 'guest_tid=2000' in row for row in ring)
    print('PASS: balanced outer leaves stay quiet, per-thread64-event ring wraps, first imbalance dumps once even beyond3000outer records, sites/depth/results preserved')
if __name__=='__main__':main()
