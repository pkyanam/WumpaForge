/* Actual mode transitions and shared fixed-function aliases on native GPU. */
static void test_shader_constant_mode(void)
{
    const float defaults[3][4]={{0,.5f,1,2},{-1,0,1,2},{0,0,-1,0}};
    const float identity[4][4]={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
    /* The production XBE contains these original SDK tables; smoke RAM does not. */
    memcpy(guest_ptr(0x10B208),defaults,sizeof(defaults));memcpy(guest_ptr(0x10B1B8),identity,sizeof(identity));
    uint32_t target[]={s_backbuffer_handle,0},off[]={0},mode[]={1};call(0xFEF20,target,2);
    call(0xFDAD0,off,1);call(0xFE5C0,off,1);call(0xFE660,off,1);
    test_state(0x304,0);test_state(0x300,0);test_state(0x358,0x1010101);
    write32(0x10EF60,0);
    uint32_t stages[4][32]={{0}};
    for(unsigned i=0;i<4;++i)stages[i][12]=stages[i][16]=stages[i][20]=1;
    stages[0][12]=stages[0][16]=2;memcpy(guest_ptr(0x10EC18),stages,sizeof(stages));
    float before[192][4];memcpy(before,s_vertex_constants,sizeof(before));
    write32(0x10EC10,0x123);write32(GUEST_DEVICE+8,read32(GUEST_DEVICE+8)|0x200);
    call(0x1026F0,mode,1);assert(g_eax==0 && read32(GUEST_DEVICE+0x2018)==1);
    assert(!(read32(GUEST_DEVICE+8)&0x200) && read32(0x10EC10)==0x123);
    assert(!memcmp(before,s_vertex_constants,sizeof(before))); /* Mode1 preserves every vector. */
    const float lower[4]={.25f,.5f,.75f,1};memcpy(guest_ptr(0x18B00),lower,sizeof(lower));
    uint32_t constants[]={(uint32_t)-96,0x18B00,1};call(0x102AA0,constants,3);
    assert(!memcmp(s_vertex_constants[0],lower,sizeof(lower)));
    const float custom[4]={.25f,.75f,.125f,1};memcpy(guest_ptr(0x18B00),custom,sizeof(custom));
    constants[0]=(uint32_t)-36;call(0x102AA0,constants,3); /* Physical c60. */
    uint32_t object=0x18C00;memset(guest_ptr(object),0,0x180);
    write32(object,1);write32(object+4,0x10);write32(object+8,2);write32(object+12,9);
    for(unsigned i=0;i<16;++i)write32(object+0x1c+i*16,2);
    write32(object+0x1c,0x42);write32(object+0x114,(8u<<18)|0xB00);
    const uint32_t words[][4]={
        {0,(1u<<21)|0x1b,2u<<26,(15u<<12)|(1u<<11)},
        {0,(1u<<21)|(60u<<13)|0x1b,3u<<26,(15u<<12)|(1u<<11)|(3u<<3)|1}
    };
    memcpy(guest_ptr(object+0x118),words,sizeof(words));
    uint32_t shader[]={object+1};call(0x102940,shader,1);call(0x102BB0,off,1);
    const float vertices[3][4]={{40,40,0,1},{80,40,0,1},{80,80,0,1}};
    memcpy(guest_ptr(0x18E00),vertices,sizeof(vertices));
    uint32_t draw[]={5,3,0x18E00,16};call(0x1019C0,draw,4);assert(g_eax==0);
    uint8_t pixel[4];glReadPixels(60,240-50,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
    assert(abs(pixel[0]-64)<=1 && abs(pixel[1]-191)<=1 && abs(pixel[2]-32)<=1);
    float viewport_alias[2][4];memcpy(viewport_alias,s_vertex_constants[58],sizeof(viewport_alias));
    mode[0]=0;call(0x1026F0,mode,1);assert(g_eax==0 && read32(GUEST_DEVICE+0x2018)==0);
    assert((read32(0x10EC10)&0x1600)==0x1600);
    assert(!memcmp(defaults,s_vertex_constants[60],sizeof(defaults)));
    assert(!memcmp(viewport_alias,s_vertex_constants[58],sizeof(viewport_alias)));
    assert(!memcmp(s_vertex_constants[0],lower,sizeof(lower)));
    for(unsigned i=0;i<4;i++)assert(!memcmp(identity,s_vertex_constants[64+i*8],sizeof(identity)));
    const float plane[4]={0,0,1,0};assert(!memcmp(plane,s_vertex_constants[57],sizeof(plane)));
    assert(!memcmp(guest_ptr(GUEST_DEVICE+0xB98+60*16),custom,sizeof(custom)));
    call(0x1019C0,draw,4);assert(g_eax==0);
    glReadPixels(60,240-50,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
    assert(pixel[0]==0 && abs(pixel[1]-128)<=1 && pixel[2]==255);
    mode[0]=1;call(0x1026F0,mode,1);
    assert(!memcmp(defaults,s_vertex_constants[60],sizeof(defaults)));
    mode[0]=0;call(0x1026F0,mode,1);
    assert(glGetError()==GL_NO_ERROR);
    puts("PASS: mode1 preserves192-vector bank, negative constant indexing, mode0 shared aliases, native GPU colors, viewport and CPU-cache ordering");
}
