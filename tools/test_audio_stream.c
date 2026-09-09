/* Deterministic production mixer + exact guest32 stream ABI. No audio device. */
#include <assert.h>
#include <stdlib.h>
#include <sys/mman.h>
#include "../src/audio_bridge.c"
#include "../third_party/xboxrecomp/src/apu/apu_core.c"
#include "fixtures/xbox_adpcm_golden.h"
_Thread_local uint32_t g_eax, g_esp;
ptrdiff_t g_xbox_mem_offset;
size_t g_xbox_map_size, g_xbox_total_ram;
static uint32_t heap = 0x20000;
uint32_t xbox_ContiguousAllocatedBytes(void) { return 0x4000; }
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
/* Music remains alive across story -> hub -> level stream changes while a
 * separate portal SFX buffer is mixed. Synthetic exact samples make stale
 * voices, channel errors, global flushes and completion lifetime bugs visible. */
static void transition_overlap(void)
{
 /* Buffer Play requires a real initialized APU lock/condition. Keep its
  * producer absent so only the deterministic render() calls advance time. */
 assert(!g_state);g_state=calloc(1,sizeof(*g_state));assert(g_state);
 qemu_mutex_init(&g_state->lock);qemu_cond_init(&g_state->cond);
 uint8_t mono[20]={1,0,1,0,0x80,0xBB,0,0,0,0x77,1,0,2,0,16,0,0,0,0,0};
 uint8_t stereo[20]={1,0,2,0,0x80,0xBB,0,0,0,0xEE,2,0,4,0,16,0,0,0,0,0};
 uint32_t sd[]={0,3,0x12100,0,0,0},create[]={0x12200,0x12000};
 memcpy(ptr(0x12200),sd,24);memcpy(ptr(0x12100),stereo,20);
 assert(call(0x137AA4,create,2)==0);uint32_t music=read32(0x12000);
 int16_t music_pcm[12][2];for(unsigned i=0;i<12;++i){music_pcm[i][0]=600;music_pcm[i][1]=-200;}
 memcpy(ptr(0x14000),music_pcm,sizeof(music_pcm));assert(submit(music,0x14000,sizeof(music_pcm),0)==0);
 memcpy(ptr(0x12100),mono,20);assert(call(0x137AA4,create,2)==0);uint32_t story=read32(0x12000);
 assert(story!=music);int16_t story_pcm[8];for(unsigned i=0;i<8;++i)story_pcm[i]=100;
 memcpy(ptr(0x14100),story_pcm,sizeof(story_pcm));assert(submit(story,0x14100,sizeof(story_pcm),1)==0);
 uint32_t bd[]={24,0,0,0x12100,0,0};memcpy(ptr(0x12200),bd,24);
 assert(call(0x137A4D,create,2)==0);uint32_t sfx=read32(0x12000);
 int16_t sfx_pcm[]={1500,1500,1500,1500};memcpy(ptr(0x14200),sfx_pcm,sizeof(sfx_pcm));
 uint32_t data[]={sfx,0x14200,sizeof(sfx_pcm)},play[]={sfx,0,0,0};
 assert(call(0x13755A,data,3)==0 && call(0x136664,play,4)==0);
 int16_t out[2][2];render(out,2);
 for(unsigned i=0;i<2;++i)assert(out[i][0]==2200 && out[i][1]==1400);
 uint32_t one[]={story};assert(call(0x136287,one,1)==0);
 assert(read32(0x1250C)==0x80004004 && read32(0x12508)==0); /* old packet cancelled */
 assert(!read32(story)); /* final Release zeroes guest object before reuse */
 assert(read32(0x12504)==0x8000000A); /* music's packet survives */
 memcpy(ptr(0x12200),sd,24);assert(call(0x137AA4,create,2)==0);uint32_t level=read32(0x12000);
 assert(level==story && read32(level)==0x16B70C); /* actual bridge slot reuse */
 int16_t level_pcm[8];for(unsigned i=0;i<8;++i)level_pcm[i]=300;
 memcpy(ptr(0x14300),level_pcm,sizeof(level_pcm));assert(submit(level,0x14300,sizeof(level_pcm),2)==0);
 render(out,2);for(unsigned i=0;i<2;++i)assert(out[i][0]==2400 && out[i][1]==1600);
 render(out,2);for(unsigned i=0;i<2;++i)assert(out[i][0]==900 && out[i][1]==100);
 assert(read32(0x1250C)==0x80004004); /* recreate cannot complete old status */
 one[0]=sfx;assert(call(0x135BFE,one,1)==0);
 one[0]=level;assert(call(0x136287,one,1)==0 && read32(0x12514)==0x80004004);
 render(out,2);for(unsigned i=0;i<2;++i)assert(out[i][0]==600 && out[i][1]==-200);
 assert(read32(0x12504)==0x8000000A);
 one[0]=music;assert(call(0x136287,one,1)==0 && read32(0x12504)==0x80004004);
 render(out,2);for(unsigned i=0;i<2;++i)assert(!out[i][0] && !out[i][1]);
 for(unsigned i=0;i<64;++i)assert(!apu_mixer_get_voice(i)->active);
 qemu_cond_destroy(&g_state->cond);qemu_mutex_destroy(&g_state->lock);
 free(g_state);g_state=NULL;
}
int main(void)
{
 /* Reserve address space only: the unused 2 GiB gap remains inaccessible and
  * consumes no physical RAM. This matches the actual guest contiguous VA. */
 size_t span=0x80004000ull;
 void *mem=mmap(NULL,span,PROT_NONE,MAP_PRIVATE|MAP_ANON,-1,0);assert(mem!=MAP_FAILED);
 assert(mprotect(mem,0x400000,PROT_READ|PROT_WRITE)==0);
 assert(mprotect((char*)mem+0x80000000ull,0x4000,PROT_READ|PROT_WRITE)==0);
 g_xbox_mem_offset=(ptrdiff_t)mem;g_xbox_map_size=g_xbox_total_ram=0x400000;
 assert(range(0x80000000,0x4000) && range(0x80003FFF,1));
 assert(!range(0x80004000,1) && !range(0x80003FFF,2) && !range(0xFFFFFFFF,2));
 assert(!range(0,108) && !range(0x7FFFFFFF,2));
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
 /* Actual title packet source is physical zero in the high contiguous VA.
  * A conflicting low RAM value ensures a mask-to-low-RAM bug cannot pass. */
 memcpy(ptr(0x80000000),golden_mono_adpcm,108);memset(ptr(0),0xA5,108);
 assert(submit(s,0x80003FF0,108,0)==(uint32_t)E_INVALIDARG);
 assert(submit(s,0x80004000,108,0)==(uint32_t)E_INVALIDARG);
 assert(submit(s,0x80000000,108,0)==0);render(out,192);
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
 transition_overlap();
 wrath_audio_shutdown();assert(munmap(mem,span)==0);puts("PASS: stream guest ABI, continuous packet PCM/ADPCM output, pending/completion sizes, bounds, pause/resume, starvation, discontinuity, flush, ref lifetime, simultaneous stream/buffer mixing and transition reuse");
}
