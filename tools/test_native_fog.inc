/* Original SDK coefficients + translated oFog on native GPU; synthetic data. */
static void test_native_fog(void)
{
    uint32_t target[]={s_backbuffer_handle,0},off[]={0};call(0xFEF20,target,2);
    call(0xFDAD0,off,1);call(0xFE5C0,off,1);call(0xFE660,off,1);
    test_state(0x304,0);test_state(0x300,0);test_state(0x358,0x1010101);
    write32(0x10EF8C,0);
    const uint32_t object=0x1B000;
    memset(guest_ptr(object),0,0x180);write32(object,1);write32(object+4,0x10);
    write32(object+8,3);write32(object+12,13);
    for(unsigned i=0;i<16;++i)write32(object+0x1c+i*16,2);
    write32(object+0x1c,0x42);write32(object+0x2c,0x12);write32(object+0x28,16);
    const uint32_t words[][4]={
        {0,(1u<<21)|0x1b,2u<<26,(15u<<12)|(1u<<11)},
        {0,(1u<<21)|0x1b,3u<<26,(15u<<12)|(1u<<11)|(3u<<3)},
        {0,(1u<<21)|(1u<<9),2u<<26,(8u<<12)|(1u<<11)|(5u<<3)|1}
    };
    write32(object+0x114,(12u<<18)|0xB00);memcpy(guest_ptr(object+0x118),words,sizeof(words));
    uint32_t shader[]={object+1};call(0x102940,shader,1);
    const float white[]={1,1,1,.5f};memcpy(guest_ptr(0x1B200),white,16);
    uint32_t vc[]={(uint32_t)-96,0x1B200,1};call(0x102AA0,vc,3);
    Nv2aPixelDef definition={0};definition.combiner_count=1;
    definition.rgb_inputs[0]=0x04200000;definition.alpha_inputs[0]=0x14200000;
    definition.rgb_outputs[0]=definition.alpha_outputs[0]=0xC0;
    write32(0x1B308,0x1B30C);memcpy(guest_ptr(0x1B30C),&definition,sizeof(definition));
    uint32_t ps[]={0x1B300};call(0x102BB0,ps,1);
    uint32_t color[]={0xFF0000FF};call(0xFDA80,color,1);
    float vertices[4][5]={{40,40,0,1,0},{80,40,0,1,0},{80,80,0,1,0},{40,80,0,1,0}};
    uint32_t draw[]={8,4,0x1B500,20};
    write32(0x10EF60,1);write32(0x10EF64,3);write32(0x10EF68,0);write32(0x10EF6C,0x41200000);
    struct {uint32_t mode;float distance,density,factor;} cases[]={
        {3,0,0,1},{3,5,0,.5f},{3,10,0,0},{3,-10,0,1},{3,20,0,0},
        {0,.25f,0,.25f},{1,5,.2f,.36787944f},{2,5,.2f,.36787944f},
        {1,10,.2f,.13533528f},{2,10,.2f,.01831564f},
        {3,INFINITY,0,1},{1,INFINITY,.2f,1},{2,INFINITY,.2f,0},
        {3,NAN,0,1},{2,NAN,.2f,0}
    };
    for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);++i){
        write32(0x10EF64,cases[i].mode);memcpy(guest_ptr(0x10EF70),&cases[i].density,4);
        for(unsigned j=0;j<4;++j)vertices[j][4]=cases[i].distance;
        memcpy(guest_ptr(draw[2]),vertices,sizeof(vertices));call(0x1019C0,draw,4);assert(g_eax==0);
        uint8_t pixel[4];glReadPixels(60,180,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
        int expected=(int)lroundf(cases[i].factor*255);
        if(abs(pixel[0]-expected)>1 || abs(pixel[1]-expected)>1 || pixel[2]!=255 || abs(pixel[3]-128)>1)
            fprintf(stderr,"fog case%u mode%u distance%g pixel=%u,%u,%u,%u expected%d\n",i,cases[i].mode,cases[i].distance,pixel[0],pixel[1],pixel[2],pixel[3],expected);
        assert(abs(pixel[0]-expected)<=1 && abs(pixel[1]-expected)<=1 && pixel[2]==255 && abs(pixel[3]-128)<=1);
    }
    /* Factors4 and-1 interpolate to >1 at this fragment: vertex clamping
     * would incorrectly fog this white pixel. */
    write32(0x10EF64,3);vertices[0][4]=vertices[3][4]=-30;vertices[1][4]=vertices[2][4]=20;
    memcpy(guest_ptr(draw[2]),vertices,sizeof(vertices));call(0x1019C0,draw,4);
    uint8_t pixel[4];glReadPixels(60,180,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
    assert(pixel[0]==255 && pixel[1]==255 && pixel[2]==255);
    float parameters[2];write32(0x10EF68,0x41200000);assert(shader_fog_parameters(parameters)==1);
    assert(parameters[0]==81921 && parameters[1]==-8192); /* Equal endpoints. */
    write32(0x10EF68,0);write32(0x10EF6C,0x41A00000);
    for(unsigned j=0;j<4;++j)vertices[j][4]=5;
    memcpy(guest_ptr(draw[2]),vertices,sizeof(vertices));call(0x1019C0,draw,4);
    glReadPixels(60,180,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);assert(abs(pixel[0]-191)<=1);
    write32(0x10EF60,0);call(0x1019C0,draw,4);
    glReadPixels(60,180,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);assert(pixel[0]==255 && pixel[1]==255);
    assert(glGetError()==GL_NO_ERROR);
    puts("PASS: native fog modes0..3, original linear/exp coefficients, live parameters, interpolation before clamp, Inf/NaN rules, alpha preservation and disable");
}
