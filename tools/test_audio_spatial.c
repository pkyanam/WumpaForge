/* Real guest DWORD spatial ABI + production mixer, synthetic PCM, no device. */
#include <assert.h>
#include <stdlib.h>
#include <pthread.h>
#include "../src/audio_bridge.c"
#include "../third_party/xboxrecomp/src/apu/apu_core.c"
_Thread_local uint32_t g_eax,g_esp;
ptrdiff_t g_xbox_mem_offset;
size_t g_xbox_map_size,g_xbox_total_ram;
static uint32_t heap=0x21000;
uint32_t xbox_ContiguousAllocatedBytes(void){return 0;}
uint32_t xbox_HeapAlloc(uint32_t n,uint32_t a){heap=(heap+a-1)&~(a-1);uint32_t p=heap;heap+=n;return p;}
void xbox_HeapFree(uint32_t p){(void)p;}
static uint32_t bits(float f){uint32_t v;memcpy(&v,&f,4);return v;}
static uint32_t call(uint32_t address,const uint32_t *args,unsigned count){
 g_esp=0x11000;write32(g_esp,0xFEEDFACE);
 for(unsigned i=0;i<count;++i)write32(g_esp+4+i*4,args[i]);
 recomp_func_t f=wrath_audio_lookup(address);assert(f);f();assert(g_esp==0x11004+count*4);return g_eax;
}
static uint32_t create(unsigned flags,unsigned channels,int16_t left,int16_t right){
 uint8_t fmt[20]={1,0,1,0,0x80,0xBB,0,0,0,0x77,1,0,2,0,16,0,0,0,0,0};
 fmt[2]=channels;fmt[12]=channels*2;memcpy(ptr(0x12100),fmt,20);
 uint32_t desc[]={24,flags,0,0x12100,0,0},args[]={0x12200,0x12000};memcpy(ptr(0x12200),desc,24);
 assert(call(0x137A4D,args,2)==0);uint32_t b=read32(0x12000);
 int16_t pcm[64];for(unsigned i=0;i<32;++i){pcm[i*channels]=left;if(channels==2)pcm[i*2+1]=right;}
 memcpy(ptr(0x13000),pcm,32*channels*2);uint32_t data[]={b,0x13000,32*channels*2};assert(call(0x13755A,data,3)==0);
 uint32_t play[]={b,0,0,1};assert(call(0x136664,play,4)==0);return b;
}
static void expect(int l,int r){int16_t out[2][2]={{0}};mixer_render(out,2);for(int i=0;i<2;++i){if(abs(out[i][0]-l)>2||abs(out[i][1]-r)>2){fprintf(stderr,"expected %d,%d got %d,%d\n",l,r,out[i][0],out[i][1]);abort();}}}
static void position(uint32_t b,float x,float y,float z,unsigned deferred){uint32_t a[]={b,bits(x),bits(y),bits(z),deferred};assert(call(0x136D85,a,5)==0);}
static void listener(float x,float y,float z,unsigned deferred){uint32_t a[]={s_device_guest,bits(x),bits(y),bits(z),deferred};assert(call(0x1374E1,a,5)==0);}
static void orientation(float z,unsigned deferred){uint32_t a[]={s_device_guest,0,0,bits(z),0,bits(1),0,deferred};assert(call(0x137497,a,8)==0);}
static void distance(uint32_t b,float value,unsigned deferred,int maximum){uint32_t a[]={b,bits(value),deferred};assert(call(maximum?0x136D3D:0x136D61,a,3)==0);}
static void rolloff(float f,unsigned deferred){uint32_t a[]={s_device_guest,bits(f),deferred};assert(call(0x137516,a,3)==0);}
static void commit(void){uint32_t a[]={s_device_guest};assert(call(0x136D09,a,1)==0);}
static void release(uint32_t b){uint32_t a[]={b};assert(call(0x135BFE,a,1)==0);}
static void *mix_thread(void *unused){
 (void)unused;
 for(unsigned pass=0;pass<3000;++pass){int16_t out[32][2]={{0}};mixer_render(out,32);
  for(unsigned i=0;i<32;++i)assert((out[i][0]==10000&&out[i][1]==2000)||(out[i][0]==2000&&out[i][1]==10000));}
 return NULL;
}
int main(void){
 void *mem=calloc(1,0x400000);assert(mem);g_xbox_mem_offset=(ptrdiff_t)mem;g_xbox_total_ram=g_xbox_map_size=0x400000;
 assert(xbox_DirectSoundCreate(NULL,&s_device,NULL)==0);s_device_guest=0x20000;s_device_refs=1;
 g_state=calloc(1,sizeof(*g_state));assert(g_state);qemu_mutex_init(&g_state->lock);qemu_cond_init(&g_state->cond);
 uint32_t a=create(0x40010,1,10000,0);expect(7071,7071);
 distance(a,3,1,0);distance(a,50,1,1);position(a,0,0,6,0);expect(1178,1178);commit();expect(3535,3535);
 position(a,6,0,0,0);expect(0,5000);position(a,-3,0,0,0);expect(10000,0);
 position(a,100,0,0,0);expect(0,600);rolloff(0,0);expect(0,10000);rolloff(2,0);expect(0,309);rolloff(1,0);
 position(a,6,0,0,0);listener(3,0,0,1);expect(0,5000);commit();expect(0,10000);
 orientation(-1,1);expect(0,10000);commit();expect(10000,0);
 orientation(1,1);orientation(-1,0);commit();expect(10000,0); /* immediate overrides same pending property */
 uint32_t badpos[]={a,bits(NAN),0,0,0};assert(call(0x136D85,badpos,5)==(uint32_t)E_INVALIDARG);expect(10000,0);
 uint32_t baddist[]={a,0,0};assert(call(0x136D61,baddist,3)==(uint32_t)E_INVALIDARG);
 baddist[1]=bits(INFINITY);assert(call(0x136D3D,baddist,3)==(uint32_t)E_INVALIDARG);
 uint32_t badroll[]={s_device_guest,bits(11),0};assert(call(0x137516,badroll,3)==(uint32_t)E_INVALIDARG);
 uint32_t badorient[]={s_device_guest,0,bits(1),0,0,bits(1),0,0};assert(call(0x137497,badorient,8)==(uint32_t)E_INVALIDARG);
 /* A stereo format cannot silently disable an already spatial source. */
 ((uint8_t*)ptr(0x12100))[2]=2;((uint8_t*)ptr(0x12100))[12]=4;
 uint32_t format[]={a,0x12100};assert(call(0x136D21,format,2)==(uint32_t)AUDIO_BADFORMAT);expect(10000,0);
 ((uint8_t*)ptr(0x12100))[2]=1;((uint8_t*)ptr(0x12100))[12]=2;
 uint32_t badapply[]={a,bits(3),2};assert(call(0x136D61,badapply,3)==(uint32_t)E_INVALIDARG);
 /* Gram-Schmidt adjusts nonperpendicular front relative to top. */
 uint32_t orient[]={s_device_guest,0,bits(1),bits(1),0,bits(2),0,0};assert(call(0x137497,orient,8)==0);expect(0,10000);
 uint32_t volume[]={a,(uint32_t)-2000};assert(call(0x13662C,volume,2)==0);expect(0,1000);volume[1]=0;assert(call(0x13662C,volume,2)==0);
 position(a,-6,0,0,1);release(a);commit();expect(0,0);
 listener(0,0,0,0);uint32_t reused=create(0x40010,1,10000,0);assert(reused==a);expect(7071,7071);release(reused);
 uint32_t stereo=create(0x40000,2,400,700);
 uint32_t desc3d[]={24,0x40010,0,0x12100,0,0},create3d[]={0x12200,0x12000};memcpy(ptr(0x12200),desc3d,24);
 assert(call(0x137A4D,create3d,2)==(uint32_t)AUDIO_BADFORMAT && !read32(0x12000));
 listener(100,0,0,0);orientation(-1,0);rolloff(10,0);expect(400,700);
 uint32_t nospatial[]={stereo,bits(3),0};assert(call(0x136D61,nospatial,3)==(uint32_t)E_INVALIDARG);release(stereo);
 listener(0,0,0,0);orientation(1,0);rolloff(1,0);
 a=create(0x40010,1,10000,0);uint32_t b=create(0x40010,1,2000,0);
 distance(a,3,0,0);distance(b,3,0,0);position(a,-3,0,0,1);position(b,3,0,0,1);commit();
 pthread_t thread;assert(pthread_create(&thread,NULL,mix_thread,NULL)==0);
 for(unsigned i=0;i<1000;++i){float side=i&1?3:-3;position(a,side,0,0,1);position(b,-side,0,0,1);commit();}
 assert(pthread_join(thread,NULL)==0);release(a);release(b);expect(0,0);
 s_device_refs=0;qemu_cond_destroy(&g_state->cond);qemu_mutex_destroy(&g_state->lock);free(g_state);g_state=NULL;wrath_audio_shutdown();free(mem);
 puts("PASS: actual spatial PCM distance/panning, deferred listener/source commit, validation, volume, stereo preservation, release/reuse and concurrent atomic mixing");
}
