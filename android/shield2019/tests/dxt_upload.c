#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
typedef unsigned GLenum;typedef int GLint;typedef int GLsizei;
#define GL_TEXTURE_CUBE_MAP 1
#define GL_TEXTURE_CUBE_MAP_POSITIVE_X 10
#define GL_TEXTURE_2D 2
#define GL_NO_ERROR 0
struct Resource {uint32_t width,height,levels,format,face_stride,bytes,sizes[13],offsets[13];};
static unsigned calls,fail_call,cap_queries;static GLenum error;
static int epoxy_has_gl_extension(const char *s){assert(!strcmp(s,"GL_EXT_texture_compression_s3tc"));++cap_queries;return 1;}
static void glCompressedTexImage2D(GLenum target,GLint level,GLenum format,GLsizei w,GLsizei h,GLint border,GLsizei size,const void *data)
{assert(target>=10&&target<=15&&level>=0&&level<4&&format==0x83F3&&w==h&&!border&&data);assert(size==((w+3)/4)*((h+3)/4)*16);if(++calls==fail_call)error=1;}
static GLenum glGetError(void){GLenum result=error;error=0;return result;}
#include "dxt_upload.inc"
int main(void){
 assert(native_dxt_format(0x0C)==0x83F1&&native_dxt_format(0x0E)==0x83F2&&native_dxt_format(0x0F)==0x83F3&&cap_queries==1);
 assert(!native_dxt_format(6));setenv("WRATH_DISABLE_DXT_UPLOAD","1",1);assert(!native_dxt_format(0x0F));unsetenv("WRATH_DISABLE_DXT_UPLOAD");
 struct Resource r={.width=8,.height=8,.levels=4,.format=0x0F,.face_stride=128,.bytes=768,.sizes={64,16,16,16},.offsets={0,64,80,96}};
 uint8_t bytes[768]={0};uint64_t uploaded=0;
 assert(upload_dxt_levels(&r,GL_TEXTURE_CUBE_MAP,6,bytes,0x83F3,&uploaded)&&calls==24&&uploaded==672);
 calls=0;fail_call=3;assert(!upload_dxt_levels(&r,GL_TEXTURE_CUBE_MAP,6,bytes,0x83F3,&uploaded)&&calls==3&&!error);
 calls=0;fail_call=0;r.sizes[0]=63;assert(!upload_dxt_levels(&r,GL_TEXTURE_CUBE_MAP,6,bytes,0x83F3,&uploaded)&&!calls);
 r.sizes[0]=64;r.bytes=63;assert(!upload_dxt_levels(&r,GL_TEXTURE_CUBE_MAP,6,bytes,0x83F3,&uploaded)&&!calls);
 puts("PASS DXT upload bounds, cube/mip block counts, capability cache, opt-out and driver-error fallback signal");
}
