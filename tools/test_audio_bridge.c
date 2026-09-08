#include <assert.h>
#include <stdlib.h>
#include <SDL.h>
#include "../src/audio_bridge.c"
#include "fixtures/xbox_adpcm_golden.h"
_Thread_local uint32_t g_eax, g_esp;
ptrdiff_t g_xbox_mem_offset;
size_t g_xbox_map_size, g_xbox_total_ram;
static uint32_t heap = 0x20000;
uint32_t xbox_ContiguousAllocatedBytes(void) { return 0; }
uint32_t xbox_HeapAlloc(uint32_t bytes, uint32_t align) {heap=(heap+align-1)&~(align-1); uint32_t p=heap;heap+=bytes;return p;}
void xbox_HeapFree(uint32_t p) {(void)p;}
static uint32_t call(uint32_t address, const uint32_t *args, unsigned count)
{
 g_esp=0x11000;write32(g_esp,0xFEEDFACE);for(unsigned i=0;i<count;++i)write32(g_esp+4+i*4,args[i]);
 recomp_func_t f=wrath_audio_lookup(address);assert(f); f();assert(g_esp==0x11004+count*4);return g_eax;
}
int main(void)
{
 SDL_setenv("SDL_AUDIODRIVER","dummy",1);void *mem=calloc(1,0x400000);assert(mem);
 g_xbox_mem_offset=(ptrdiff_t)mem;g_xbox_map_size=g_xbox_total_ram=0x400000;
 uint32_t dc[]={0,0x12000,0};write32(0x12004,0xCAFEBABE);
 assert(call(0x137A06,dc,3)==0);uint32_t device=read32(0x12000);assert(device && read32(0x12004)==0xCAFEBABE);
 uint8_t fmt[20]={0x69,0,1,0,0x22,0x56,0,0,0x73,0x30,0,0,36,0,4,0,2,0,64,0};memcpy(ptr(0x12100),fmt,20);
 uint32_t desc[]={24,0x40000,0,0x12100,0,0};memcpy(ptr(0x12200),desc,24);
 uint32_t cb[]={0x12200,0x12000};uint32_t handles[252];
 for(unsigned i=0;i<252;++i){assert(call(0x137A4D,cb,2)==0);handles[i]=read32(0x12000);assert(handles[i] && handles[i]!=device);}
 memcpy(ptr(0x13000),golden_mono_adpcm,108);uint32_t data[]={handles[0],0x13000,108};assert(call(0x13755A,data,3)==0);
 uint32_t format[]={handles[0],0x12100};assert(call(0x136D21,format,2)==0);
 write32(0x12104,48000);assert(call(0x136D21,format,2)==0);
 uint32_t pitch[]={handles[0],(uint32_t)-4096};assert(call(0x136648,pitch,2)==0);
 uint32_t play[]={handles[0],0,0,1};assert(call(0x136664,play,4)==0);
 APUMixerVoice *voice=apu_mixer_get_voice(0);assert(voice->sample_rate==24000);
 const int32_t pitches[]={0,4096,-4096,-4608,-32767,8191};
 const uint32_t rates[]={48000,96000,24000,22008,188,191968};
 for(unsigned i=0;i<6;++i){pitch[1]=(uint32_t)pitches[i];assert(call(0x136648,pitch,2)==0);assert(voice->sample_rate==rates[i]);}
 pitch[1]=(uint32_t)-32768;assert(call(0x136648,pitch,2)==(uint32_t)E_INVALIDARG && voice->sample_rate==191968);
 pitch[1]=8192;assert(call(0x136648,pitch,2)==(uint32_t)E_INVALIDARG && voice->sample_rate==191968);
 assert(call(0x136D21,format,2)==0 && voice->sample_rate==48000);
 play[3]=0;assert(call(0x136664,play,4)==0);SDL_Delay(30);
 uint32_t status[]={handles[0],0x12000};assert(call(0x1366A0,status,2)==0);assert(!(read32(0x12000)&1));
 uint32_t pos[]={handles[0],0x12000,0};assert(call(0x1366BC,pos,3)==0);assert(read32(0x12000)==0);
 uint32_t seek[]={handles[0],36};assert(call(0x1366DC,seek,2)==0);assert(call(0x1366BC,pos,3)==0 && read32(0x12000)==36);
 data[1]=0x3FFFF0;assert(call(0x13755A,data,3)==(uint32_t)E_INVALIDARG);
 uint32_t fx[]={device,0,0,0,0x12000};assert(call(0x136605,fx,5)==(uint32_t)E_NOTIMPL && read32(0x12000)==0);
 assert(call(0x137AA4,cb,2)==(uint32_t)E_NOTIMPL && read32(0x12000)==0);

 /* Real producer thread completes native packets without DoWork/status polling. */
 uint32_t sd[]={0,3,0x12100,0,0,0};memcpy(ptr(0x12200),sd,24);
 assert(call(0x137AA4,cb,2)==0);uint32_t stream_handle=read32(0x12000);
 uint32_t sp[]={0x13000,108,0x12300,0x12304,0,0};memcpy(ptr(0x12400),sp,24);
 uint32_t process[]={stream_handle,0x12400,0};assert(call(0x136427,process,3)==0);
 SDL_Delay(40);assert(__atomic_load_n((uint32_t*)ptr(0x12304),__ATOMIC_ACQUIRE)==0 && read32(0x12300)==108);
 uint32_t stream_release[]={stream_handle};assert(call(0x136287,stream_release,1)==0);
 for(unsigned i=0;i<252;++i){uint32_t release[]={handles[i]};assert(call(0x135BFE,release,1)==0);}
 uint32_t release[]={device};assert(call(0x135BE8,release,1)==0 && !s_apu);
 wrath_audio_shutdown();free(mem);puts("PASS: guest32 constructors, 252 buffers, native playback/pitch/format changes, checked stack/output/cursor ABI, explicit limitations, cleanup");
}
