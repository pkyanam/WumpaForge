/* Xbox context-layout checks plus public SHA-1 vectors; no game inputs. */
#include "kernel/kernel.h"
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static unsigned failures;
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL line%d: %s\n",__LINE__,#x);++failures; } } while(0)
static uint32_t word(const unsigned char *p) { uint32_t n;memcpy(&n,p,4);return n; }
static void digest_check(const unsigned char *digest,const char *expected)
{
    char text[41];for(unsigned i=0;i<20;i++)snprintf(text+2*i,3,"%02x",digest[i]);
    CHECK(strcmp(text,expected)==0);
}
static void guarded_vector(const char *message,size_t length,const char *expected)
{
    _Alignas(8) unsigned char memory[8+116+8],output[28];
    memset(memory,0xA5,sizeof memory);memset(output,0x5A,sizeof output);
    XBOX_SHA_CONTEXT *ctx=(void*)(memory+8);
    xbox_XcSHAInit(ctx);
    CHECK(word(memory+8+24)==0x67452301u);
    CHECK(word(memory+8+44)==0 && word(memory+8+48)==0);
    /* Context prefix is reserved by this implementation, never hash state. */
    for(unsigned i=0;i<24;i++)CHECK(memory[8+i]==0xA5);
    for(size_t i=0;i<length;i++)xbox_XcSHAUpdate(ctx,(const UCHAR*)message+i,1);
    xbox_XcSHAUpdate(ctx,NULL,0);
    xbox_XcSHAFinal(ctx,output+4);digest_check(output+4,expected);
    for(unsigned i=0;i<8;i++){CHECK(memory[i]==0xA5);CHECK(memory[124+i]==0xA5);}
    for(unsigned i=0;i<4;i++){CHECK(output[i]==0x5A);CHECK(output[24+i]==0x5A);}
}
int main(void)
{
    CHECK(sizeof(ULONG)==4);
    CHECK(sizeof(XBOX_SHA_CONTEXT)==116);
    CHECK(offsetof(XBOX_SHA_CONTEXT,State)==24);
    CHECK(offsetof(XBOX_SHA_CONTEXT,Count)==44);
    CHECK(offsetof(XBOX_SHA_CONTEXT,Buffer)==52);
    guarded_vector("",0,"da39a3ee5e6b4b0d3255bfef95601890afd80709");
    guarded_vector("abc",3,"a9993e364706816aba3e25717850c26c9cd0d89d");
    const char multi[]="abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    guarded_vector(multi,strlen(multi),"84983e441c3bd26ebaae4aa1f95129e5e54670f1");
    XBOX_SHA_CONTEXT one,copy;UCHAR block[1000],digest[20],second[20];
    memset(block,'a',sizeof block);xbox_XcSHAInit(&one);
    for(unsigned i=0;i<500;i++)xbox_XcSHAUpdate(&one,block,sizeof block);
    memcpy(&copy,&one,sizeof copy);
    for(unsigned i=0;i<500;i++){
        xbox_XcSHAUpdate(&one,block,sizeof block);
        for(unsigned j=0;j<10;j++)xbox_XcSHAUpdate(&copy,block+j*100,100);
    }
    xbox_XcSHAFinal(&one,digest);xbox_XcSHAFinal(&copy,second);
    digest_check(digest,"34aa973cd4c4daa4f61eeb2bdbad27316534016f");
    CHECK(memcmp(digest,second,20)==0);
    if(failures){fprintf(stderr,"%u failed SHA context/vector checks\n",failures);return 1;}
    puts("PASS Xbox SHA116-byte context,24-byte reserved prefix, state/count/buffer offsets, canaries, empty/abc/multiblock/million-a vectors and cloned incremental contexts");
    return 0;
}
