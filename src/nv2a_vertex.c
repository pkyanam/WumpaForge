/* SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
 * Native NV2A microcode -> GLSL 4.10 compiler, written for this workspace.
 * Format/semantics reference: xemu fdfb5a8, vsh-prog.c and vsh.c.
 * Reference copyrights: espes (2012), Jannik Vogel (2014), Matt Borgerson
 * (2025); antecedents Aaron Robinson/Kingofc (2004), Shadow_tj/PatrickvL (2007).
 * See docs/NV2A-VERTEX.md for exact provenance and supported semantics.
 */
#include "nv2a_vertex.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef struct Emit {
    char *out, *error;
    size_t cap, used, error_cap, slot;
    int failed;
    Nv2aVertexInfo info;
} Emit;
static void fail(Emit *e, const char *fmt, ...)
{
    if (e->failed) return;
    e->failed = 1;
    if (!e->error || !e->error_cap) return;
    int n = snprintf(e->error, e->error_cap, "instruction %zu: ", e->slot);
    if (n < 0 || (size_t)n >= e->error_cap) return;
    va_list ap; va_start(ap, fmt);
    vsnprintf(e->error + n, e->error_cap - (size_t)n, fmt, ap);
    va_end(ap);
}
static void emit(Emit *e, const char *fmt, ...)
{
    if (e->failed) return;
    va_list ap; va_start(ap, fmt);
    int n = vsnprintf(e->out + e->used, e->cap - e->used, fmt, ap);
    va_end(ap);
    if (n < 0 || (size_t)n >= e->cap - e->used) fail(e, "GLSL source buffer too small");
    else e->used += (size_t)n;
}
static unsigned bits(uint32_t w, unsigned at, unsigned n) { return (w >> at) & ((1u << n)-1); }
static void mask_text(unsigned mask, char out[5])
{
    unsigned n = 0;
    for (unsigned i=0; i<4; i++) if (mask & (8u>>i)) out[n++] = "xyzw"[i];
    out[n] = 0;
}
static void source_reg(Emit *e, const uint32_t *w, unsigned s, char out[96])
{
    unsigned mux, r, neg, swz;
    if (s == 0) { mux=bits(w[2],26,2); r=bits(w[2],28,4); neg=bits(w[1],8,1); swz=bits(w[1],0,8); }
    else if (s == 1) { mux=bits(w[2],11,2); r=bits(w[2],13,4); neg=bits(w[2],25,1); swz=bits(w[2],17,8); }
    else { mux=bits(w[3],28,2); r=(bits(w[2],0,2)<<2)|bits(w[3],30,2); neg=bits(w[2],10,1); swz=bits(w[2],2,8); }
    char base[48] = {0};
    if (mux == 1) {
        if (r > 12) { fail(e,"source %c uses invalid temporary R%u", 'A'+s,r); return; }
        snprintf(base,sizeof(base),r == 12 ? "o[0]" : "r[%u]",r);
    } else if (mux == 2) {
        unsigned v=bits(w[1],9,4); e->info.input_mask |= 1u<<v;
        snprintf(base,sizeof(base),"v%u",v);
    } else if (mux == 3) {
        unsigned c=bits(w[1],13,8);
        if (bits(w[3],1,1)) {
            e->info.relative_constants = 1;
            for (unsigned i=0;i<3;i++) e->info.constant_mask[i]=UINT64_MAX;
            snprintf(base,sizeof(base),"nv_constant(a0+%u.0)",c);
        } else {
            if (c>=192) { fail(e,"constant c%u is outside physical c0..c191",c); return; }
            e->info.constant_mask[c/64] |= (uint64_t)1 << (c%64);
            snprintf(base,sizeof(base),"u_vconstants[%u]",c);
        }
    } else { fail(e,"source %c has reserved register mux 0",'A'+s); return; }
    snprintf(out,96,"%s%s.%c%c%c%c",neg?"-":"",base,
             "xyzw"[swz>>6&3],"xyzw"[swz>>4&3],"xyzw"[swz>>2&3],"xyzw"[swz&3]);
}
static void store_mask(Emit *e,const char *dst,unsigned mask,const char *src)
{
    if (!mask) return;
    char m[5]; mask_text(mask,m);
    emit(e,"  %s.%s = %s.%s;\n",dst,m,src,m);
}
static void instruction(Emit *e,const uint32_t *w)
{
    unsigned mac=bits(w[1],21,4), ilu=bits(w[1],25,3);
    unsigned mm=bits(w[3],24,4), im=bits(w[3],16,4), om=bits(w[3],12,4);
    unsigned dst=bits(w[3],20,4), mux=bits(w[3],2,1);
    char a[96]="vec4(0.0)", b[96]="vec4(0.0)", c[96]="vec4(0.0)";
    if (mac>=14) { fail(e,"unsupported MAC opcode %u (reserved)",mac); return; }
    if (ilu>=5) { fail(e,"unsupported ILU opcode %u (EXP/LOG/LIT)",ilu); return; }
    if (dst>12 && ((mac&&mac!=13&&mm)||(ilu&&im&&!mac))) { fail(e,"destination R%u is invalid",dst); return; }
    if (mac) source_reg(e,w,0,a);
    if (mac>=2 && mac!=3 && mac!=13) source_reg(e,w,1,b);
    if (ilu || mac==3 || mac==4) source_reg(e,w,2,c);
    if (e->failed) return;
    emit(e,"  // slot %zu: MAC %u ILU %u\n  { vec4 A=%s; vec4 B=%s; vec4 C=%s;\n",e->slot,mac,ilu,a,b,c);
    /* Evaluate both halves before committing writes: paired ILU reads the old
     * temporaries, and paired ILU writes R1 while MAC targeting R1 is masked. */
    const char *mac_expr[]={"vec4(0.0)","A","nv_mul(A,B)","A+C","nv_mul(A,B)+C",
        "vec4(dot(A.xyz,B.xyz))","vec4(dot(A.xyz,B.xyz)+B.w)","vec4(dot(A,B))",
        "vec4(1.0,A.y*B.y,A.z,B.w)","min(A,B)","max(A,B)",
        "vec4(lessThan(A,B))","vec4(greaterThanEqual(A,B))","vec4(0.0)"};
    const char *ilu_expr[]={"vec4(0.0)","C","vec4(1.0/C.x)","vec4(nv_clamp(1.0/C.x))","vec4(inversesqrt(abs(C.x)))"};
    emit(e,"  vec4 M=%s; vec4 I=%s;\n",mac_expr[mac],ilu_expr[ilu]);
    if (om) {
        if (mac==13 && !mux) { fail(e,"ARL routed to vector output is not implemented"); return; }
        if (!(mux?ilu:mac)) { fail(e,"output mux selects NOP operation"); return; }
        if (!bits(w[3],11,1)) { fail(e,"writable constant/state shaders are not implemented"); return; }
        unsigned o=bits(w[3],3,8)&15;
        if (o==1 || o==2 || o>=13 || o==7 || o==8) { fail(e,"unsupported output register o%u",o); return; }
        e->info.output_mask |= 1u<<o;
        if (o==5) {
            char m[5]; mask_text(om,m);
            emit(e,"  o[%u].x = %s.%c;\n",o,mux?"I":"M",m[0]);
        } else {
            char d[16]; snprintf(d,sizeof(d),"o[%u]",o);
            store_mask(e,d,om,mux?"I":"M");
        }
    }
    if (ilu && im) {
        unsigned idst=mac?1:dst;
        char d[16]; snprintf(d,sizeof(d),idst==12?"o[0]":"r[%u]",idst);
        store_mask(e,d,im,"I");
    }
    if (mac && mac!=13 && mm && !(ilu && dst==1)) {
        char d[16]; snprintf(d,sizeof(d),dst==12?"o[0]":"r[%u]",dst);
        store_mask(e,d,mm,"M");
    }
    /* ARL writes its own address register irrespective of the vector mask.
     * A/B/C above snapshot old A0, including a paired ILU constant read. */
    if (mac==13) emit(e,"  a0=floor(A.x);\n");
    emit(e,"  }\n");
}
int nv2a_vertex_generate(const uint32_t *words,size_t count,char *out,size_t cap,
                        Nv2aVertexInfo *info,char *error,size_t error_cap)
{
    Emit e={.out=out,.cap=cap,.error=error,.error_cap=error_cap};
    if (info) memset(info,0,sizeof(*info));
    if (error && error_cap) error[0]=0;
    if (!words || !out || !cap || !count || count>136) {
        fail(&e,"invalid argument or instruction count (maximum136)");
        if(out&&cap)out[0]=0; return 0;
    }
    out[0]=0;
    emit(&e,"#version 410 core\n");
    for(unsigned i=0;i<16;i++)emit(&e,"layout(location=%u) in vec4 v%u;\n",i,i);
    emit(&e,"uniform vec4 u_vconstants[192];\nuniform vec4 u_nv2a_viewport;\nuniform vec2 u_nv2a_depth;\n"
         "out vec4 vD0,vD1,vT0,vT1,vT2,vT3;\nout float vFog;\n"
         "float nv_clamp(float x) { float a=clamp(abs(x),uintBitsToFloat(0x1f800000u),uintBitsToFloat(0x5f800000u)); return (floatBitsToUint(x)&0x80000000u)!=0u ? -a:a; }\n"
         "vec4 nv_mul(vec4 a,vec4 b) { vec4 p=a*b; for(int j=0;j<4;j++) if(a[j]==0.0||b[j]==0.0)p[j]=0.0; return p; }\n"
         "vec4 nv_constant(float index) { if(index>=0.0 && index<192.0) return u_vconstants[int(index)]; return vec4(0.0); }\n"
         "void main() {\n  vec4 r[12]; vec4 o[13]; float a0=0.0;\n"
         "  for(int j=0;j<12;j++)r[j]=vec4(0.0);\n"
         "  for(int j=0;j<13;j++)o[j]=vec4(0.0,0.0,0.0,1.0);\n");
    int final=0;
    for(e.slot=0;e.slot<count && !e.failed;e.slot++) {
        instruction(&e,words+4*e.slot);
        e.info.instruction_count=(unsigned)e.slot+1;
        if(words[4*e.slot+3]&1) { final=1; break; }
    }
    if(!final && !e.failed)fail(&e,"missing FINAL instruction bit");
    emit(&e,"  vD0=clamp(o[3],0.0,1.0); vD1=clamp(o[4],0.0,1.0);\n"
         "  vT0=o[9]; vT1=o[10]; vT2=o[11]; vT3=o[12]; vFog=o[5].x; gl_PointSize=o[6].x;\n"
         "  vec4 p=o[0]; p.xy=trunc(p.xy*16.0)/16.0; p.w=nv_clamp(p.w);\n"
         "  vec2 screen=(p.xy-u_nv2a_viewport.xy)/u_nv2a_viewport.zw;\n"
         "  float span=u_nv2a_depth.y-u_nv2a_depth.x; float z=span!=0.0 ? (p.z-u_nv2a_depth.x)/span : 0.0;\n"
         "  gl_Position=vec4(vec3(2.0*screen.x-1.0,1.0-2.0*screen.y,2.0*z-1.0)*p.w,p.w);\n}\n");
    if(e.failed) { out[0]=0; return 0; }
    if(info)*info=e.info;
    return 1;
}
