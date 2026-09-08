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

static int direct_zero_operand(const uint32_t *w, unsigned operand,
                               const float constants[192][4])
{
    unsigned mux=operand==0?(w[2]>>26)&3:(w[2]>>11)&3;
    unsigned index=(w[1]>>13)&255;
    if (mux!=3 || (w[3]&2) || index>=192) return 0;
    unsigned swizzle=operand==0?w[1]&255:(w[2]>>17)&255;
    for (unsigned component=0;component<4;++component)
        if (constants[index][(swizzle>>(component*2))&3]!=0.0f) return 0;
    return 1;
}

int nv2a_vertex_input_is_dead(const uint32_t *words, size_t count, unsigned input,
                             const float constants[192][4])
{
    if (!words || !constants || !count || count>136 || input>=16) return 0;
    for (size_t i=0;i<count;++i) {
        const uint32_t *w=words+i*4;
        unsigned mac=(w[1]>>21)&15, ilu=(w[1]>>25)&7;
        if (mac>13 || ilu>4) return 0;
        if (((w[1]>>9)&15)==input) {
            unsigned a=(w[2]>>26)&3,b=(w[2]>>11)&3,c=(w[3]>>28)&3;
            /* The encoded C register is shared by MAC and ILU. Any live C
             * use is essential, including MAD's addend and paired ILU. */
            if (c==2 && (ilu || mac==3 || mac==4)) return 0;
            if (a==2 && mac &&
                ((mac!=2 && mac!=4) || !direct_zero_operand(w,1,constants))) return 0;
            if (b==2 && mac>=2 && mac!=3 && mac!=13 &&
                ((mac!=2 && mac!=4) || !direct_zero_operand(w,0,constants))) return 0;
        }
        if (w[3]&1) return 1;
    }
    return 0;
}

/* Abstract values contain dependency bits and an exact-zero fact only. They
 * never contain computed vertex values, addresses or evaluated shader math. */
typedef struct InputDependency { uint16_t inputs; unsigned char zero; } InputDependency;
typedef struct DependencyState {
    InputDependency temporary[12][4],output[13][4];
    uint16_t address;
} DependencyState;
static InputDependency dependency_union(InputDependency a,InputDependency b)
{
    return (InputDependency){(uint16_t)(a.inputs|b.inputs),(unsigned char)(a.zero&&b.zero)};
}
static InputDependency dependency_multiply(InputDependency a,InputDependency b)
{
    /* Matches nv_mul: exact zero annihilates even NaN/Inf. Ordinary dot/DST
     * operations do not use this rule because the generator emits native math. */
    if(a.zero||b.zero)return (InputDependency){0,1};
    return (InputDependency){(uint16_t)(a.inputs|b.inputs),0};
}
static int dependency_source(const uint32_t *w,unsigned operand,const DependencyState *state,
                             const float constants[192][4],InputDependency out[4])
{
    unsigned mux,r,swizzle;
    if(!operand){mux=w[2]>>26&3;r=w[2]>>28&15;swizzle=w[1]&255;}
    else if(operand==1){mux=w[2]>>11&3;r=w[2]>>13&15;swizzle=w[2]>>17&255;}
    else {mux=w[3]>>28&3;r=((w[2]&3)<<2)|(w[3]>>30&3);swizzle=w[2]>>2&255;}
    unsigned input=w[1]>>9&15,constant=w[1]>>13&255;
    if(!mux || (mux==1&&r>12) || (mux==3&&!(w[3]&2)&&constant>=192))return 0;
    for(unsigned i=0;i<4;++i) {
        unsigned component=swizzle>>(6-i*2)&3;
        if(mux==1)out[i]=r==12?state->output[0][component]:state->temporary[r][component];
        else if(mux==2)out[i]=(InputDependency){(uint16_t)(1u<<input),0};
        else if(w[3]&2)out[i]=(InputDependency){state->address,0};
        else out[i]=(InputDependency){0,(unsigned char)(constants[constant][component]==0.0f)};
        /* Negation preserves both dependencies and the exact-zero fact. */
    }
    return 1;
}
static void dependency_store(InputDependency destination[4],unsigned mask,const InputDependency source[4])
{
    for(unsigned i=0;i<4;++i)if(mask&(8u>>i))destination[i]=source[i];
}
uint16_t nv2a_vertex_dead_input_mask(const uint32_t *words,size_t count,const float constants[192][4])
{
    if(!words||!constants||!count||count>136)return 0;
    DependencyState state={0};
    /* Zero-initialized GLSL temporaries/outputs are independent, but initially
     * leave their zero facts unknown. This deliberately avoids relying on an
     * Xbox hardware power-on value to prove annihilation. */
    for(size_t pc=0;pc<count;++pc) {
        const uint32_t *w=words+pc*4;
        unsigned mac=w[1]>>21&15,ilu=w[1]>>25&7;
        unsigned mm=w[3]>>24&15,im=w[3]>>16&15,om=w[3]>>12&15,dst=w[3]>>20&15;
        unsigned mux=w[3]>>2&1,output=w[3]>>3&15;
        if(mac>=14||ilu>=5 || (dst>12&&((mac&&mac!=13&&mm)||(ilu&&im&&!mac))))return 0;
        if(om&&(!(w[3]&(1u<<11)) || !(mux?ilu:mac) || (mac==13&&!mux) ||
                output==1||output==2||output==7||output==8||output>=13))return 0;
        InputDependency a[4]={{0}},b[4]={{0}},c[4]={{0}},m[4]={{0}},l[4]={{0}};
        if(mac&&!dependency_source(w,0,&state,constants,a))return 0;
        if(mac>=2&&mac!=3&&mac!=13&&!dependency_source(w,1,&state,constants,b))return 0;
        if((ilu||mac==3||mac==4)&&!dependency_source(w,2,&state,constants,c))return 0;
        for(unsigned i=0;i<4;++i) {
            switch(mac) {
            case 1:m[i]=a[i];break;
            case 2:m[i]=dependency_multiply(a[i],b[i]);break;
            case 3:m[i]=dependency_union(a[i],c[i]);break;
            case 4:m[i]=dependency_union(dependency_multiply(a[i],b[i]),c[i]);break;
            case 5:case 6:case 7:
                for(unsigned j=0;j<(mac==7?4u:3u);++j)m[i].inputs|=a[j].inputs|b[j].inputs;
                if(mac==6)m[i].inputs|=b[3].inputs;
                break;
            case 8:
                if(i==1)m[i].inputs=a[1].inputs|b[1].inputs;
                else if(i==2)m[i]=a[2];else if(i==3)m[i]=b[3];
                break;
            case 9:case 10:case 11:case 12:m[i].inputs=a[i].inputs|b[i].inputs;break;
            default:break;
            }
            if(ilu==1)l[i]=c[i];else if(ilu)l[i].inputs=c[0].inputs;
        }
        /* Both sources/results precede every write, matching paired old A0
         * and temporary reads. Order mirrors the native GLSL compiler. */
        if(om) {
            const InputDependency *value=mux?l:m;
            if(output==5) {
                for(unsigned i=0;i<4;++i)if(om&(8u>>i)){state.output[5][0]=value[i];break;}
            } else dependency_store(state.output[output],om,value);
        }
        if(ilu&&im) {
            unsigned idst=mac?1:dst;
            dependency_store(idst==12?state.output[0]:state.temporary[idst],im,l);
        }
        if(mac&&mac!=13&&mm&&!(ilu&&dst==1))
            dependency_store(dst==12?state.output[0]:state.temporary[dst],mm,m);
        if(mac==13)state.address=a[0].inputs;
        if(w[3]&1) {
            uint16_t live=0;
            for(unsigned o=0;o<13;++o)for(unsigned i=0;i<4;++i)live|=state.output[o][i].inputs;
            return (uint16_t)~live;
        }
    }
    return 0;
}
