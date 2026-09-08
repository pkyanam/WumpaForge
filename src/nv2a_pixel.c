/* Independent GLSL source generator from the published NV register-combiner
 * equations and Xbox wire-field definitions. No upstream implementation copied.
 * Layout/bitfield/reference provenance and limitations: docs/PIXEL-SHADERS.md. */
#include "nv2a_pixel.h"
#include <stdarg.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
_Static_assert(sizeof(Nv2aPixelDef) == 240, "Xbox pixel definition size");
_Static_assert(offsetof(Nv2aPixelDef, combiner_count) == 0xd4, "combiner count offset");
_Static_assert(offsetof(Nv2aPixelDef, final_constants) == 0xec, "constant mapping offset");
typedef struct Generator {
    const Nv2aPixelDef *d;
    char *out, *error; size_t capacity, used, error_capacity;
    unsigned defined[16], stage, failed;
} Generator;
static int fail(Generator *g, const char *format, ...)
{
    if (!g->failed && g->error && g->error_capacity) {
        va_list ap; va_start(ap, format); vsnprintf(g->error, g->error_capacity, format, ap); va_end(ap);
    }
    g->failed = 1; return 0;
}
static void emit(Generator *g, const char *format, ...)
{
    if (g->failed) return;
    va_list ap; va_start(ap, format);
    int n = vsnprintf(g->out + g->used, g->capacity - g->used, format, ap); va_end(ap);
    if (n < 0 || (size_t)n >= g->capacity - g->used) { fail(g, "GLSL source buffer too small"); return; }
    g->used += (size_t)n;
}
int nv2a_pixel_read_definition(const void *bytes, size_t length, Nv2aPixelDef *d)
{
    if (!bytes || !d || length < sizeof(*d)) return 0;
    const uint8_t *p = bytes; uint32_t words[60];
    for (unsigned i = 0; i < 60; ++i, p += 4)
        words[i] = (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
    memcpy(d, words, sizeof(*d)); return 1;
}
uint32_t nv2a_pixel_pack_constant(const float color[4])
{
    uint32_t c[4];
    for (unsigned i=0;i<4;++i) {
        float x=color[i];
        /* CVTTSS2SI's invalid result ends in byte0 for NaN. Infinities clamp. */
        if (isnan(x) || x<0.0f) x=0.0f;
        if (x>1.0f) x=1.0f;
        /* X87 arithmetic then an explicit float store before CVTTSS2SI. */
        float rounded=(float)((double)x*255.0+0.5);
        c[i]=(uint32_t)rounded;
    }
    return (c[3]<<24)|(c[0]<<16)|(c[1]<<8)|c[2];
}
int nv2a_pixel_set_constants(Nv2aPixelDef *active,uint32_t index,const float *colors,uint32_t count)
{
    if(!active || index>16 || count>16-index || (count && !colors))return 0;
    for(unsigned n=0;n<count;++n) {
        unsigned reg=index+n;uint32_t color=nv2a_pixel_pack_constant(colors+n*4);
        for(unsigned s=0;s<8;++s) {
            if(((active->c0_mapping>>(s*4))&15)==reg)active->constant0[s]=color;
            if(((active->c1_mapping>>(s*4))&15)==reg)active->constant1[s]=color;
        }
        if((active->final_constants&15)==reg)active->final_constant0=color;
        if(((active->final_constants>>4)&15)==reg)active->final_constant1=color;
    }
    return 1;
}

/* Fixed pipeline lowering, derived from Xbox 4361 10A9B0/10A910 and published
 * texture-operation equations. Keep arithmetic in the already validated native
 * combiner generator, including simultaneous RGB/alpha writes and saturation. */
static int fixed_argument(unsigned stage, unsigned value, int alpha,
                          const unsigned dimensions[4], unsigned *byte)
{
    static const unsigned regs[6] = {4, 12, 0, 1, 5, 13};
    unsigned source = value & 15;
    if ((value & ~0x3fu) || source > 5 || (source == 2 && !dimensions[stage])) return 0;
    *byte = source == 2 ? 8 + stage : regs[source];
    if (alpha || (value & 0x20)) *byte |= 0x10;
    if (value & 0x10) *byte |= 0x20;
    return 1;
}
static int fixed_portion(unsigned stage, unsigned op, unsigned arg1, unsigned arg2,
                         unsigned result, int alpha, const unsigned dimensions[4],
                         uint32_t *inputs, uint32_t *outputs)
{
    *inputs = *outputs = 0;
    if (alpha && op == 1) return 1; /* No alpha write, confirmed 10AA9D/10ADBC. */
    if (op < 2 || op > 10 || (result != 1 && result != 5)) return 0;
    unsigned a = 0, b = 0;
    if (op != 3 && !fixed_argument(stage, arg1, alpha, dimensions, &a)) return 0;
    if (op != 2 && !fixed_argument(stage, arg2, alpha, dimensions, &b)) return 0;
    unsigned dst = result == 5 ? 13 : 12;
    uint32_t mapping = op == 5 ? 0x10000 : op == 6 ? 0x20000 :
                       op == 8 ? 0x8000 : op == 9 ? 0x18000 : 0;
    /* Inputs A*B+C*D, destination is sum; 0x20 is unsigned-inverted ZERO=1. */
    if (op == 2 || op == 3) *inputs = ((op == 2 ? a : b) << 24) | 0x00200000u;
    else if (op >= 4 && op <= 6) *inputs = (a << 24) | (b << 16);
    else *inputs = (a << 24) | 0x00200000u | (b << 8) | (op == 10 ? 0x40u : 0x20u);
    *outputs = (dst << 8) | mapping;
    return 1;
}
int nv2a_pixel_fixed_definition(const uint32_t states[4][32], const unsigned dimensions[4],
                                uint32_t texture_factor, Nv2aPixelDef *definition,
                                char *error, size_t error_capacity)
{
    if (error && error_capacity) error[0] = 0;
    if (definition) memset(definition, 0, sizeof(*definition));
    if (!states || !dimensions || !definition) {
        if (error && error_capacity) snprintf(error, error_capacity, "invalid fixed-stage arguments");
        return 0;
    }
    Nv2aPixelDef d = {0};
    /* CURRENT initially means diffuse, including alpha when its operation is
     * disabled in the first stage. A real combiner copy establishes this state. */
    d.rgb_inputs[0] = 0x04200000; d.alpha_inputs[0] = 0x14200000;
    d.rgb_outputs[0] = d.alpha_outputs[0] = 0xC00;
    d.combiner_count = 1; d.constant0[0] = texture_factor;
    for (unsigned stage = 0; stage < 4; ++stage) {
        const uint32_t *s = states[stage];
        if (s[12] == 1) break;
        unsigned n = d.combiner_count;
        if (s[9] || s[10] || s[11] || s[21]) {
            if (error && error_capacity) snprintf(error, error_capacity,
                "fixed stage %u unsupported color key/sign/alpha kill/texture transform", stage);
            return 0;
        }
        if (dimensions[stage] != 0 && dimensions[stage] != 2 &&
            dimensions[stage] != 3 && dimensions[stage] != 4) {
            if (error && error_capacity) snprintf(error, error_capacity, "fixed stage %u invalid texture dimension", stage);
            return 0;
        }
        if (!fixed_portion(stage, s[12], s[14], s[15], s[20], 0, dimensions,
                           &d.rgb_inputs[n], &d.rgb_outputs[n]) ||
            !fixed_portion(stage, s[16], s[18], s[19], s[20], 1, dimensions,
                           &d.alpha_inputs[n], &d.alpha_outputs[n])) {
            if (error && error_capacity) snprintf(error, error_capacity,
                "fixed stage %u invalid/unsupported COLOROP=%u ALPHAOP=%u arguments/result/binding", stage, s[12], s[16]);
            return 0;
        }
        /* Original107CA0 selects PROJECT2D/PROJECT3D/CUBEMAP from each bound
         * resource header. Programmed vertex outputs retain their own T0..T3. */
        unsigned mode = dimensions[stage] == 2 ? 1 : dimensions[stage] == 3 ? 2 : dimensions[stage] == 4 ? 3 : 0;
        d.texture_modes |= mode << (stage * 5);
        ++d.combiner_count;
    }
    *definition = d;
    return 1;
}

static void rgba(uint32_t color, float v[4])
{
    v[0] = ((color >> 16) & 255)/255.0f; v[1] = ((color >> 8)&255)/255.0f;
    v[2] = (color & 255)/255.0f; v[3] = (color >> 24)/255.0f;
}
static const char *register_name(unsigned r)
{
    static const char *names[16] = {"vec4(0.0)", "", "", "fog", "d0", "d1", "", "",
                                     "t0", "t1", "t2", "t3", "r0", "r1", "vsum", "ef"};
    return names[r];
}
/* Input expressions are computed before either portion writes any register. */
static int input(Generator *g, unsigned byte, int alpha, int final_variable, char out[512])
{
    unsigned r = byte & 15, modifier = byte >> 5;
    if (r == 6 || r == 7 || (g->stage < 8 && (r >= 14 || (r == 3 && (byte & 16)))))
        return fail(g, "stage %u invalid input register %u", g->stage, r);
    if (g->stage == 8 && (modifier > 1 ||
        (r >= 14 && (final_variable >= 4 || (byte & 16))) || (r == 14 && final_variable == 0)))
        return fail(g, "invalid final combiner input %d (0x%02X)", final_variable, byte);
    unsigned needed = (byte & 16) ? 8 : alpha ? 4 : 7;
    if (r != 1 && r != 2 && (g->defined[r] & needed) != needed)
        return fail(g, "stage %u reads undefined register %u components 0x%X", g->stage, r, needed);
    char reg[64];
    if (r == 1 || r == 2) {
        unsigned unique = g->d->combiner_count & (r == 1 ? 0x1000u : 0x10000u);
        unsigned stage = g->stage == 8 ? 8 : unique ? g->stage : 0;
        snprintf(reg, sizeof(reg), "u_psconstants[%u]", stage*2+r-1);
    } else snprintf(reg, sizeof(reg), "%s", register_name(r));
    const char *channel = (byte & 16) ? (alpha ? "a" : "aaa") : (alpha ? "b" : "rgb");
    char raw[96]; snprintf(raw, sizeof(raw), "%s.%s", reg, channel);
    switch (modifier) {
    case 0: snprintf(out,512,"max(%s,0.0)",raw); break;
    case 1: snprintf(out,512,"(1.0-clamp(%s,0.0,1.0))",raw); break;
    case 2: snprintf(out,512,"(2.0*max(%s,0.0)-1.0)",raw); break;
    case 3: snprintf(out,512,"(1.0-2.0*max(%s,0.0))",raw); break;
    case 4: snprintf(out,512,"(max(%s,0.0)-0.5)",raw); break;
    case 5: snprintf(out,512,"(0.5-max(%s,0.0))",raw); break;
    case 6: snprintf(out,512,"(%s)",raw); break;
    case 7: snprintf(out,512,"(-%s)",raw); break;
    }
    return 1;
}
static int output_valid(Generator *g, uint32_t word, int alpha)
{
    unsigned flags = word >> 12, mapping = flags & 0x38;
    if (word & 0xfff00000u || (mapping != 0 && mapping != 8 && mapping != 16 &&
        mapping != 24 && mapping != 32 && mapping != 48))
        return fail(g,"stage %u unsupported output word 0x%08X",g->stage,word);
    if (alpha && (flags & 0xc3)) return fail(g,"stage %u RGB-only flags in alpha output",g->stage);
    unsigned dst[3] = {(word >> 4)&15,word&15,(word>>8)&15};
    if ((flags & 3) && dst[2]) return fail(g,"stage %u dot-product with sum/mux destination unsupported",g->stage);
    for (unsigned i=0;i<3;++i) {
        if (dst[i] && dst[i]!=4 && dst[i]!=5 && (dst[i]<8 || dst[i]>13))
            return fail(g,"stage %u invalid output register %u",g->stage,dst[i]);
        for (unsigned j=0;j<i;++j) if (dst[i] && dst[i]==dst[j])
            return fail(g,"stage %u conflicting output register %u",g->stage,dst[i]);
    }
    return 1;
}
static void portion(Generator *g, uint32_t inputs, uint32_t outputs, int alpha)
{
    if (!output_valid(g,outputs,alpha)) return;
    char x[4][512];
    for (unsigned i=0;i<4;++i) if(!input(g,(inputs >> (24-i*8))&255,alpha,-1,x[i]))return;
    const char *type=alpha?"float":"vec3", *tag=alpha?"a":"rgb";
    unsigned flags=outputs>>12, mapping=flags&0x38;
    double bias=(mapping==8 || mapping==24)?-0.5:0.0;
    double scale=(mapping==16 || mapping==24)?2.0:mapping==32?4.0:mapping==48?0.5:1.0;
    emit(g,"  %s p_%s = %s * %s;\n  %s q_%s = %s * %s;\n",type,tag,x[0],x[1],type,tag,x[2],x[3]);
    if(!alpha && (flags&2)) emit(g,"  p_rgb = vec3(dot(%s,%s));\n",x[0],x[1]);
    if(!alpha && (flags&1)) emit(g,"  q_rgb = vec3(dot(%s,%s));\n",x[2],x[3]);
    emit(g,"  %s s_%s = ",type,tag);
    if (flags&4) emit(g,"mux_pick ? q_%s : p_%s;\n",tag,tag);
    else emit(g,"p_%s + q_%s;\n",tag,tag);
    emit(g,"  ab.%s = clamp((p_%s+%.1f)*%.1f,-1.0,1.0);\n",tag,tag,bias,scale);
    emit(g,"  cd.%s = clamp((q_%s+%.1f)*%.1f,-1.0,1.0);\n",tag,tag,bias,scale);
    emit(g,"  sum.%s = clamp((s_%s+%.1f)*%.1f,-1.0,1.0);\n",tag,tag,bias,scale);
}
static void write_outputs(Generator *g,uint32_t outputs,int alpha)
{
    unsigned destinations[3]={(outputs>>4)&15,outputs&15,(outputs>>8)&15};
    const char *values[3]={"ab","cd","sum"};
    for(unsigned i=0;i<3;++i) if(destinations[i]) {
        unsigned r=destinations[i];const char *mask=alpha?"a":"rgb";
        emit(g,"  %s.%s = %s.%s;\n",register_name(r),mask,values[i],mask);
        g->defined[r] |= alpha?8:7;
        if(!alpha && i<2 && (outputs & (i==0 ? 0x80000u : 0x40000u))) {
            emit(g,"  %s.a = %s.b;\n",register_name(r),values[i]);g->defined[r]|=8;
        }
    }
}
static void final_combiner(Generator *g,uint32_t abcd,uint32_t efg)
{
    if(efg & 0x1fu) {fail(g,"reserved final-combiner flag bits 0x%X",efg&255);return;}
    g->stage=8;char x[7][512];
    if(!input(g,(efg>>24)&255,0,4,x[4]) || !input(g,(efg>>16)&255,0,5,x[5]))return;
    emit(g,"  vec4 ef = vec4(%s * %s,0.0);\n",x[4],x[5]);g->defined[15]=15;
    emit(g,"  vec4 vsum = vec4(%s + %s,0.0);\n",
         (efg&0x40)?"(1.0-clamp(d1.rgb,0.0,1.0))":"max(d1.rgb,0.0)",
         (efg&0x20)?"(1.0-clamp(r0.rgb,0.0,1.0))":"max(r0.rgb,0.0)");
    if(efg&0x80)emit(g,"  vsum = clamp(vsum,0.0,1.0);\n");
    g->defined[14]=(g->defined[12]&7)==7 ? 15 : 8;
    for(unsigned i=0;i<4;++i)if(!input(g,(abcd>>(24-i*8))&255,0,(int)i,x[i]))return;
    if(!input(g,(efg>>8)&255,1,6,x[6]))return;
    emit(g,"  fragColor = clamp(vec4(%s*%s+(1.0-%s)*%s+%s,%s),0.0,1.0);\n",x[0],x[1],x[0],x[2],x[3],x[6]);
}
int nv2a_pixel_generate(const Nv2aPixelDef *d,const Nv2aPixelOptions *options,
                        char *source,size_t capacity,Nv2aPixelInfo *info,char *error,size_t error_capacity)
{
    if(error && error_capacity)error[0]=0;
    if(source && capacity)source[0]=0;
    if(info)memset(info,0,sizeof(*info));
    Generator g={.d=d,.out=source,.capacity=capacity,.error=error,.error_capacity=error_capacity};
    if(!d || !options || !source || !capacity || !info)return fail(&g,"invalid generator argument");
    unsigned stages=d->combiner_count&255;
    if(stages>8 || (d->combiner_count & ~0x111ffu))return fail(&g,"unsupported combiner count/control 0x%08X",d->combiner_count);
    if(d->texture_modes & ~0xfffffu || options->rectangle_texture_mask&~15u)
        return fail(&g,"reserved texture mode or rectangle bits");
    info->stages=stages;info->c0_mapping=d->c0_mapping;info->c1_mapping=d->c1_mapping;info->final_constants=d->final_constants;
    for(unsigned i=0;i<8;++i){rgba(d->constant0[i],info->constants[i*2]);rgba(d->constant1[i],info->constants[i*2+1]);}
    rgba(d->final_constant0,info->constants[16]);rgba(d->final_constant1,info->constants[17]);
    emit(&g,"#version 410 core\nin vec4 vD0,vD1,vT0,vT1,vT2,vT3;\nin float vFog;\nlayout(location=0) out vec4 fragColor;\nuniform vec4 u_psconstants[18];\nuniform vec3 u_fogcolor;\nuniform int u_alpha_enable,u_alpha_func;\nuniform float u_alpha_ref;\n");
    for(unsigned i=0;i<4;++i) {
        unsigned mode=(d->texture_modes>>(i*5))&31;
        if(mode>5){fail(&g,"texture stage %u mode 0x%X unsupported",i,mode);break;}
        if((options->rectangle_texture_mask&(1u<<i)) && mode!=1){fail(&g,"rectangle texture stage %u requires PROJECT2D",i);break;}
        if(mode>=1 && mode<=3) {
            static const char *types[]={"","2D","3D","Cube"};
            emit(&g,"uniform sampler%s tex%u;\n",types[mode],i);
            info->texture_mask|=1u<<i;info->sampler_dimension[i]=mode==3?4:mode+1;
        }
    }
    emit(&g,"void main() {\n  vec4 d0=vD0,d1=vD1;\n  vec4 fog=vec4(u_fogcolor,%s);\n",
         options->fog_enabled?"clamp(vFog,0.0,1.0)":"1.0");
    for(unsigned i=0;i<4;++i) {
        unsigned mode=(d->texture_modes>>(i*5))&31;
        if(mode==0 || mode==5)emit(&g,"  vec4 t%u=vec4(0.0);\n",i);
        else if(mode==1) {
            if(options->rectangle_texture_mask&(1u<<i))
                emit(&g,"  vec4 t%u=textureProj(tex%u,vec3(vT%u.xy/vec2(textureSize(tex%u,0)),vT%u.w));\n",i,i,i,i,i);
            else emit(&g,"  vec4 t%u=textureProj(tex%u,vT%u.xyw);\n",i,i,i);
        } else if(mode==2)emit(&g,"  vec4 t%u=textureProj(tex%u,vT%u);\n",i,i,i);
        else if(mode==3)emit(&g,"  vec4 t%u=texture(tex%u,vT%u.xyz);\n",i,i,i);
        else if(mode==4)emit(&g,"  vec4 t%u=vT%u;\n",i,i);
        if(mode==5)for(unsigned c=0;c<4;++c)
            emit(&g,"  if(vT%u.%c %s 0.0) discard;\n",i,"xyzw"[c],(d->compare_mode&(1u<<(i*4+c)))?">=":"<");
    }
    /* Uninitialized RGB temporary reads are rejected by the tracker below. */
    emit(&g,"  vec4 r0=vec4(0.0),r1=vec4(0.0);\n  r0.a=%s;\n",(d->texture_modes&31)?"t0.a":"1.0");
    for(unsigned i=0;i<12;++i)g.defined[i]=15;g.defined[12]=8;
    for(unsigned s=0;s<stages && !g.failed;++s) {
        g.stage=s;
        unsigned rgb=d->rgb_outputs[s],alpha=d->alpha_outputs[s];
        unsigned alpha_dst[3]={(alpha>>4)&15,alpha&15,(alpha>>8)&15};
        for(unsigned p=0;p<2;++p)if(rgb&(p==0?0x80000u:0x40000u)) {
            unsigned dest=p==0?(rgb>>4)&15:rgb&15;
            for(unsigned q=0;q<3;++q)if(dest && dest==alpha_dst[q])fail(&g,"stage %u blue-to-alpha conflicts with alpha output",s);
        }
        emit(&g,"  { // general combiner %u\n  vec4 ab=vec4(0.0),cd=vec4(0.0),sum=vec4(0.0);\n",s);
        emit(&g,"  bool mux_pick=%s;\n",(d->combiner_count&0x100)?"r0.a>=0.5":"(uint(clamp(r0.a,0.0,1.0)*255.0)&1u)!=0u");
        portion(&g,d->rgb_inputs[s],rgb,0);portion(&g,d->alpha_inputs[s],alpha,1);
        write_outputs(&g,rgb,0);write_outputs(&g,alpha,1);emit(&g,"  }\n");
    }
    uint32_t abcd=d->final_abcd,efg=d->final_efg;
    if(!abcd && !efg) {
        info->uses_default_final=1;abcd=options->fog_enabled?0x130c0300u:0xcu;
        if(options->specular_enabled)abcd+=options->fog_enabled?0x20000u:2;
        efg=0x1c80;
    }
    if(!g.failed)final_combiner(&g,abcd,efg);
    emit(&g,"  if(u_alpha_enable!=0) {\n"
         "    int a=int(round(fragColor.a*255.0)),ref=int(round(clamp(u_alpha_ref,0.0,1.0)*255.0));\n"
         "    bool pass=u_alpha_func==8 || (u_alpha_func==2 && a<ref) || (u_alpha_func==3 && a==ref) ||\n"
         "      (u_alpha_func==4 && a<=ref) || (u_alpha_func==5 && a>ref) || (u_alpha_func==6 && a!=ref) ||\n"
         "      (u_alpha_func==7 && a>=ref);\n    if(!pass)discard;\n  }\n}\n");
    if(g.failed){source[0]=0;memset(info,0,sizeof(*info));return 0;}
    return 1;
}
