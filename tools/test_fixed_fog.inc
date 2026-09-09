/* Original fixed fog generation on native GPU, with synthetic XYZ vertices. */
static void fixed_fog_pixel(const uint32_t draw[4],float expected)
{
    call(0x1019C0,draw,4);assert(g_eax==0);
    uint8_t p[4];glReadPixels(160,120,1,1,GL_RGBA,GL_UNSIGNED_BYTE,p);
    int red=(int)lroundf(expected*255);
    if(abs(p[0]-red)>2 || abs(p[1]-red)>2 || p[2]!=255 || abs(p[3]-128)>1)
        fprintf(stderr,"fixed fog pixel %u,%u,%u,%u expected %d,%d,255,128\n",p[0],p[1],p[2],p[3],red,red);
    assert(abs(p[0]-red)<=2 && abs(p[1]-red)<=2 && p[2]==255 && abs(p[3]-128)<=1);
}
static void test_fixed_fog(void)
{
    uint32_t target[]={s_backbuffer_handle,0},off[]={0},fvf[]={D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_SPECULAR};
    call(0xFEF20,target,2);call(0xFDAD0,off,1);call(0xFE5C0,off,1);call(0xFE660,off,1);
    test_state(0x304,0);test_state(0x300,0);test_state(0x358,0x1010101);
    for(unsigned i=0;i<4;++i){uint32_t tex[]={i,0};call(0xFFC90,tex,2);}
    call(0x102BB0,off,1);call(0x102940,fvf,1);call(0x1026F0,off,1);
    const unsigned transforms[]={6,0,1};
    float saved[3][16],m[3][16]={{0}};
    for(unsigned i=0;i<3;++i){
        memcpy(saved[i],guest_ptr(GUEST_DEVICE+0x750+transforms[i]*64),64);
        for(unsigned j=0;j<4;++j)m[i][j*5]=1;
    }
    m[2][0]=m[2][5]=.1f;m[2][10]=.01f;
    for(unsigned i=0;i<3;++i)memcpy(guest_ptr(GUEST_DEVICE+0x750+transforms[i]*64),m[i],64);
    struct {float x,y,z;uint32_t diffuse,specular;} v[4]={
        {-3,-4,5,0x80FFFFFF,0x40000000},{3,-4,5,0x80FFFFFF,0x40000000},
        {3,4,5,0x80FFFFFF,0x40000000},{-3,4,5,0x80FFFFFF,0x40000000}};
    uint32_t draw[]={8,4,0x1D800,sizeof(v[0])},color[]={0xFF0000FF};
    memcpy(guest_ptr(draw[2]),v,sizeof(v));call(0xFDA80,color,1);
    write32(0x10EF60,1);write32(0x10EF64,3);write32(0x10EF68,0);write32(0x10EF6C,0x41200000);write32(0x10EF74,0);
    fixed_fog_pixel(draw,.5f); /* Signed planar eye z, not clip z or radial. */
    write32(0x10EF74,1);fixed_fog_pixel(draw,1-sqrtf(50)/10);
    write32(0x10EF64,0);fixed_fog_pixel(draw,64/255.f); /* NONE ignores range. */
    write32(0x10EF74,0);
    float density=.2f;memcpy(guest_ptr(0x10EF70),&density,4);
    write32(0x10EF64,1);fixed_fog_pixel(draw,expf(-1));
    write32(0x10EF64,2);fixed_fog_pixel(draw,expf(-1));
    write32(0x10EF64,3);
    /* World then view both affect distance; projection changes must not. */
    for(unsigned i=0;i<4;++i)v[i].z=0;
    memcpy(guest_ptr(draw[2]),v,sizeof(v));m[0][14]=2;m[1][14]=3;m[2][10]=.1f;
    for(unsigned i=0;i<3;++i)memcpy(guest_ptr(GUEST_DEVICE+0x750+transforms[i]*64),m[i],64);
    fixed_fog_pixel(draw,.5f);
    /* Live physical fog plane, including its offset. */
    s_vertex_constants[57][2]=.5f;s_vertex_constants[57][3]=1;fixed_fog_pixel(draw,.65f);
    s_vertex_constants[57][2]=1;s_vertex_constants[57][3]=0;
    /* Negative eye distance is signed, not abs-planar. Keep geometry clipped in. */
    m[0][14]=-8;m[2][10]=-.1f;
    memcpy(guest_ptr(GUEST_DEVICE+0x750+6*64),m[0],64);memcpy(guest_ptr(GUEST_DEVICE+0x750+64),m[2],64);
    fixed_fog_pixel(draw,1);
    write32(0x10EF60,0);fixed_fog_pixel(draw,1);
    for(unsigned i=0;i<3;++i)memcpy(guest_ptr(GUEST_DEVICE+0x750+transforms[i]*64),saved[i],64);
    assert(glGetError()==GL_NO_ERROR);
    puts("PASS: fixed native fog signed planar/radial/specular-alpha distances, world/view/projection separation, live plane, linear/exp/exp2, disable and unchanged alpha");
}
