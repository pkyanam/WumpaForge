#!/usr/bin/env python3
"""Compile actual Android CopyRects helpers; verify authority, aliases and GL state.
This mock does not prove actual driver pixels; physical GPU validation is required.
"""
import os
from pathlib import Path
import subprocess
import tempfile
HERE=Path(__file__).resolve().parents[1]
patch=(HERE/'patches/title-zzzzzz-gpu-copyrects.patch').read_text()
added='\n'.join(line[1:] for line in patch.splitlines() if line.startswith('+') and not line.startswith('+++'))
helper=added[added.index('struct ShieldGpuCopy'):added.index('    struct ShieldGpuCopy copy;')]
# The added helper ends immediately before the original SDK function.
helper=helper[:helper.rfind('}\n')+2]
source=r'''
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
typedef unsigned GLuint; typedef int GLint,GLboolean; typedef int GLenum;
typedef struct {int32_t x1,y1,x2,y2;} D3DRECT;
enum {GL_READ_FRAMEBUFFER=1,GL_DRAW_FRAMEBUFFER,GL_READ_FRAMEBUFFER_BINDING,
GL_DRAW_FRAMEBUFFER_BINDING,GL_READ_BUFFER,GL_DRAW_BUFFER,GL_SCISSOR_TEST,
GL_FRAMEBUFFER_COMPLETE,GL_COLOR_ATTACHMENT0,GL_FRONT,GL_COLOR_BUFFER_BIT,GL_NEAREST};
struct Resource {uint32_t data,bytes,width,height,format,owner; GLuint target_texture,target_fbo; GLenum framebuffer; int authoritative;};
static GLuint read_fbo=7,draw_fbo=8; static GLint reads[16],draws[16];
static int scissor=1,incomplete,blits,coords[8];
static struct Resource *surface_gpu_storage(struct Resource *r){return r->authoritative?r:NULL;}
static GLuint xbox_D3D8GLBackBuffer(int front){return front?4:3;}
static void glBindFramebuffer(GLenum target,GLuint fbo){if(target==GL_READ_FRAMEBUFFER)read_fbo=fbo;else draw_fbo=fbo;}
static void glGetIntegerv(GLenum name,GLint *v){switch(name){
case GL_READ_FRAMEBUFFER_BINDING:*v=read_fbo;break;case GL_DRAW_FRAMEBUFFER_BINDING:*v=draw_fbo;break;
case GL_READ_BUFFER:*v=reads[read_fbo];break;case GL_DRAW_BUFFER:*v=draws[draw_fbo];break;default:abort();}}
static int glIsEnabled(GLenum e){assert(e==GL_SCISSOR_TEST);return scissor;}
static void glEnable(GLenum e){assert(e==GL_SCISSOR_TEST);scissor=1;}
static void glDisable(GLenum e){assert(e==GL_SCISSOR_TEST);scissor=0;}
static void glReadBuffer(GLenum e){reads[read_fbo]=e;}
static void glDrawBuffer(GLenum e){draws[draw_fbo]=e;}
static GLenum glCheckFramebufferStatus(GLenum e){(void)e;return incomplete?0:GL_FRAMEBUFFER_COMPLETE;}
static void glBlitFramebuffer(int a,int b,int c,int d,int e,int f,int g,int h,GLenum mask,GLenum filter){
 assert(read_fbo==1 && draw_fbo==2 && !scissor);assert(mask==GL_COLOR_BUFFER_BIT && filter==GL_NEAREST);
 int p[]={a,b,c,d,e,f,g,h};memcpy(coords,p,sizeof p);++blits;}
'''+helper+r'''
int main(int argc,char **argv){
 (void)argv;
 struct Resource a={.data=100,.bytes=400,.width=10,.height=10,.format=0x12,.target_fbo=1,.authoritative=1};
 struct Resource b={.data=1000,.bytes=800,.width=10,.height=20,.format=0x12,.target_fbo=2,.authoritative=1};
 struct ShieldGpuCopy copy;
 if(argc>1){assert(!shield_gpu_copy_begin(&a,&b,&copy));return 0;}
 setenv("WRATH_GPU_COPYRECTS","1",1);
 reads[1]=91;draws[2]=92;reads[7]=93;draws[8]=94;
 assert(shield_gpu_copy_begin(&a,&b,&copy));
 D3DRECT r={1,2,5,7};int32_t point[]={3,4};
 shield_gpu_copy_rect(&a,&b,&r,point);
 int expected[]={1,3,5,8,3,11,7,16};assert(!memcmp(coords,expected,sizeof expected));
 shield_gpu_copy_end(&copy);
 assert(read_fbo==7 && draw_fbo==8 && scissor && reads[1]==91 && draws[2]==92 && reads[7]==93 && draws[8]==94);
 b.authoritative=0;assert(!shield_gpu_copy_begin(&a,&b,&copy));b.authoritative=1;
 b.target_fbo=1;assert(!shield_gpu_copy_begin(&a,&b,&copy));b.target_fbo=2;
 b.data=499;assert(!shield_gpu_copy_begin(&a,&b,&copy));b.data=1000;
 a.owner=b.owner=12;assert(!shield_gpu_copy_begin(&a,&b,&copy));a.owner=b.owner=0;
 a.target_texture=b.target_texture=4;assert(!shield_gpu_copy_begin(&a,&b,&copy));a.target_texture=b.target_texture=0;
 b.format=6;assert(!shield_gpu_copy_begin(&a,&b,&copy));b.format=0x12;
 incomplete=1;assert(!shield_gpu_copy_begin(&a,&b,&copy));
 assert(read_fbo==7 && draw_fbo==8 && scissor && reads[1]==91 && draws[2]==92 && blits==1);
 return 0;}
'''
with tempfile.TemporaryDirectory(prefix='shield-copy-fixture-') as tmp:
    c=Path(tmp)/'test.c';c.write_text(source);exe=Path(tmp)/'test'
    subprocess.run(['cc','-std=c11','-D_POSIX_C_SOURCE=200809L','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(c),'-o',str(exe)],check=True)
    env=dict(os.environ);env.pop('WRATH_GPU_COPYRECTS',None)
    subprocess.run([str(exe),'disabled'],check=True,env=env)
    subprocess.run([str(exe)],check=True,env=env)
assert 'glGetError' not in helper
assert 'if (!width || !height) continue;\n+        if(direct)' in patch
print('GPU CopyRects helper fixture PASS (ASan/UBSan); physical pixels remain unverified')
