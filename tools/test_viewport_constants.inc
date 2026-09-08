/* Included by graphics smoke after call()/test_state(); run after shader tests. */
static void test_viewport_constants(void)
{
    uint32_t target[]={s_backbuffer_handle,s_depthbuffer_handle},off[]={0},on[]={1};
    call(0xFEF20,target,2); assert(g_eax==0);
    assert(s_vertex_constants[58][0]==160 && s_vertex_constants[58][1]==-120);
    assert(s_vertex_constants[59][0]==160.53125f && s_vertex_constants[59][1]==120.53125f);
    assert(s_vertex_constants[58][2]==16777215 && s_vertex_constants[58][3]==0 && s_vertex_constants[59][3]==0);
    D3DVIEWPORT8 vp={20,30,200,120,.25f,.75f};
    memcpy(guest_ptr(0x18000),&vp,sizeof(vp));uint32_t setvp[]={0x18000};call(0xFF860,setvp,1);
    assert(g_eax==0 && s_vertex_constants[58][0]==100 && s_vertex_constants[58][1]==-60);
    assert(s_vertex_constants[58][2]==8388607.5f && s_vertex_constants[59][2]==4194303.75f);
    assert(s_vertex_constants[59][0]==120.53125f && s_vertex_constants[59][1]==90.53125f);
    uint32_t object=0x18200;memset(guest_ptr(object),0,0x180);
    write32(object,1);write32(object+4,0x10);write32(object+8,3);write32(object+12,13);
    for(unsigned i=0;i<16;i++)write32(object+0x1c+i*16,2);
    write32(object+0x1c,0x42);write32(object+0x114,(12u<<18)|0xB00);
    /* MOV oPos,v0 followed by the reached MUL/RCC and MAD epilogue. */
    uint32_t words[][4]={
        {0,(1u<<21)|0x1b,2u<<26,(15u<<12)|(1u<<11)},
        {0,(2u<<21)|(3u<<25)|(58u<<13)|0x1b,
            (12u<<28)|(1u<<26)|(0x1bu<<17)|(3u<<11)|(0xffu<<2)|3,
            (1u<<28)|(8u<<16)|(14u<<12)|(1u<<11)},
        {0,(4u<<21)|(59u<<13)|0x1b,
            (12u<<28)|(1u<<26)|(1u<<13)|(1u<<11)|(0x1bu<<2),
            (3u<<28)|(14u<<12)|(1u<<11)|1}
    };
    memcpy(guest_ptr(object+0x118),words,sizeof(words));
    uint32_t vs[]={object+1};call(0x102940,vs,1);assert(g_eax==0);
    call(0x102BB0,off,1);call(0xFDAD0,off,1);call(0xFE5C0,on,1);call(0xFE660,off,1);
    test_state(0x304,0);test_state(0x300,0);test_state(0x358,0x1010101);test_state(0x35C,1);
    uint32_t stages[4][32]={{0}};
    for(unsigned i=0;i<4;i++)stages[i][12]=stages[i][16]=stages[i][20]=1;
    stages[0][12]=stages[0][16]=2;stages[0][14]=stages[0][18]=3;
    memcpy(guest_ptr(0x10EC18),stages,sizeof(stages));write32(0x10F01C,0xff20c040);
    const float vertices[4][4]={{-.8f,.8f,.5f,2},{.8f,.8f,.5f,2},{.8f,-.8f,.5f,2},{-.8f,-.8f,.5f,2}};
    memcpy(guest_ptr(0x18400),vertices,sizeof(vertices));
    uint32_t clear[]={0,0,0xf3,0,0x3f800000,0};call(0x100EA0,clear,6);
    uint32_t draw[]={8,4,0x18400,16};call(0x1019C0,draw,4);assert(g_eax==0);
    uint8_t pixel[4];float depth;
    glReadPixels(120,240-90,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
    assert(pixel[0]==0x20 && pixel[1]==0xc0 && pixel[2]==0x40);
    glReadPixels(120,240-90,1,1,GL_DEPTH_COMPONENT,GL_FLOAT,&depth);assert(fabsf(depth-.375f)<.00001f);
    glReadPixels(25,240-35,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);assert(pixel[0]==0 && pixel[1]==0 && pixel[2]==0);
    /* Explicit constant writes persist until the SDK next writes its alias. */
    float explicit_scale[4]={1,2,3,4};memcpy(guest_ptr(0x18500),explicit_scale,sizeof(explicit_scale));
    uint32_t constants[]={(uint32_t)-38,0x18500,1};call(0x102AA0,constants,3);
    assert(!memcmp(s_vertex_constants[58],explicit_scale,sizeof(explicit_scale)));
    call(0x102940,vs,1);assert(s_vertex_constants[58][0]==100 && s_vertex_constants[58][3]==0);
    uint32_t create[]={64,32,1,0,6,0,0x18520};call(0xFE9C0,create,7);assert(g_eax==0);
    uint32_t texture=read32(0x18520),surface_args[]={texture,0,0x18524};call(0x103C30,surface_args,3);assert(g_eax==0);
    uint32_t surface=read32(0x18524),rt[]={surface,0};call(0xFEF20,rt,2);assert(g_eax==0);
    assert(s_vertex_constants[58][0]==32 && s_vertex_constants[58][1]==-16 && s_vertex_constants[58][2]==16777215);
    assert(s_vertex_constants[59][0]==32.53125f && s_vertex_constants[59][1]==16.53125f);
    call(0xFEF20,target,2);assert(s_vertex_constants[58][0]==160 && s_vertex_constants[58][1]==-120);
    uint32_t release[]={surface};call(0x103AD0,release,1);release[0]=texture;call(0x103AD0,release,1);
    assert(glGetError()==GL_NO_ERROR);
    puts("PASS: reserved viewport alias, original MUL/RCC/MAD GPU epilogue, moved viewport/depth/W, explicit-write ordering and texture-target reset");
}
