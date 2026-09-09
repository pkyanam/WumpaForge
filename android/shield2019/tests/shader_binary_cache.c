/* Exercise untrusted cache bytes and resource bounds with a deterministic GL driver. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
typedef unsigned GLuint;typedef unsigned GLenum;typedef int GLint;typedef int GLsizei;
#define GL_VENDOR 1
#define GL_RENDERER 2
#define GL_VERSION 3
#define GL_NUM_PROGRAM_BINARY_FORMATS 4
#define GL_PROGRAM_BINARY_FORMATS 5
#define GL_LINK_STATUS 6
#define GL_PROGRAM_BINARY_LENGTH 7
#define GL_NO_ERROR 0
static int loads, reject;
static const unsigned char *glGetString(GLenum name) { return (const unsigned char*)(name==GL_VENDOR?"vendor":name==GL_RENDERER?"renderer":"4.1 driver"); }
static void glGetIntegerv(GLenum name,GLint *value) { *value=name==GL_NUM_PROGRAM_BINARY_FORMATS?1:42; }
static void glGetProgramiv(GLuint p,GLenum name,GLint *value) { (void)p;*value=name==GL_LINK_STATUS?!reject:4; }
static void glProgramBinary(GLuint p,GLenum format,const void *data,GLsizei size) { (void)p;assert(format==42&&size==4&&!memcmp(data,"test",4));++loads; }
static void glGetProgramBinary(GLuint p,GLsizei size,GLsizei *written,GLenum *format,void *data) { (void)p;assert(size==4);*written=4;*format=42;memcpy(data,"test",4); }
static GLenum glGetError(void) { return GL_NO_ERROR; }
#include "shader_binary_cache.inc"
int main(int argc,char **argv)
{
    assert(argc==2);setenv("WRATH_SHADER_CACHE_ROOT",argv[1],1);
    struct ShaderBinaryKey key=shader_binary_key("vertex","pixel");assert(key.directory>=0);
    assert(!shader_binary_load(&key,1));shader_binary_save(&key,1);
    assert(shader_binary_load(&key,1)&&loads==1);
    reject=1;assert(!shader_binary_load(&key,1)&&loads==2);reject=0;
    struct ShaderBinaryKey collision=shader_binary_key("VERTEX","pixel");
    strcpy(collision.name,key.name);assert(!shader_binary_load(&collision,1)&&loads==2);close(collision.directory);
    const char *vendor=key.part[0];key.part[0]="VENDOR";
    assert(!shader_binary_load(&key,1)&&loads==2);key.part[0]=vendor;
    int fd=openat(key.directory,key.name,O_RDWR);assert(fd>=0);
    assert(lseek(fd,-1,SEEK_END)>=0&&write(fd,"!",1)==1);close(fd);
    assert(!shader_binary_load(&key,1)&&loads==2); // Corrupt payload never reaches driver.
    shader_binary_save(&key,1);
    fd=openat(key.directory,key.name,O_RDWR);assert(fd>=0);
    struct ShaderBinaryHeader h;assert(read(fd,&h,sizeof(h))==sizeof(h));
    h.binary_size=SHADER_BINARY_LIMIT+1;lseek(fd,0,SEEK_SET);assert(write(fd,&h,sizeof(h))==sizeof(h));close(fd);
    assert(!shader_binary_load(&key,1)&&loads==2);
    unlinkat(key.directory,key.name,0);
    for(unsigned i=0;i<256;++i){char name[32];snprintf(name,sizeof(name),"f%u",i);fd=openat(key.directory,name,O_CREAT|O_WRONLY,0600);assert(fd>=0);close(fd);}
    shader_binary_save(&key,1);assert(faccessat(key.directory,key.name,F_OK,0)!=0);
    for(unsigned i=0;i<256;++i){char name[32];snprintf(name,sizeof(name),"f%u",i);unlinkat(key.directory,name,0);}
    fd=openat(key.directory,"large",O_CREAT|O_WRONLY,0600);assert(fd>=0&&ftruncate(fd,64u*1024u*1024u)==0);close(fd);
    shader_binary_save(&key,1);assert(faccessat(key.directory,key.name,F_OK,0)!=0);
    unlinkat(key.directory,"large",0);close(key.directory);
    unsetenv("WRATH_SHADER_CACHE_ROOT");key=shader_binary_key("vertex","pixel");assert(key.directory<0);
    puts("shader cache checks passed: hit, reject, collision, driver identity, corruption, size/count bounds, disabled");
}
