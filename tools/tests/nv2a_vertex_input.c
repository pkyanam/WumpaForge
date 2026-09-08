#include "../../src/nv2a_vertex_input.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char storage[20001];
static unsigned char *b=storage+1; /* Deliberately misaligned object. */
static size_t end;
static Nv2aVertexObject result;
static char error[256];
static void put(size_t offset,uint32_t value)
{
    for(unsigned i=0;i<4;i++)b[offset+i]=(unsigned char)(value>>(8*i));
}
static void begin(unsigned instructions)
{
    memset(storage,0,sizeof(storage)); end=0x114;
    put(0,1);put(4,0x10);put(8,instructions);
    for(unsigned i=0;i<16;i++)put(0x1c+16*i,2);
    put(0x1c,0x32);put(0x18,4);put(0x14,1);
    put(0x4c,0x22);put(0x48,20);
}
static void packet(unsigned method,unsigned count,uint32_t first)
{
    put(end,(count<<18)|method);end+=4;
    for(unsigned i=0;i<count;i++,end+=4)put(end,first+i);
}
static void program(unsigned instructions)
{
    for(unsigned i=0;i<instructions*4;) {
        unsigned n=instructions*4-i;if(n>32)n=32;
        packet(0xb00,n,0xa0000000+i);i+=n;
    }
}
static void finish(void) { put(12,(uint32_t)(end-0x114)/4);put(end,0);end+=4; }
static void bad(const char *fragment)
{
    memset(&result,0xab,sizeof(result));
    assert(!nv2a_vertex_object_decode(b,end,&result,error,sizeof(error)));
    if(!strstr(error,fragment))fprintf(stderr,"expected %s in %s\n",fragment,error);
    assert(strstr(error,fragment) && result.flags==0 && result.instruction_count==0 && result.constant_mask[0]==0);
}
int main(int argc,char **argv)
{
    const unsigned cases[]={1,8,9,136};
    for(unsigned t=0;t<4;t++) {
        unsigned n=cases[t];begin(n);program(n);finish();
        assert(nv2a_vertex_object_decode(b,end,&result,error,sizeof(error)));
        assert(result.instruction_count==n && result.slots[0].stream==1 && result.slots[0].offset==4);
        assert(result.slots[0].kind==NV2A_VERTEX_FLOAT32 && result.slots[0].byte_count==12);
        assert(result.slots[3].kind==NV2A_VERTEX_FLOAT32 && result.slots[3].components==2);
        for(unsigned i=0;i<n*4;i++)assert(result.words[i/4][i%4]==0xa0000000+i);
    }
    begin(9);program(9);
    packet(0x1ea4,1,183);packet(0xb80,32,0x3f800000);packet(0xb80,4,0x40000000);
    packet(0x1ea4,1,184);packet(0xb80,4,0x40400000);finish();
    assert(nv2a_vertex_object_decode(b,end,&result,error,sizeof(error)));
    assert(result.constant_words[183][0]==0x3f800000 && result.constant_words[184][0]==0x40400000 && result.constant_words[191][0]==0x40000000);
    assert(result.constant_mask[2]==(0x1ffull<<55));
    assert(!nv2a_vertex_object_decode(b,end-1,&result,error,sizeof(error)) && strstr(error,"truncated"));
    put(end-4,1);bad("terminator");put(end-4,0);
    put(0x114,(33u<<18)|0xb00);bad("payload count");
    begin(1);packet(0xb00,3,0);finish();bad("program payload");
    begin(2);program(1);finish();bad("count mismatch");
    begin(1);program(2);finish();bad("program payload");
    begin(1);program(1);packet(0xb80,4,0);finish();bad("constant payload/range");
    begin(1);program(1);packet(0x1ea4,1,191);packet(0xb80,8,0);finish();bad("constant payload/range");
    begin(1);program(1);packet(0x1ea4,1,192);finish();bad("constant index");
    begin(1);program(1);packet(0x1ea4,2,0);finish();bad("selector");
    begin(1);program(1);packet(0xb00|0x2000,4,0);finish();bad("unsupported header");
    begin(1);program(1);finish();put(8,137);bad("instruction count");put(8,1);
    put(12,0xffffffff);bad("packet DWORD count");put(12,5);
    put(0x14,16);bad("invalid stream");put(0x14,0);
    put(0x18,0xffffffff);bad("overflow");put(0x18,0);
    put(0x1c,0x99);bad("unsupported Xbox vertex format");put(0x1c,0x32);
    b[0x20]=1;bad("tessellation");b[0x20]=0;
    put(4,0x19);bad("flags");
    const unsigned formats[]={0x02,0x12,0x22,0x32,0x42,0x40,0x11,0x41,0x15,0x45,0x14,0x44,0x16,0x72};
    const unsigned bytes[]={0,4,8,12,16,4,2,8,2,8,1,4,4,12};
    for(unsigned i=0;i<sizeof(formats)/sizeof(*formats);i++) {
        Nv2aVertexSlot s={.stream=7,.offset=99};
        assert(nv2a_vertex_format_decode(formats[i],&s,error,sizeof(error)));
        assert(s.byte_count==bytes[i] && s.stream==7 && s.offset==99);
        if(formats[i]==0x40)assert(s.kind==NV2A_VERTEX_BGRA8 && s.normalized && s.components==4);
    }
    if(argc>1) {
        FILE *f=fopen(argv[1],"rb");assert(f);assert(!fseek(f,0x700,SEEK_SET));
        size_t n=fread(b,1,0x1900,f);fclose(f);
        assert(nv2a_vertex_object_decode(b,n,&result,error,sizeof(error)));
        assert(result.instruction_count==6 && result.packet_dwords==25);
        assert(result.words[5][3]&1);
        assert(result.slots[0].format==0x32 && result.slots[1].format==0x12 && result.slots[2].format==0x40 && result.slots[3].format==0x22);
        for(unsigned i=0;i<3;i++)assert(result.constant_mask[i]==0);
        puts("PASS: actual startup object: six instructions, four declaration slots, no embedded constants");
    }
    puts("PASS: unaligned objects, 8/9/136 instruction packet boundaries, constant continuation/overwrite, format sizes, precise malformed/truncated errors");
}
