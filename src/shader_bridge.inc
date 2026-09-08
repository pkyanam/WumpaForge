/* Xbox shader SDK bridge, included after graphics resource helpers. All GPU
 * programs execute on the native GPU; all guest CPU code remains AOT ARM64. */
#include "nv2a_vertex.h"
#include "nv2a_vertex_input.h"
#include "nv2a_pixel.h"
extern GLuint xbox_D3D8GLTextureName(IDirect3DTexture8 *texture);
extern void xbox_D3D8GLApplyRenderStates(void);
static uint32_t s_vertex_handle, s_pixel_handle;
static Nv2aVertexObject s_vertex_object;
static Nv2aPixelDef s_pixel_definition;
static float s_vertex_constants[192][4];
static uint32_t s_pixel_constants[16];
static float s_pixel_eye_vector[3];
static int shader_range(uint32_t address, size_t size)
{
    return address >= 0x1000 && address < 0x4000000 && size <= 0x4000000-address;
}
static void shader_error(const char *what, const char *detail)
{
    fprintf(stderr,"[wrath shaders] %s: %s, guest return0x%X after%u swaps\n",what,detail,read32(g_esp),read32(GUEST_SWAP_COUNT));
    abort();
}
static void shader_set_vertex(uint32_t handle)
{
    uint32_t object=handle-1;
    if (!shader_range(object,0x114)) shader_error("vertex object","outside RAM");
    uint32_t packets=read32(object+12);
    size_t size=0x118+(size_t)packets*4;
    char error[256]="object range outside RAM";
    if (packets>4096 || !shader_range(object,size) ||
        !nv2a_vertex_object_decode(guest_ptr(object),size,&s_vertex_object,error,sizeof(error)))
        shader_error("vertex object",packets>4096?"oversized packet stream":error);
    for (unsigned i=0;i<192;++i)
        if (s_vertex_object.constant_mask[i/64]&(UINT64_C(1)<<(i%64)))
            memcpy(s_vertex_constants[i],s_vertex_object.constant_words[i],16);
    s_vertex_handle=handle;
    write32(GUEST_DEVICE+0x380,object); write32(GUEST_DEVICE+0x384,handle);
    write32(GUEST_DEVICE+0x388,0);
    write32(0x10EC10,read32(0x10EC10)|0x1670);
}
void sub_00102AA0(void) /* SetVertexShaderConstant(signedIndex,data,vectorCount), ret12. */
{
    int64_t index=(int32_t)arg(0); uint32_t data=arg(1), count=arg(2);
    index+=96;
    if (index<0 || index>192 || count>192-index || (count&&!shader_range(data,(size_t)count*16)))
        shader_error("vertex constants","invalid index/data/count");
    if (count) {
        memcpy(s_vertex_constants[index],guest_ptr(data),(size_t)count*16);
        if (!(read32(GUEST_DEVICE+8)&0x10))
            memcpy(guest_ptr(GUEST_DEVICE+0xB98+(uint32_t)index*16),guest_ptr(data),(size_t)count*16);
    }
    finish(12,0);
}
static void shader_set_pixel(uint32_t handle)
{
    if (!shader_range(handle,12)) shader_error("pixel object","outside RAM");
    uint32_t definition=read32(handle+8);
    if (!shader_range(definition,sizeof(s_pixel_definition)) ||
        !nv2a_pixel_read_definition(guest_ptr(definition),sizeof(s_pixel_definition),&s_pixel_definition))
        shader_error("pixel definition","invalid layout/range");
    s_pixel_handle=handle;
    write32(GUEST_DEVICE+0x370,handle);
    write32(GUEST_DEVICE+0x374,s_pixel_definition.final_abcd|s_pixel_definition.final_efg);
    write32(GUEST_DEVICE+0x378,s_pixel_definition.final_constants&0x100);
    write32(GUEST_DEVICE+0x37C,s_pixel_definition.texture_modes);
    if (!(read32(GUEST_DEVICE+8)&0x10)) {
        memcpy(guest_ptr(0x10EE18),&s_pixel_definition,57*4);
        write32(0x10EFEC,s_pixel_definition.texture_modes);
    }
    write32(0x10EC10,read32(0x10EC10)|0x4000);
}
void sub_00102D80(void) /* SetPixelShaderConstant(index,floatRGBA,count), ret12. */
{
    uint32_t index=arg(0),data=arg(1),count=arg(2);
    if (!s_pixel_handle || index>16 || count>16-index || (count&&!shader_range(data,(size_t)count*16)))
        shader_error("pixel constants","invalid shader/index/data/count");
    const float *values=guest_ptr(data);
    if (!nv2a_pixel_set_constants(&s_pixel_definition,index,values,count))
        shader_error("pixel constants","unsupported constant update");
    for (unsigned i=0;i<count;++i) {
        uint32_t packed=nv2a_pixel_pack_constant(values+i*4);
        s_pixel_constants[index+i]=packed;
        write32(GUEST_DEVICE+0x3EC+(index+i)*4,packed);
    }
    if (!index&&count) memcpy(s_pixel_eye_vector,values,sizeof(s_pixel_eye_vector));
    /* Preserve live render-state constants even on pure devices: original
     * SetPixelShaderConstant calls SetRenderState for each mapped register. */
    for (unsigned stage=0;stage<8;++stage) {
        unsigned c0=(s_pixel_definition.c0_mapping>>(stage*4))&15;
        unsigned c1=(s_pixel_definition.c1_mapping>>(stage*4))&15;
        if (c0>=index&&c0<index+count) write32(0x10EE18+(10+stage)*4,s_pixel_definition.constant0[stage]);
        if (c1>=index&&c1<index+count) write32(0x10EE18+(18+stage)*4,s_pixel_definition.constant1[stage]);
    }
    unsigned c0=s_pixel_definition.final_constants&15,c1=(s_pixel_definition.final_constants>>4)&15;
    if (c0>=index&&c0<index+count) write32(0x10EE18+43*4,s_pixel_definition.final_constant0);
    if (c1>=index&&c1<index+count) write32(0x10EE18+44*4,s_pixel_definition.final_constant1);
    finish(12,0);
}
#define SHADER_CACHE_LIMIT 64
struct ShaderKey {
    uint32_t count, words[136][4];
    Nv2aPixelDef pixel;
    Nv2aPixelOptions options;
};
static struct ShaderProgram {
    struct ShaderKey key;
    GLuint program;
    uint64_t age;
    Nv2aVertexInfo vertex;
    Nv2aPixelInfo pixel;
} s_shader_programs[SHADER_CACHE_LIMIT];
static uint64_t s_shader_age;
static GLuint s_shader_vao, s_shader_vbo, s_shader_samplers[4];
static GLuint shader_compile(GLenum type, const char *source)
{
    GLuint shader=glCreateShader(type);
    glShaderSource(shader,1,&source,NULL); glCompileShader(shader);
    GLint success; glGetShaderiv(shader,GL_COMPILE_STATUS,&success);
    if (!success) {
        char error[2048]; glGetShaderInfoLog(shader,sizeof(error),NULL,error);
        glDeleteShader(shader); shader_error("native compile",error);
    }
    return shader;
}
static struct ShaderProgram *shader_program(const Nv2aPixelDef *pixel)
{
    if (!s_vertex_handle)
        shader_error("shader pairing","fixed vertex/programmed pixel stages not yet implemented");
    struct ShaderKey key={0};
    key.count=s_vertex_object.instruction_count;
    memcpy(key.words,s_vertex_object.words,key.count*16);
    key.pixel=*pixel;
    memset(key.pixel.constant0,0,sizeof(key.pixel.constant0));
    memset(key.pixel.constant1,0,sizeof(key.pixel.constant1));
    key.pixel.final_constant0=key.pixel.final_constant1=0;
    key.options.fog_enabled=read32(0x10EF60)!=0;
    key.options.specular_enabled=read32(0x10EF8C)!=0;
    if (key.options.fog_enabled) shader_error("fog","native fog parameters not yet integrated");
    for (unsigned i=0;i<4;++i) {
        struct Resource *r=resource(s_texture_handles[i]);
        if (r&&r->linear&&((key.pixel.texture_modes>>(i*5))&31)==1) key.options.rectangle_texture_mask|=1u<<i;
    }
    struct ShaderProgram *slot=&s_shader_programs[0];
    for (unsigned i=0;i<SHADER_CACHE_LIMIT;++i) {
        struct ShaderProgram *candidate=&s_shader_programs[i];
        if (candidate->program&&!memcmp(&candidate->key,&key,sizeof(key))) {
            candidate->age=++s_shader_age; return candidate;
        }
        if (!candidate->program || (slot->program&&candidate->age<slot->age)) slot=candidate;
    }
    static char vertex_source[65536], pixel_source[65536];
    char error[256]; Nv2aVertexInfo vi; Nv2aPixelInfo pi;
    if (!nv2a_vertex_generate(&key.words[0][0],key.count,vertex_source,sizeof(vertex_source),&vi,error,sizeof(error)))
        shader_error("vertex translation",error);
    if (!nv2a_pixel_generate(pixel,&key.options,pixel_source,sizeof(pixel_source),&pi,error,sizeof(error)))
        shader_error("pixel translation",error);
    GLuint vs=shader_compile(GL_VERTEX_SHADER,vertex_source), ps=shader_compile(GL_FRAGMENT_SHADER,pixel_source);
    GLuint program=glCreateProgram(); glAttachShader(program,vs); glAttachShader(program,ps);
    glLinkProgram(program); glDeleteShader(vs); glDeleteShader(ps);
    GLint success; glGetProgramiv(program,GL_LINK_STATUS,&success);
    if (!success) {
        char info[2048]; glGetProgramInfoLog(program,sizeof(info),NULL,info);
        glDeleteProgram(program); shader_error("native program link",info);
    }
    if (slot->program) glDeleteProgram(slot->program);
    slot->key=key; slot->program=program; slot->vertex=vi; slot->pixel=pi; slot->age=++s_shader_age;
    fprintf(stderr,"[wrath shaders] linked native program%u (%u vertex instructions, %u combiners)\n",program,vi.instruction_count,pi.stages);
    return slot;
}
static void shader_rgba(uint32_t color,float out[4])
{
    out[0]=((color>>16)&255)/255.0f; out[1]=((color>>8)&255)/255.0f;
    out[2]=(color&255)/255.0f; out[3]=(color>>24)/255.0f;
}
static GLenum shader_wrap(uint32_t mode)
{
    switch(mode) {
    case 1:return GL_REPEAT; case 2:return GL_MIRRORED_REPEAT;
    case 3:return GL_CLAMP_TO_EDGE; case 4:return GL_CLAMP_TO_BORDER;
    default:shader_error("texture addressing","unsupported mode"); return GL_REPEAT;
    }
}
static HRESULT shader_draw(GLenum primitive, uint32_t count, const void *vertices, uint32_t stride)
{
    Nv2aPixelDef pixel=s_pixel_definition;
    if (!s_pixel_handle) {
        uint32_t stages[4][32]; unsigned dimensions[4]; char error[256];
        for (unsigned i=0;i<4;++i) {
            memcpy(stages[i],guest_ptr(0x10EC18+i*128),sizeof(stages[i]));
            struct Resource *r=resource(s_texture_handles[i]);
            dimensions[i]=!r?0:r->type==RESOURCE_CUBE_TEXTURE?4:2;
        }
        if (!nv2a_pixel_fixed_definition(stages,dimensions,read32(0x10F01C),&pixel,error,sizeof(error)))
            shader_error("fixed pixel stages",error);
    }
    struct ShaderProgram *p=shader_program(&pixel);
    if (!s_shader_vao) { glGenVertexArrays(1,&s_shader_vao); glGenBuffers(1,&s_shader_vbo); glGenSamplers(4,s_shader_samplers); }
    glBindVertexArray(s_shader_vao); glBindBuffer(GL_ARRAY_BUFFER,s_shader_vbo);
    glBufferData(GL_ARRAY_BUFFER,(size_t)count*stride,vertices,GL_STREAM_DRAW);
    for (unsigned i=0;i<16;++i) {
        glDisableVertexAttribArray(i); glVertexAttrib4f(i,0,0,0,1);
        if (!(p->vertex.input_mask&(1u<<i))) continue;
        Nv2aVertexSlot *slot=&s_vertex_object.slots[i];
        if (slot->stream || slot->offset>stride || slot->byte_count>stride-slot->offset || slot->tessellation_type)
            shader_error("vertex stream","unsupported stream/stride/tessellation");
        GLenum type; GLint components=slot->components; GLboolean normalize=slot->normalized;
        switch(slot->kind) {
        case NV2A_VERTEX_FLOAT32:type=GL_FLOAT;break;
        case NV2A_VERTEX_BGRA8:type=GL_UNSIGNED_BYTE;components=GL_BGRA;break;
        case NV2A_VERTEX_SINT16:type=GL_SHORT;break;
        case NV2A_VERTEX_UNORM8:type=GL_UNSIGNED_BYTE;break;
        case NV2A_VERTEX_NONE:continue;
        default:shader_error("vertex format","packed/float2h conversion not yet integrated");return D3DERR_INVALIDCALL;
        }
        glEnableVertexAttribArray(i);
        glVertexAttribPointer(i,components,type,normalize,stride,(const void *)(uintptr_t)slot->offset);
    }
    glUseProgram(p->program);
    glUniform4fv(glGetUniformLocation(p->program,"u_vconstants"),192,&s_vertex_constants[0][0]);
    glUniform4f(glGetUniformLocation(p->program,"u_nv2a_viewport"),s_viewport.X,s_viewport.Y,s_viewport.Width,s_viewport.Height);
    glUniform2f(glGetUniformLocation(p->program,"u_nv2a_depth"),s_viewport.MinZ*16777215.0f,s_viewport.MaxZ*16777215.0f);
    float colors[18][4];
    for (unsigned i=0;i<8;++i) { shader_rgba(pixel.constant0[i],colors[i*2]); shader_rgba(pixel.constant1[i],colors[i*2+1]); }
    shader_rgba(pixel.final_constant0,colors[16]); shader_rgba(pixel.final_constant1,colors[17]);
    glUniform4fv(glGetUniformLocation(p->program,"u_psconstants"),18,&colors[0][0]);
    glUniform3f(glGetUniformLocation(p->program,"u_fogcolor"),0,0,0);
    glUniform1i(glGetUniformLocation(p->program,"u_alpha_enable"),read32(0x10EE18+60*4)!=0);
    glUniform1i(glGetUniformLocation(p->program,"u_alpha_func"),(int)read32(0x10EE18+58*4)-GL_NEVER+1);
    glUniform1f(glGetUniformLocation(p->program,"u_alpha_ref"),(read32(0x10EE18+61*4)&255)/255.0f);
    GLint previous_samplers[4];
    for (unsigned i=0;i<4;++i) { glActiveTexture(GL_TEXTURE0+i); glGetIntegerv(GL_SAMPLER_BINDING,&previous_samplers[i]); }
    for (unsigned i=0;i<4;++i) {
        if (!(p->pixel.texture_mask&(1u<<i))) continue;
        struct Resource *r=resource(s_texture_handles[i]);
        if (!r) shader_error("texture","shader samples unbound texture");
        GLenum target=r->type==RESOURCE_CUBE_TEXTURE?GL_TEXTURE_CUBE_MAP:GL_TEXTURE_2D;
        if (p->pixel.sampler_dimension[i]==3 || ((p->pixel.sampler_dimension[i]==4)!=(target==GL_TEXTURE_CUBE_MAP)))
            shader_error("texture","sampler dimension mismatch");
        HRESULT upload=r->type==RESOURCE_CUBE_TEXTURE?upload_cube_texture(r):upload_texture_stage(r,i);
        if (upload<0) shader_error("texture","native upload failed");
        glActiveTexture(GL_TEXTURE0+i);
        glBindTexture(target,r->type==RESOURCE_CUBE_TEXTURE?r->cube_gl:texture_gl_name(r,i));
        uint32_t state=0x10EC18+i*128;
        uint32_t mag=read32(state+12),min=read32(state+16),mip=read32(state+20);
        if (mag<1||mag>2||min<1||min>2||mip>2) shader_error("texture filtering","unsupported filter");
        GLenum min_filter=min==1?GL_NEAREST:GL_LINEAR;
        if (r->levels>1&&mip) min_filter=mip==1?(min==1?GL_NEAREST_MIPMAP_NEAREST:GL_LINEAR_MIPMAP_NEAREST):(min==1?GL_NEAREST_MIPMAP_LINEAR:GL_LINEAR_MIPMAP_LINEAR);
        glSamplerParameteri(s_shader_samplers[i],GL_TEXTURE_MIN_FILTER,min_filter);
        glSamplerParameteri(s_shader_samplers[i],GL_TEXTURE_MAG_FILTER,mag==1?GL_NEAREST:GL_LINEAR);
        glSamplerParameteri(s_shader_samplers[i],GL_TEXTURE_WRAP_S,shader_wrap(read32(state)));
        glSamplerParameteri(s_shader_samplers[i],GL_TEXTURE_WRAP_T,shader_wrap(read32(state+4)));
        float border[4]; shader_rgba(read32(state+0x74),border);
        glSamplerParameterfv(s_shader_samplers[i],GL_TEXTURE_BORDER_COLOR,border);
        glBindSampler(i,s_shader_samplers[i]);
        char sampler[8]; snprintf(sampler,sizeof(sampler),"tex%u",i);
        glUniform1i(glGetUniformLocation(p->program,sampler),i);
    }
    glActiveTexture(GL_TEXTURE0);
    xbox_D3D8GLApplyRenderStates();
    glDrawArrays(primitive,0,count);
    for (unsigned i=0;i<4;++i) glBindSampler(i,previous_samplers[i]);
    GLenum error=glGetError(); if(error) { char msg[40];snprintf(msg,sizeof(msg),"GL error0x%X",error);shader_error("draw",msg); }
    return 0;
}
