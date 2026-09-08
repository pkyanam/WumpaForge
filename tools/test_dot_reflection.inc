/* Actual mode chain, synthetic basis/normal map/cube and native GPU pixels. */
static void test_dot_reflection(void)
{
    uint32_t off[]={0},target[]={s_backbuffer_handle,0};call(0xFEF20,target,2);
    call(0xFDAD0,off,1);call(0xFE5C0,off,1);call(0xFE660,off,1);
    test_state(0x304,0);test_state(0x300,0);test_state(0x358,0x1010101);
    write32(0x10EF60,0);write32(0x10EF8C,0);
    uint32_t create[]={1,1,1,0,6,0,0x1C000};call(0xFE9C0,create,7);assert(g_eax==0);
    uint32_t normal=read32(0x1C000);write32(read32(normal+4),0xFF8080FF);
    uint32_t cube_create[]={4,1,0,6,0,0x1C004};call(0xFE9F0,cube_create,6);assert(g_eax==0);
    uint32_t cube=read32(0x1C004);struct Resource *r=resource(cube);
    const uint32_t colors[]={0xFFFF0000,0xFF00FF00,0xFF0000FF,0xFFFFFF00,0xFFFF00FF,0xFF00FFFF};
    for(unsigned face=0;face<6;++face)for(unsigned i=0;i<16;++i)write32(r->data+face*r->face_stride+i*4,colors[face]);
    for(unsigned stage=0;stage<4;++stage){
        uint32_t bind[]={stage,stage==0?normal:stage==3?cube:0};call(0xFFC90,bind,2);assert(g_eax==0);
        uint32_t state=0x10EC18+stage*128;write32(state,3);write32(state+4,3);
        write32(state+12,1);write32(state+16,1);write32(state+20,0);
    }
    const uint32_t object=0x1C100;memset(guest_ptr(object),0,0x200);
    write32(object,1);write32(object+4,0x10);write32(object+8,6);write32(object+12,25);
    for(unsigned i=0;i<16;++i)write32(object+0x1c+i*16,2);
    write32(object+0x1c,0x42);write32(object+0x1c+3*16,0x22);write32(object+0x18+3*16,16);
    uint32_t words[6][4]={
        {0,(1u<<21)|0x1b,2u<<26,(15u<<12)|(1u<<11)},
        {0,(1u<<21)|0x1b,3u<<26,(15u<<12)|(1u<<11)|(3u<<3)},
        {0,(1u<<21)|(3u<<9)|0x1b,2u<<26,(15u<<12)|(1u<<11)|(9u<<3)},
        {0,(1u<<21)|(1u<<13)|0x1b,3u<<26,(15u<<12)|(1u<<11)|(10u<<3)},
        {0,(1u<<21)|(2u<<13)|0x1b,3u<<26,(15u<<12)|(1u<<11)|(11u<<3)},
        {0,(1u<<21)|(3u<<13)|0x1b,3u<<26,(15u<<12)|(1u<<11)|(12u<<3)|1}
    };
    write32(object+0x114,(24u<<18)|0xB00);memcpy(guest_ptr(object+0x118),words,sizeof(words));
    uint32_t vs[]={object+1};call(0x102940,vs,1);
    float constants[4][4]={{.5f,.75f,1,.5f},{1,0,0,0},{0,1,0,0},{0,0,1,0}};
    Nv2aPixelDef definition={0};definition.texture_modes=0x64621;definition.dot_mapping=0x111;
    definition.combiner_count=0x11101;definition.rgb_inputs[0]=0xC4CB0000;definition.alpha_inputs[0]=0xD4DB1010;
    definition.rgb_outputs[0]=definition.alpha_outputs[0]=0xC0;definition.final_constants=0x1ff;
    write32(0x1C408,0x1C40C);memcpy(guest_ptr(0x1C40C),&definition,sizeof(definition));
    uint32_t ps[]={0x1C400};call(0x102BB0,ps,1);
    const float vertices[4][6]={{40,40,0,1,.5f,.5f},{80,40,0,1,.5f,.5f},{80,80,0,1,.5f,.5f},{40,80,0,1,.5f,.5f}};
    memcpy(guest_ptr(0x1C600),vertices,sizeof(vertices));uint32_t draw[]={8,4,0x1C600,24};
    const float eyes[8][3]={{-1,0,0},{1,0,0},{0,-1,0},{0,1,0},{0,0,1},{0,0,-1},{2,.5f,1},{0,0,1}};
    const unsigned faces[]={0,1,2,3,4,5,1,4};
    for(unsigned c=0;c<8;++c){
        for(unsigned row=0;row<3;++row){constants[row+1][3]=eyes[c][row];constants[row+1][row]=c==6?(float)(row+2):c==7&&row<2?256:1;}
        memcpy(guest_ptr(0x1C700),constants,sizeof(constants));uint32_t vc[]={(uint32_t)-96,0x1C700,4};call(0x102AA0,vc,3);
        call(0x1019C0,draw,4);assert(g_eax==0);
        uint8_t pixel[4];glReadPixels(60,180,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
        uint32_t color=colors[faces[c]];int expected[]={((color>>16)&255)?128:0,((color>>8)&255)?191:0,(color&255)?255:0,128};
        for(unsigned channel=0;channel<4;++channel){
            if(abs(pixel[channel]-expected[channel])>1)fprintf(stderr,"reflection case%u channel%u got%u expected%d\n",c,channel,pixel[channel],expected[channel]);
            assert(abs(pixel[channel]-expected[channel])<=1);
        }
    }
    static char source[65536];char error[256];Nv2aPixelInfo info;
    assert(nv2a_pixel_generate(&definition,&(Nv2aPixelOptions){0},source,sizeof(source),&info,error,sizeof(error)));
    assert(info.texture_mask==9 && info.sampler_dimension[3]==4);
    definition.dot_mapping=0x121;assert(!nv2a_pixel_generate(&definition,&(Nv2aPixelOptions){0},source,sizeof(source),&info,error,sizeof(error)));
    definition.dot_mapping=0x111;definition.input_texture=2u<<16;assert(!nv2a_pixel_generate(&definition,&(Nv2aPixelOptions){0},source,sizeof(source),&info,error,sizeof(error)));
    definition.input_texture=0;definition.rgb_inputs[0]=0x09200000;assert(!nv2a_pixel_generate(&definition,&(Nv2aPixelOptions){0},source,sizeof(source),&info,error,sizeof(error)));
    for(unsigned stage=0;stage<4;++stage){uint32_t unbind[]={stage,0};call(0xFFC90,unbind,2);}
    uint32_t release[]={normal};call(0x103AD0,release,1);release[0]=cube;call(0x103AD0,release,1);
    assert(glGetError()==GL_NO_ERROR);
    puts("PASS: actual dot/dot/specular-reflection chain, six cube faces, nonunit normal denominator, exact128 signed center, diffuse RGBA modulation, invalid mapping/dependency rejection");
}
