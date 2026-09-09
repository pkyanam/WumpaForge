/* Original102850's three XYZRHW shader distance paths, no game bytes. */
static void test_transformed_fog(void)
{
    uint32_t target[]={s_backbuffer_handle,0},off[]={0};call(0xFEF20,target,2);
    call(0xFDAD0,off,1);call(0xFE5C0,off,1);call(0xFE660,off,1);
    test_state(0x304,0);test_state(0x300,0);test_state(0x358,0x1010101);
    for(unsigned i=0;i<4;++i){uint32_t t[]={i,0};call(0xFFC90,t,2);}
    call(0x102BB0,off,1);
    uint32_t fvf[]={D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_SPECULAR|D3DFVF_TEX1};call(0x102940,fvf,1);
    struct {float x,y,z,rhw;uint32_t diffuse,specular;float u,v;} v[4]={
        {120,80,.25f,.2f,0x80FFFFFF,0x40000000,0,0},
        {200,80,.25f,.2f,0x80FFFFFF,0x40000000,1,0},
        {200,160,.25f,.2f,0x80FFFFFF,0x40000000,1,1},
        {120,160,.25f,.2f,0x80FFFFFF,0x40000000,0,1}};
    uint32_t draw[]={8,4,0x1D800,sizeof(v[0])},color[]={0xFF0000FF};call(0xFDA80,color,1);
    uint32_t saved_flags=read32(GUEST_DEVICE+8);
    write32(0x10EF60,1);write32(0x10EF64,3);write32(0x10EF68,0);write32(0x10EF6C,0x41200000);
    write32(GUEST_DEVICE+8,saved_flags&~2u);
    memcpy(guest_ptr(draw[2]),v,sizeof(v));fixed_fog_pixel(draw,.5f); /* 1/.2=5 */
    write32(0x10EF74,1);fixed_fog_pixel(draw,.5f); /* Range flag cannot alter SDK program. */
    write32(GUEST_DEVICE+8,saved_flags|2u);fixed_fog_pixel(draw,.975f); /* Original Z=.25, NOT clip Z. */
    write32(0x10EF64,0);fixed_fog_pixel(draw,64/255.f);
    /* Missing specular declaration supplies actual default alpha1. */
    fvf[0]=D3DFVF_XYZRHW|D3DFVF_DIFFUSE;call(0x102940,fvf,1);fixed_fog_pixel(draw,1);
    fvf[0]=D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_SPECULAR|D3DFVF_TEX1;call(0x102940,fvf,1);
    write32(GUEST_DEVICE+8,saved_flags&~2u);
    float density=.2f;memcpy(guest_ptr(0x10EF70),&density,4);
    write32(0x10EF64,1);fixed_fog_pixel(draw,expf(-1));
    write32(0x10EF64,2);fixed_fog_pixel(draw,expf(-1));
    /* Preserve original zero RHW even though existing clip conversion uses a
     * finite fallback: native RCP yields Inf and original fog special rule1. */
    write32(0x10EF64,3);for(unsigned i=0;i<4;++i)v[i].rhw=0;
    memcpy(guest_ptr(draw[2]),v,sizeof(v));fixed_fog_pixel(draw,1);
    /* Original factors2 and-7 must interpolate before clamp, using actual
     * perspective weights. Clamping at vertices would incorrectly darken RGB. */
    v[0].rhw=v[3].rhw=1;v[1].rhw=v[2].rhw=.1f;
    write32(0x10EF68,0x40000000);write32(0x10EF6C,0x40400000);
    memcpy(guest_ptr(draw[2]),v,sizeof(v));fixed_fog_pixel(draw,1);
    write32(0x10EF60,0);fixed_fog_pixel(draw,1);
    write32(0x10EF74,0);write32(GUEST_DEVICE+8,saved_flags);
    assert(glGetError()==GL_NO_ERROR);
    puts("PASS: transformed SDK fog reciprocalRHW/originalZ/specular-alpha, range independence, no-specular default, EXP/EXP2, zeroRHW/Inf, perspective interpolation before clamp, alpha and disable");
}
