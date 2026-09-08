/* Xbox 4361 shader-object parser. Authored here from supplied-XBE disassembly
 * 0x102130/0x1021A0/0x102340/0x102440 and documented Xbox type formats.
 * See docs/NV2A-VERTEX.md. No guest execution or native graphics calls. */
#include "nv2a_vertex_input.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static int object_error(char *error,size_t cap,const char *fmt,...)
{
    if(error && cap) {
        va_list ap; va_start(ap,fmt); vsnprintf(error,cap,fmt,ap); va_end(ap);
    }
    return 0;
}
static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
int nv2a_vertex_format_decode(uint32_t format,Nv2aVertexSlot *s,char *error,size_t cap)
{
    if(error && cap)error[0]=0;
    if(!s)return object_error(error,cap,"null slot output");
    s->format=format; s->kind=NV2A_VERTEX_NONE;
    s->components=s->byte_count=s->normalized=0;
    if(format==2)return 1;
    if(format==0x40) {
        s->kind=NV2A_VERTEX_BGRA8; s->components=s->byte_count=4; s->normalized=1; return 1;
    }
    if(format==0x16) {
        s->kind=NV2A_VERTEX_SNORM11_11_10; s->components=3; s->byte_count=4; s->normalized=1; return 1;
    }
    if(format==0x72) {
        s->kind=NV2A_VERTEX_FLOAT2H; s->components=3; s->byte_count=12; return 1;
    }
    unsigned n=format>>4,type=format&15;
    if(n<1 || n>4 || (type!=1 && type!=2 && type!=4 && type!=5))
        return object_error(error,cap,"unsupported Xbox vertex format 0x%X",format);
    s->components=(uint8_t)n;
    if(type==2) { s->kind=NV2A_VERTEX_FLOAT32; s->byte_count=(uint8_t)(4*n); }
    else if(type==1 || type==5) {
        s->kind=NV2A_VERTEX_SINT16; s->byte_count=(uint8_t)(2*n); s->normalized=type==1;
    } else { s->kind=NV2A_VERTEX_UNORM8; s->byte_count=(uint8_t)n; s->normalized=1; }
    return 1;
}

int nv2a_vertex_object_decode(const void *bytes,size_t available,Nv2aVertexObject *result,
                             char *error,size_t cap)
{
    if(error && cap)error[0]=0;
    if(!result)return object_error(error,cap,"null object output");
    memset(result,0,sizeof(*result));
    if(!bytes || available<0x118)return object_error(error,cap,"truncated 4361 object header/terminator");
    const uint8_t *b=bytes;
    Nv2aVertexObject obj={0};
    obj.flags=le32(b+4); obj.instruction_count=le32(b+8); obj.packet_dwords=le32(b+12);
    memcpy(obj.dimensionality,b+16,4);
    if(!(obj.flags&0x10) || (obj.flags&9))
        return object_error(error,cap,"unsupported vertex object flags 0x%X (requires ordinary program)",obj.flags);
    if(!obj.instruction_count || obj.instruction_count>136)
        return object_error(error,cap,"invalid instruction count %u (maximum136)",obj.instruction_count);
    /* Explicit resource bound, far above 136 instructions plus every physical
     * constant uploaded separately. Checked before length arithmetic/looping. */
    if(!obj.packet_dwords || obj.packet_dwords>4096)
        return object_error(error,cap,"invalid packet DWORD count %u (maximum4096)",obj.packet_dwords);
    if((available-0x118)/4 < obj.packet_dwords)
        return object_error(error,cap,"truncated packet stream: need %u DWORDs and terminator",obj.packet_dwords);
    if(le32(b+0x114+4*(size_t)obj.packet_dwords))
        return object_error(error,cap,"missing zero packet terminator");
    for(unsigned i=0;i<16;i++) {
        const uint8_t *p=b+0x14+16*i;
        Nv2aVertexSlot *s=&obj.slots[i];
        s->stream=le32(p); s->offset=le32(p+4);
        s->tessellation_type=p[12]; s->tessellation_source=p[13];
        char detail[96];
        if(!nv2a_vertex_format_decode(le32(p+8),s,detail,sizeof(detail)))
            return object_error(error,cap,"slot %u: %s",i,detail);
        if(s->stream>=16)return object_error(error,cap,"slot %u: invalid stream %u",i,s->stream);
        if(s->offset>UINT32_MAX-s->byte_count)return object_error(error,cap,"slot %u: offset/size overflow",i);
        if(s->tessellation_type || s->tessellation_source)
            return object_error(error,cap,"slot %u: unsupported tessellation type/source %u/%u",i,s->tessellation_type,s->tessellation_source);
    }
    unsigned pc=0,word_count=0,constant_cursor=192;
    while(pc<obj.packet_dwords) {
        unsigned header_pc=pc;
        uint32_t h=le32(b+0x114+4*(size_t)pc++);
        unsigned count=h>>18,method=h&0x3ffff;
        if(!count || count>32 || count>obj.packet_dwords-pc)
            return object_error(error,cap,"packet DWORD %u: invalid/truncated payload count %u",header_pc,count);
        const uint8_t *payload=b+0x114+4*(size_t)pc;
        if(method==0xb00) {
            if(count%4 || word_count+count>4*obj.instruction_count)
                return object_error(error,cap,"packet DWORD %u: invalid/excess program payload %u",header_pc,count);
            for(unsigned j=0;j<count;j++) {
                unsigned index=word_count++;
                obj.words[index/4][index%4]=le32(payload+4*j);
            }
        } else if(method==0x1ea4) {
            if(count!=1)return object_error(error,cap,"packet DWORD %u: constant selector needs one DWORD",header_pc);
            constant_cursor=le32(payload);
            if(constant_cursor>=192)return object_error(error,cap,"packet DWORD %u: constant index %u exceeds191",header_pc,constant_cursor);
        } else if(method==0xb80) {
            if(count%4 || constant_cursor>=192 || count/4>192-constant_cursor)
                return object_error(error,cap,"packet DWORD %u: invalid constant payload/range (index%u,count%u)",header_pc,constant_cursor,count);
            for(unsigned j=0;j<count/4;j++,constant_cursor++) {
                for(unsigned k=0;k<4;k++)obj.constant_words[constant_cursor][k]=le32(payload+16*j+4*k);
                obj.constant_mask[constant_cursor/64] |= (uint64_t)1<<(constant_cursor%64);
            }
        } else return object_error(error,cap,"packet DWORD %u: unsupported header/method 0x%08X",header_pc,h);
        pc+=count;
    }
    if(word_count!=4*obj.instruction_count)
        return object_error(error,cap,"program count mismatch: declared %u instructions, extracted %u DWORDs",obj.instruction_count,word_count);
    *result=obj;
    return 1;
}
