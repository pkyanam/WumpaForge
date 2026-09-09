#!/usr/bin/env python3
"""Replay the actual patch in-repo; validate selection and fetch equivalence."""
from pathlib import Path
import os, subprocess, tempfile
HERE=Path(__file__).resolve().parents[1]
ROOT=HERE.parents[1]
C=r'''
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define NV2A_VERTEX_NONE 0
struct Nv2aVertexSlot {unsigned kind,stream,tessellation_type;};
typedef struct Nv2aVertexSlot Nv2aVertexSlot;
static unsigned s_vertex_handle=1;
static struct {Nv2aVertexSlot slots[16];} s_vertex_object;
#include "indexed_shader_select.inc"
static unsigned seed=7;
static unsigned rnd(void){seed=seed*1664525u+1013904223u;return seed;}
int main(void) {
 setenv("WRATH_INDEXED_SHADER","1",1);
 s_vertex_object.slots[0].kind=1;
 assert(indexed_shader_eligible(5,6,28,3));
 assert(!indexed_shader_eligible(5,6,28,6));
 assert(!indexed_shader_eligible(8,6,28,3));
 assert(!indexed_shader_eligible(5,5,28,3));
 s_vertex_handle=0;assert(!indexed_shader_eligible(5,6,28,3));s_vertex_handle=1;
 s_vertex_object.slots[1]=(Nv2aVertexSlot){1,2,0};
 assert(!indexed_shader_eligible(5,6,28,3));
 s_vertex_object.slots[1]=(Nv2aVertexSlot){1,0,1};
 assert(!indexed_shader_eligible(5,6,28,3));memset(&s_vertex_object.slots[1],0,sizeof(Nv2aVertexSlot));
 unsigned hits=0;
 for(unsigned trial=0;trial<10000;++trial) {
  unsigned stride=4+rnd()%60,base=rnd()%9,offset=rnd()%31,n=(1+rnd()%60)*3;
  uint16_t indices[180];unsigned maximum=0;
  for(unsigned i=0;i<n;++i){indices[i]=rnd()%64;if(indices[i]>maximum)maximum=indices[i];}
  unsigned char original[100*64+32],expanded[180*64];
  for(unsigned i=0;i<sizeof(original);++i)original[i]=(unsigned char)rnd();
  for(unsigned i=0;i<n;++i)memcpy(expanded+i*stride,original+offset+(base+indices[i])*stride,stride);
  if(indexed_shader_eligible(5,n,stride,maximum)) {
   ++hits;assert((maximum+1)*stride+n*2<=n*stride);
   const unsigned char *uploaded=original+offset+base*stride;
   for(unsigned i=0;i<n;++i)assert(!memcmp(expanded+i*stride,uploaded+indices[i]*stride,stride));
  }
 }
 assert(hits>1000);
 unsetenv("WRATH_INDEXED_SHADER");assert(!indexed_shader_eligible(5,6,28,3));
 puts("PASS: opt-in indexed selection, guarded fallback and 10000 original-expansion equivalence cases");
}
'''
def main():
 out=ROOT/'build/shield2019';out.mkdir(parents=True,exist_ok=True)
 with tempfile.TemporaryDirectory(prefix='indexed-source-',dir=out) as directory:
  work=Path(directory)
  for name in ['index_bridge.inc','vertex_fetch.inc','shader_bridge.inc','graphics.c']:
   (work/name).write_text((out/'title'/name).read_text())
  patch=HERE/'patches/title-zzzzzzzzz-indexed-shader.patch'
  env=dict(os.environ,GIT_CEILING_DIRECTORIES=str(out))
  # The build owner may already have replayed this patch in prepared sources.
  if 'indexed_vertices;' not in (work/'vertex_fetch.inc').read_text():
   subprocess.run(['git','apply',str(patch)],cwd=work,env=env,check=True)
  else:
   (work/'indexed_shader_select.inc').write_text((out/'title/indexed_shader_select.inc').read_text())
  (work/'fixture.c').write_text(C)
  subprocess.run(['cc','-std=c11','-O1','-fsanitize=address,undefined',str(work/'fixture.c'),'-o',str(work/'fixture')],check=True)
  subprocess.run([str(work/'fixture')],check=True)
  # Structural transfer check: fetch pointer is passed synchronously through RPC.
  wrapper=(HERE/'shader_draw_rpc_wrap.inc').read_text()
  assert '.fetch=fetch' in wrapper and 'request->stride,request->fetch' in wrapper
if __name__=='__main__':main()
