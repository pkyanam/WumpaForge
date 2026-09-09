#!/usr/bin/env python3
"""Exercise the exact Android UP-RPC wrapper block with isolated thread TLS."""
from pathlib import Path
import subprocess
import tempfile

HERE=Path(__file__).resolve().parents[1]
PRELUDE=r'''
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#define __stdcall
 typedef int HRESULT;typedef unsigned UINT;typedef int D3DPRIMITIVETYPE;
typedef int D3DFORMAT;typedef struct {int unused;} IDirect3DDevice8;
static _Thread_local struct {uint64_t draws,draw_time,other;} g_profile_calls;
static _Thread_local int owner,depth;
static int enabled,flushes,submits;
static IDirect3DDevice8 device;
static unsigned char vertices[12]={7},indices[6]={9};
static int wumpa_gl_rpc_draw_enabled(void) {return enabled;}
static int wumpa_gl_rpc_owner(void) {return owner;}
static int xbox_D3D8GLBeginStateCall(void) {int outer=!depth;if(outer)++depth;return outer;}
static void xbox_D3D8GLEndCall(int outer) {assert(depth>0);if(outer)--depth;}
static void wumpa_gl_flush_state(void) {assert(depth>0);++flushes;}
static HRESULT dev_DrawPrimitiveUP_impl(IDirect3DDevice8 *s,D3DPRIMITIVETYPE pt,
    UINT count,const void *v,UINT stride) {
    assert(s==&device && pt==3 && count==4 && v==vertices && stride==12);
    assert(*(const unsigned char *)v==7);
    g_profile_calls.draws+=2;g_profile_calls.draw_time+=31;return -71;
}
static HRESULT dev_DrawIndexedPrimitiveUP_impl(IDirect3DDevice8 *s,D3DPRIMITIVETYPE pt,
    UINT min,UINT nv,UINT count,const void *idx,D3DFORMAT format,const void *v,UINT stride) {
    assert(s==&device && pt==5 && min==2 && nv==8 && count==6 && idx==indices);
    assert(format==16 && v==vertices && stride==24);
    assert(*(const unsigned char *)idx==9 && *(const unsigned char *)v==7);
    g_profile_calls.draws+=3;g_profile_calls.draw_time+=47;return -93;
}
struct Request {void (*function)(void *);void *argument;};
static void *worker(void *opaque) {
    struct Request *request=opaque;owner=1;
    g_profile_calls.draws=100;g_profile_calls.draw_time=200;g_profile_calls.other=300;
    request->function(request->argument);
    assert(g_profile_calls.draws==100 && g_profile_calls.draw_time==200 && g_profile_calls.other==300);
    return NULL;
}
static void wumpa_gl_rpc_call_named(const char *name,void (*function)(void *),void *argument) {
    assert(depth>0 && flushes==submits+1);
    assert(!strcmp(name,"DrawPrimitiveUP") || !strcmp(name,"DrawIndexedPrimitiveUP"));
    ++submits;struct Request request={function,argument};pthread_t thread;
    assert(!pthread_create(&thread,NULL,worker,&request));assert(!pthread_join(thread,NULL));
}
'''
MAIN=r'''
int main(void) {
    g_profile_calls.draws=10;g_profile_calls.draw_time=20;g_profile_calls.other=90;
    enabled=1;
    assert(dev_DrawPrimitiveUP(&device,3,4,vertices,12)==-71);
    assert(!depth && submits==1 && g_profile_calls.draws==12 && g_profile_calls.draw_time==51);
    depth=1; /* Existing SDK serialization remains held after the nested wrapper. */
    assert(dev_DrawIndexedPrimitiveUP(&device,5,2,8,6,indices,16,vertices,24)==-93);
    assert(depth==1 && submits==2 && g_profile_calls.draws==15 && g_profile_calls.draw_time==98);
    depth=0;enabled=0;
    assert(dev_DrawPrimitiveUP(&device,3,4,vertices,12)==-71);
    assert(submits==2 && g_profile_calls.draws==17 && g_profile_calls.draw_time==129);
    enabled=1;owner=1;
    assert(dev_DrawIndexedPrimitiveUP(&device,5,2,8,6,indices,16,vertices,24)==-93);
    assert(submits==2 && g_profile_calls.draws==20 && g_profile_calls.draw_time==176);
    assert(g_profile_calls.other==90 && !depth && flushes==2);
    return 0;
}
'''
def main():
    source=(HERE.parents[1]/'build/shield2019/runtime/src/d3d/d3d8_gl.c').read_text()
    start=source.index('struct WumpaUPDraw {')
    stop=source.index('static HRESULT __stdcall dev_DrawPrimitive(',start)
    block=source[start:stop]
    # Actual prepared owner guards must avoid taking producer mutexes recursively.
    for name in ['xbox_D3D8GLBeginCall','xbox_D3D8GLBeginStateCall']:
        begin=source.index('int '+name+'(void) {')
        assert 'if(wumpa_gl_rpc_owner())return 0;' in source[begin:begin+180]
    with tempfile.TemporaryDirectory() as directory:
        work=Path(directory);(work/'fixture.c').write_text(PRELUDE+block+MAIN)
        subprocess.run(['cc','-std=c11','-O1','-pthread','-fsanitize=address,undefined',str(work/'fixture.c'),'-o',str(work/'fixture')],check=True)
        subprocess.run([str(work/'fixture')],check=True,timeout=10)
    print('PASS: actual UP wrapper arguments, pointers, results, owner bypass, caller locks and isolated TLS deltas')
if __name__=='__main__':main()
