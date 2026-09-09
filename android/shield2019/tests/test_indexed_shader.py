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
CAP=r'''
#include <assert.h>
#include <string.h>
#include <stdio.h>
typedef int GLint;typedef unsigned GLuint;
#define GL_MAJOR_VERSION 1
#define GL_MINOR_VERSION 2
#define GL_NUM_EXTENSIONS 3
#define GL_EXTENSIONS 4
static int major=4,minor=1,queries,extension;
static void glGetIntegerv(unsigned key,int *out) {
 ++queries;
 switch(key){case GL_MAJOR_VERSION:*out=major;break;case GL_MINOR_VERSION:*out=minor;break;
 case GL_NUM_EXTENSIONS:*out=1;break;default:assert(0);}
}
static const unsigned char *glGetStringi(unsigned key,unsigned index) {
 assert(key==GL_EXTENSIONS && index==0);++queries;
 return (const unsigned char *)(extension==1?"GL_ARB_ES3_compatibility":extension==2?"GL_ARB_ES3_compatibility_suffix":"GL_ARB_other");
}
#include "indexed_restart_cap.inc"
int main(void) {
 assert(!indexed_fixed_restart_supported());int old=queries;
 assert(!indexed_fixed_restart_supported() && queries==old);
 s_indexed_fixed_restart=-1;extension=1;assert(indexed_fixed_restart_supported());
 s_indexed_fixed_restart=-1;extension=2;assert(!indexed_fixed_restart_supported());
 s_indexed_fixed_restart=-1;minor=3;old=queries;assert(indexed_fixed_restart_supported() && queries==old+2);
 s_indexed_fixed_restart=-1;major=5;minor=0;assert(indexed_fixed_restart_supported());
 puts("PASS: GL4.1 unsupported, exact extension match, GL4.3+ and immutable capability cache");
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
   (work/'indexed_restart_cap.inc').write_text((out/'title/indexed_restart_cap.inc').read_text())
  (work/'fixture.c').write_text(C)
  subprocess.run(['cc','-std=c11','-O1','-fsanitize=address,undefined',str(work/'fixture.c'),'-o',str(work/'fixture')],check=True)
  subprocess.run([str(work/'fixture')],check=True)
  (work/'cap.c').write_text(CAP)
  subprocess.run(['cc','-std=c11','-O1','-fsanitize=address,undefined',str(work/'cap.c'),'-o',str(work/'cap')],check=True)
  subprocess.run([str(work/'cap')],check=True)
  # Structural transfer check: fetch pointer is passed synchronously through RPC.
  wrapper=(HERE/'shader_draw_rpc_wrap.inc').read_text()
  assert '.fetch=fetch' in wrapper and 'request->stride,request->fetch' in wrapper
if __name__=='__main__':main()
