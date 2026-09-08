/* Deterministic production mixer + exact guest32 stream ABI. No audio device. */
#include <assert.h>
#include <stdlib.h>
#include "../src/audio_bridge.c"
#include "../third_party/xboxrecomp/src/apu/apu_core.c"
#include "fixtures/xbox_adpcm_golden.h"
_Thread_local uint32_t g_eax, g_esp;
ptrdiff_t g_xbox_mem_offset;
size_t g_xbox_map_size, g_xbox_total_ram;
static uint32_t heap = 0x20000;
uint32_t xbox_HeapAlloc(uint32_t bytes, uint32_t align) {heap=(heap+align-1)&~(align-1);uint32_t p=heap;heap+=bytes;return p;}
void xbox_HeapFree(uint32_t p) {(void)p;}
static uint32_t call(uint32_t address, const uint32_t *args, unsigned n)
{
 g_esp=0x11000;write32(g_esp,0xFEEDFACE);for(unsigned i=0;i<n;++i)write32(g_esp+4+i*4,args[i]);
 recomp_func_t f=wrath_audio_lookup(address);assert(f);f();assert(g_esp==0x11004+n*4);return g_eax;
}
static uint32_t submit(uint32_t s, uint32_t data, uint32_t bytes, unsigned index)
{
 uint32_t packet[]={data,bytes,0x12500+index*8,0x12504+index*8,0,0};memcpy(ptr(0x12400),packet,24);
 uint32_t args[]={s,0x12400,0};return call(0x136427,args,3);
}
static void render(int16_t out[][2], unsigned n) {memset(out,0,n*4);mixer_render(out,n);}
int main(void)
{
 void *mem=calloc(1,0x400000);assert(mem);g_xbox_mem_offset=(ptrdiff_t)mem;g_xbox_map_size=g_xbox_total_ram=0x400000;
 assert(xbox_DirectSoundCreate(NULL,&s_device,NULL)==0);s_device_refs=1;
 uint8_t fmt[20]={1,0,1,0,0x80,0xBB,0,0,0,0x77,1,0,2,0,16,0,0,0,0,0};memcpy(ptr(0x12100),fmt,20);
 uint32_t desc[]={0,3,0x12100,0,0,0};memcpy(ptr(0x12200),desc,24);
 uint32_t create[]={0x12200,0x12000};assert(call(0x137AA4,create,2)==0);uint32_t s=read32(0x12000);assert(read32(s)==0x16B70C);
 uint32_t one[]={s},two[]={s,0x12600};assert(call(0x136240,one,1)==2);assert(call(0x136287,one,1)==1);
 assert(call(0x1362D5,two,2)==0 && read32(0x12600)==5 && read32(0x12604)==2 && !read32(0x12608));
 two[1]=1;assert(call(0x1366FD,two,2)==0);
 for(unsigned p=0;p<3;++p){int16_t pcm[4];for(unsigned f=0;f<4;++f)pcm[f]=(p+1)*1000;memcpy(ptr(0x13000+p*8),pcm,8);assert(submit(s,0x13000+p*8,8,p)==0);}
 write32(0x1251C,0xCAFEBABE);assert(submit(s,0x13000,8,3)==(uint32_t)E_FAIL && read32(0x1251C)==0xCAFEBABE);
 int16_t out[200][2];render(out,2);assert(out[0][0]==0 && read32(0x12504)==0x8000000A);
 two[1]=0;assert(call(0x1366FD,two,2)==0);render(out,10);
 for(unsigned f=0;f<10;++f)assert(out[f][0]==(int)((f/4+1)*1000) && out[f][0]==out[f][1]);
 assert(read32(0x12504)==0 && read32(0x12500)==8 && read32(0x1250C)==0 && read32(0x12514)==0x8000000A);
 assert(call(0x13633C,one,1)==0);render(out,2);assert(out[1][0]==3000 && read32(0x12514)==0 && read32(0x12510)==8);
 two[1]=0x12600;assert(call(0x1363D6,two,2)==0 && read32(0x12600)==1);
 assert(submit(s,0x13000,8,0)==0);render(out,4);assert(call(0x1363D6,two,2)==0 && read32(0x12600)==0x40001);
 assert(submit(s,0x13000,8,0)==0);render(out,1);two[1]=1;assert(call(0x1366FD,two,2)==0);render(out,7);assert(!out[0][0] && read32(0x12504)==0x8000000A);
 two[1]=0;assert(call(0x1366FD,two,2)==0);render(out,3);assert(out[2][0]==1000 && read32(0x12504)==0);
 assert(submit(s,0x13000,8,0)==0);assert(call(0x136389,one,1)==0);assert(read32(0x12504)==0x80004004 && !read32(0x12500));
 assert(call(0x136287,one,1)==0);
 uint8_t adpcm[20]={0x69,0,1,0,0x80,0xBB,0,0,0,0,0,0,36,0,4,0,2,0,64,0};memcpy(ptr(0x12100),adpcm,20);
 assert(call(0x137AA4,create,2)==0);s=read32(0x12000);one[0]=s;memcpy(ptr(0x13000),golden_mono_adpcm,108);
 assert(submit(s,0x13000,108,0)==0);render(out,192);
 for(unsigned f=0;f<192;++f)assert(out[f][0]==golden_mono_pcm[f] && out[f][1]==golden_mono_pcm[f]);
 assert(read32(0x12504)==0 && read32(0x12500)==108);
 assert(submit(s,0x13000,107,0)==(uint32_t)AUDIO_BADFORMAT);
 assert(submit(s,0x13000,108,0)==0);assert(call(0x136287,one,1)==0 && read32(0x12504)==0x80004004);

 /* A nonintegral source rate retains its fractional position across packets. */
 memcpy(ptr(0x12100),fmt,20);write32(0x12104,22050);
 assert(call(0x137AA4,create,2)==0);s=read32(0x12000);one[0]=s;
 for(unsigned p=0;p<3;++p){int16_t pcm[7];for(unsigned f=0;f<7;++f)pcm[f]=100+p*7+f;memcpy(ptr(0x13000+p*14),pcm,14);assert(submit(s,0x13000+p*14,14,p)==0);}
 render(out,50);uint64_t inc=((uint64_t)22050<<16)/48000;
 for(unsigned f=0;f<50;++f){unsigned source=(unsigned)((f*inc)>>16);assert(out[f][0]==(source<21?(int)(100+source):0));}
 assert(read32(0x12514)==0);assert(call(0x136287,one,1)==0);
 wrath_audio_shutdown();free(mem);puts("PASS: stream guest ABI, continuous packet PCM/ADPCM output, pending/completion sizes, bounds, pause/resume, starvation, discontinuity, flush, ref lifetime");
}
