/* Retail4361 Begin/End and immediate float attributes. Included after the
 * resource, shader and draw_vertices_data helpers. See IMMEDIATE-DRAWS.md. */
#define IMMEDIATE_VERTEX_LIMIT (256u * 1024u)
typedef struct ImmediateVertex { float attributes[16][4]; } ImmediateVertex;
static float s_immediate_attributes[16][4];
static int s_immediate_initialized, s_immediate_active;
static uint32_t s_immediate_type, s_immediate_fvf, s_immediate_count, s_immediate_capacity;
static ImmediateVertex *s_immediate_vertices;
static HRESULT s_immediate_failure;

static HRESULT immediate_error(const char *reason)
{
    fprintf(stderr,"[wrath graphics] immediate draw rejected: %s (type%u FVF0x%X vertices%u)\n",
            reason,s_immediate_type,s_fvf,s_immediate_count);
    if (s_immediate_active) s_immediate_failure=D3DERR_INVALIDCALL;
    return D3DERR_INVALIDCALL;
}
static void immediate_initialize(void)
{
    if (s_immediate_initialized) return;
    for (unsigned i=0;i<16;++i) s_immediate_attributes[i][3]=1.0f;
    s_immediate_initialized=1;
}
static HRESULT immediate_emit(void)
{
    if (!s_immediate_active) return immediate_error("position outside Begin/End");
    if (s_immediate_count==IMMEDIATE_VERTEX_LIMIT) return immediate_error("64MiB batch limit");
    if (s_immediate_count==s_immediate_capacity) {
        uint32_t capacity=s_immediate_capacity?s_immediate_capacity*2:64;
        ImmediateVertex *vertices=realloc(s_immediate_vertices,(size_t)capacity*sizeof(*vertices));
        if (!vertices) { s_immediate_failure=(HRESULT)0x8007000Eu; return s_immediate_failure; }
        s_immediate_vertices=vertices; s_immediate_capacity=capacity;
    }
    memcpy(s_immediate_vertices[s_immediate_count++].attributes,s_immediate_attributes,sizeof(s_immediate_attributes));
    return 0;
}
void sub_00101EC0(void) /* Begin(primitiveType), ret4. */
{
    uint32_t type=arg(0);
    if (!s_device || s_immediate_active || !s_fvf ||
        (type!=1 && type!=2 && type!=3 && type!=5 && type!=6 && type!=7 && type!=8)) {
        finish(4,(uint32_t)immediate_error("invalid or nested Begin")); return;
    }
    immediate_initialize();
    s_immediate_active=1; s_immediate_type=type; s_immediate_fvf=s_fvf; s_immediate_count=0;
    s_immediate_failure=0;
    write32(GUEST_DEVICE+8,read32(GUEST_DEVICE+8)|0x800);
    finish(4,0);
}
static HRESULT immediate_attribute(uint32_t reg,const float value[4])
{
    if (!s_device || (reg>=16 && reg!=UINT32_MAX)) return immediate_error("attribute register out of range");
    immediate_initialize();
    unsigned slot=reg==UINT32_MAX?0:reg;
    memcpy(s_immediate_attributes[slot],value,4*sizeof(float));
    return slot==0?immediate_emit():0;
}
void sub_00101E60(void) /* SetVertexData4f(reg,x,y,z,w), ret20. */
{
    uint32_t reg=arg(0),bits[4]={arg(1),arg(2),arg(3),arg(4)};
    float value[4]; memcpy(value,bits,sizeof(value));
    finish(20,(uint32_t)immediate_attribute(reg,value));
}
void sub_00101E20(void) /* SetVertexData2f(reg,x,y), ret12. */
{
    uint32_t reg=arg(0),bits[2]={arg(1),arg(2)};
    float value[4]={0,0,0,1}; memcpy(value,bits,sizeof(bits));
    /* Unlike4f, the original2f routine has no special -1 position alias. */
    if (reg>=16) { finish(12,(uint32_t)immediate_error("2f register out of range")); return; }
    finish(12,(uint32_t)immediate_attribute(reg,value));
}
static uint32_t immediate_color(const float value[4])
{
    uint32_t rgba[4];
    for(unsigned i=0;i<4;++i) {
        float v=value[i];
        rgba[i]=!(v>0)?0:v>=1?255:(uint32_t)floorf(v*255.0f+0.5f);
    }
    return (rgba[3]<<24)|(rgba[0]<<16)|(rgba[1]<<8)|rgba[2];
}
static HRESULT immediate_draw_fixed(void)
{
    uint32_t position=s_fvf&0xE, textures=(s_fvf>>8)&15;
    if ((position!=D3DFVF_XYZ && position!=D3DFVF_XYZRHW) || textures>4 || (s_fvf&0x20))
        return immediate_error("unsupported fixed vertex layout");
    unsigned position_bytes=position==D3DFVF_XYZ?12:16;
    unsigned stride=position_bytes+(s_fvf&D3DFVF_NORMAL?12:0)+
        (s_fvf&D3DFVF_DIFFUSE?4:0)+(s_fvf&D3DFVF_SPECULAR?4:0);
    unsigned dimensions[4];
    static const unsigned counts[4]={2,3,4,1};
    for(unsigned i=0;i<textures;++i) { dimensions[i]=counts[(s_fvf>>(16+i*2))&3]; stride+=dimensions[i]*4; }
    uint8_t *packed=malloc((size_t)s_immediate_count*stride);
    if (!packed) return (HRESULT)0x8007000Eu;
    for(uint32_t i=0;i<s_immediate_count;++i) {
        float (*a)[4]=s_immediate_vertices[i].attributes;
        if(position==D3DFVF_XYZ && a[0][3]!=1.0f) {
            free(packed); return immediate_error("XYZ immediate position requires homogeneous w1");
        }
        uint8_t *p=packed+(size_t)i*stride;
        memcpy(p,a[0],position_bytes); p+=position_bytes;
        if(s_fvf&D3DFVF_NORMAL) { memcpy(p,a[2],12); p+=12; }
        if(s_fvf&D3DFVF_DIFFUSE) { uint32_t color=immediate_color(a[3]); memcpy(p,&color,4); p+=4; }
        if(s_fvf&D3DFVF_SPECULAR) { uint32_t color=immediate_color(a[4]); memcpy(p,&color,4); p+=4; }
        for(unsigned t=0;t<textures;++t) { memcpy(p,a[9+t],dimensions[t]*4); p+=dimensions[t]*4; }
    }
    HRESULT result=draw_vertices_data(s_immediate_type,s_immediate_count,packed,stride);
    free(packed); return result;
}
void sub_00101F00(void) /* End(), ret0. */
{
    if (!graphics_thread()) abort();
    HRESULT result=0;
    if (!s_immediate_active) result=immediate_error("End without Begin");
    else if (s_immediate_failure<0) result=s_immediate_failure;
    else if (s_immediate_fvf!=s_fvf) result=immediate_error("vertex format changed inside Begin/End");
    else if (s_immediate_count) {
        if (s_vertex_handle || s_pixel_handle) {
            /* Immediate float attributes bypass stream declaration conversion.
             * Retain all original shader instructions/constants, supplying all
             * sixteen registers as float4 snapshots at each position emit. */
            Nv2aVertexSlot saved[16]; memcpy(saved,s_vertex_object.slots,sizeof(saved));
            for(unsigned i=0;i<16;++i) s_vertex_object.slots[i]=(Nv2aVertexSlot){
                .offset=i*16,.format=0x42,.kind=NV2A_VERTEX_FLOAT32,.components=4,.byte_count=16};
            result=draw_vertices_data(s_immediate_type,s_immediate_count,s_immediate_vertices,sizeof(ImmediateVertex));
            memcpy(s_vertex_object.slots,saved,sizeof(saved));
        } else result=immediate_draw_fixed();
    }
    s_immediate_active=0; s_immediate_count=0;
    free(s_immediate_vertices); s_immediate_vertices=NULL; s_immediate_capacity=0;
    write32(GUEST_DEVICE+8,read32(GUEST_DEVICE+8)&~0x1800u);
    finish(0,(uint32_t)result);
}
