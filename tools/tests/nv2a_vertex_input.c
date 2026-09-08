#include "../../src/nv2a_vertex_input.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char storage[20001];
static unsigned char *b=storage+1; /* Deliberately misaligned object. */
static size_t end;
static Nv2aVertexObject result;
static char error[256];
static void test_dead_input(void)
{
    float constants[192][4]={{0}};
    uint32_t w[8]={0,(4u<<21)|(122u<<13)|(6u<<9),
        (3u<<26)|(0x1bu<<17)|(2u<<11)|(0x1bu<<2),
        (1u<<28)|(15u<<24)|(2u<<20)|1};
    assert(nv2a_vertex_input_is_dead(w,1,6,constants));
    constants[122][0]=-0.0f; assert(nv2a_vertex_input_is_dead(w,1,6,constants));
    constants[122][0]=1e-30f; assert(!nv2a_vertex_input_is_dead(w,1,6,constants));
    constants[122][0]=1; assert(!nv2a_vertex_input_is_dead(w,1,6,constants));
    constants[122][0]=NAN; assert(!nv2a_vertex_input_is_dead(w,1,6,constants));
    constants[122][0]=0; constants[122][1]=1;
    assert(nv2a_vertex_input_is_dead(w,1,6,constants)); /* Only c122.x selected. */
    w[1]|=0x55; assert(!nv2a_vertex_input_is_dead(w,1,6,constants)); w[1]&=~255u;
    w[3]|=2; assert(!nv2a_vertex_input_is_dead(w,1,6,constants)); w[3]&=~2u;
    w[3]=(w[3]&~(3u<<28))|(2u<<28);
    assert(!nv2a_vertex_input_is_dead(w,1,6,constants)); /* MAD addend is live. */
    w[1]|=1u<<25; assert(!nv2a_vertex_input_is_dead(w,1,6,constants)); /* ILU read. */
    w[1]&=~(7u<<25); w[3]=(w[3]&~(3u<<28))|(1u<<28);
    w[1]=(w[1]&~(15u<<21))|(7u<<21);
    assert(!nv2a_vertex_input_is_dead(w,1,6,constants)); /* DP4 not in narrow proof. */
    w[1]=(w[1]&~(15u<<21))|(2u<<21);
    assert(nv2a_vertex_input_is_dead(w,1,6,constants));
    /* Also prove the symmetric A=input/B=zero case. */
    w[2]=(2u<<26)|(3u<<11);
    assert(nv2a_vertex_input_is_dead(w,1,6,constants));
    w[3]&=~1u;
    assert(!nv2a_vertex_input_is_dead(w,1,6,constants)); /* FINAL required. */
    w[5]=(1u<<21)|(6u<<9)|0x1b; w[6]=2u<<26;w[7]=(15u<<24)|1;
    assert(!nv2a_vertex_input_is_dead(w,2,6,constants)); /* Later MOV needs input. */
    puts("PASS: dead-input proof checks all uses/live exact constants, rejects nearzero/NaN/relative/ILU/addend/other arithmetic");
}
static void test_transitive_dependencies(void)
{
    float constants[192][4]={{0}};
    const unsigned v6=1u<<6;
    /* MOV r2,v6; MUL oPos,r2,c122.x. Data can pass through temporaries and
     * relative constant addressing before the final exact-zero annihilator. */
    uint32_t words[][4]={
        {0,(1u<<21)|(6u<<9)|0x1b,2u<<26,(15u<<24)|(2u<<20)},
        {0,(2u<<21)|(122u<<13)|0x1b,(2u<<28)|(1u<<26)|(3u<<11),(15u<<12)|(1u<<11)|1},
        {0,0,0,0}
    };
    assert(nv2a_vertex_dead_input_mask(&words[0][0],2,constants)&v6);
    constants[122][0]=1e-30f;assert(!(nv2a_vertex_dead_input_mask(&words[0][0],2,constants)&v6));
    constants[122][0]=NAN;assert(!(nv2a_vertex_dead_input_mask(&words[0][0],2,constants)&v6));
    constants[122][0]=0;
    /* An earlier observable copy remains live even if a later result is zero. */
    words[0][3]|=(15u<<12)|(1u<<11)|(3u<<3);
    assert(!(nv2a_vertex_dead_input_mask(&words[0][0],2,constants)&v6));words[0][3]&=~0xffffu;
    /* Masked stores must retain untouched components. R12 aliases oPos. */
    words[0][3]=(15u<<24)|(12u<<20);
    words[1][3]=(8u<<12)|(1u<<11)|1;
    assert(!(nv2a_vertex_dead_input_mask(&words[0][0],2,constants)&v6));
    words[1][3]=(15u<<12)|(1u<<11)|1;
    assert(nv2a_vertex_dead_input_mask(&words[0][0],2,constants)&v6);
    /* ARL v6.x paired with ILU MOV oD0,c100[A0] uses the OLD address. */
    words[0][1]=(13u<<21)|(1u<<25)|(100u<<13)|(6u<<9);
    words[0][2]=(2u<<26)|(0x1bu<<2);
    words[0][3]=(3u<<28)|(15u<<12)|(1u<<11)|(3u<<3)|4|2|1;
    assert(nv2a_vertex_dead_input_mask(&words[0][0],1,constants)&v6);
    words[0][3]&=~1u;
    words[1][1]=(1u<<21)|(100u<<13)|0x1b;words[1][2]=3u<<26;
    words[1][3]=(15u<<12)|(1u<<11)|2|1;
    assert(!(nv2a_vertex_dead_input_mask(&words[0][0],2,constants)&v6));
    /* A second ARL sourced from v0 erases only the address dependency. */
    memcpy(words[2],words[1],sizeof(words[1]));
    words[1][1]=(13u<<21);words[1][2]=2u<<26;words[1][3]=0;
    assert(nv2a_vertex_dead_input_mask(&words[0][0],3,constants)&v6);
    /* Paired ILU writes R1 and suppresses MAC R1; both see old sources. */
    words[0][1]=(1u<<21)|(1u<<25)|(100u<<13)|(6u<<9)|0x1b;
    words[0][2]=(2u<<26)|(0x1bu<<2);
    words[0][3]=(3u<<28)|(15u<<24)|(1u<<20)|(15u<<16);
    words[1][1]=(1u<<21)|0x1b;words[1][2]=(1u<<28)|(1u<<26);
    words[1][3]=(15u<<12)|(1u<<11)|1;
    assert(nv2a_vertex_dead_input_mask(&words[0][0],2,constants)&v6);
    /* Fog writes only first active source component to oFog.x. */
    words[0][1]=(1u<<21)|(6u<<9)|0x1b;words[0][2]=2u<<26;words[0][3]=(8u<<24)|(1u<<20);
    words[1][3]=(4u<<12)|(1u<<11)|(5u<<3)|1;
    assert(nv2a_vertex_dead_input_mask(&words[0][0],2,constants)&v6);
    words[1][3]=(8u<<12)|(1u<<11)|(5u<<3)|1;
    assert(!(nv2a_vertex_dead_input_mask(&words[0][0],2,constants)&v6));
    words[1][3]&=~(1u<<11);assert(nv2a_vertex_dead_input_mask(&words[0][0],2,constants)==0);
    words[1][3]|=1u<<11;words[1][1]|=15u<<21;
    assert(nv2a_vertex_dead_input_mask(&words[0][0],2,constants)==0);
    puts("PASS: transitive dependency proof: exactzero/nearzero/NaN, observable stores, component masks/R12, oldA0, address overwrite, pairedR1, fog selector and fail-closed op/state writes");
}
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
    test_dead_input();
    test_transitive_dependencies();
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
