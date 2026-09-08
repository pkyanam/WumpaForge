/* Native GPU regression: independent buffers, indexed fetch order and lifetime. */
static void test_multistream(void)
{
    uint32_t target[]={s_backbuffer_handle,0},off[]={0};
    call(0xFEF20,target,2); call(0xFDAD0,off,1);call(0xFE5C0,off,1);call(0xFE660,off,1);
    test_state(0x304,0);test_state(0x300,0);test_state(0x358,0x1010101);
    D3DVIEWPORT8 vp={0,0,320,240,0,1}; memcpy(guest_ptr(0x18600),&vp,sizeof(vp));
    uint32_t viewport[]={0x18600};call(0xFF860,viewport,1);
    for(unsigned i=0;i<4;i++){uint32_t unbind[]={i,0};call(0xFFC90,unbind,2);}
    uint32_t stages[4][32]={{0}};
    for(unsigned i=0;i<4;i++)stages[i][12]=stages[i][16]=stages[i][20]=1;
    stages[0][12]=stages[0][16]=2; /* SELECTARG1 diffuse, including default alpha1. */
    memcpy(guest_ptr(0x10EC18),stages,sizeof(stages));
    uint32_t object=0x18800;memset(guest_ptr(object),0,0x180);
    write32(object,1);write32(object+4,0x10);write32(object+8,2);write32(object+12,9);
    for(unsigned i=0;i<16;i++)write32(object+0x1c+i*16,2);
    write32(object+0x1c,0x42);
    write32(object+0x14+6*16,1);write32(object+0x18+6*16,4);write32(object+0x1c+6*16,0x32);
    uint32_t words[][4]={
        {0,(1u<<21)|0x1b,2u<<26,(15u<<12)|(1u<<11)},
        {0,(1u<<21)|(6u<<9)|0x1b,2u<<26,(15u<<12)|(1u<<11)|(3u<<3)|1}
    };
    write32(object+0x114,(8u<<18)|0xB00);memcpy(guest_ptr(object+0x118),words,sizeof(words));
    uint32_t shader[]={object+1};call(0x102940,shader,1);assert(g_eax==0);call(0x102BB0,off,1);
    const float positions[5][4]={{200,200,0,1},{40,40,0,1},{80,40,0,1},{80,80,0,1},{40,80,0,1}};
    struct {uint32_t poison;float color[3];uint32_t tail;} colors[5]={
        {0,{1,1,0},0},{0,{1,0,0},0},{0,{0,1,0},0},{0,{0,0,1},0},{0,{1,1,1},0}};
    uint32_t create[]={4+sizeof(positions),0,0,0,0x18A00};call(0x100D70,create,5);assert(g_eax==0);
    uint32_t primary=read32(create[4]);memcpy(guest_ptr(read32(primary+4)+4),positions,sizeof(positions));
    create[0]=8+sizeof(colors);call(0x100D70,create,5);assert(g_eax==0);
    uint32_t secondary=read32(create[4]);memcpy(guest_ptr(read32(secondary+4)+8),colors,sizeof(colors));
    uint32_t stream[]={0,primary,16};call(0x102580,stream,3);assert(g_eax==0);
    write32(0x10F284,4);
    stream[0]=1;stream[1]=secondary;stream[2]=sizeof(colors[0]);call(0x102580,stream,3);assert(g_eax==0);
    write32(0x10F290,8);
    assert(s_stream_handle==primary && s_stream_stride==16 && read32(0x10F294)==secondary);
    call(0x102580,stream,3);assert(resource(secondary)->bindings==1);
    uint16_t indices[]={2,0,1};memcpy(guest_ptr(0x18A20),indices,sizeof(indices));
    uint32_t bind[]={0,1};call(0xFFEA0,bind,2);
    struct VertexFetch fetch={.first=1,.indices=(const uint8_t *)indices};uint8_t *gathered=NULL;
    assert(gather_vertex_attribute(1,4,12,3,&fetch,&gathered)==0);
    assert(!memcmp(gathered,colors[3].color,12) && !memcmp(gathered+12,colors[1].color,12) &&
           !memcmp(gathered+24,colors[2].color,12));free(gathered);
    uint32_t clear[]={0,0,0xF0,0,0,0};call(0x100EA0,clear,6);
    uint32_t draw[]={5,3,0x18A20};call(0x101BC0,draw,3);assert(g_eax==0);
    uint8_t pixel[4];glReadPixels(60,240-50,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
    assert(pixel[0]>115 && pixel[0]<140 && pixel[1]>55 && pixel[1]<80 && pixel[2]>55 && pixel[2]<80);
    /* Non-indexed firstVertex uses the same numbering for every stream. */
    call(0x100EA0,clear,6);uint32_t direct[]={5,1,3};call(0x101B20,direct,3);assert(g_eax==0);
    uint8_t other[4];glReadPixels(60,240-50,1,1,GL_RGBA,GL_UNSIGNED_BYTE,other);
    assert(!memcmp(pixel,other,4));
    /* Both paths triangulate original quads identically across all streams. */
    direct[0]=8;direct[2]=4;call(0x100EA0,clear,6);call(0x101B20,direct,3);assert(g_eax==0);
    glReadPixels(50,240-70,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
    uint16_t quad[]={0,1,2,3};memcpy(guest_ptr(0x18A20),quad,sizeof(quad));
    draw[0]=8;draw[1]=4;call(0x100EA0,clear,6);call(0x101BC0,draw,3);assert(g_eax==0);
    glReadPixels(50,240-70,1,1,GL_RGBA,GL_UNSIGNED_BYTE,other);assert(!memcmp(pixel,other,4));
    /* Bounds reject before upload, including base overflow and stream offsets. */
    fetch.first=UINT32_MAX;assert(gather_vertex_attribute(1,4,12,3,&fetch,&gathered)==D3DERR_INVALIDCALL);
    fetch.first=1;write32(0x10F290,UINT32_MAX);
    assert(gather_vertex_attribute(1,4,12,3,&fetch,&gathered)==D3DERR_INVALIDCALL);write32(0x10F290,8);
    stream[2]=0;call(0x102580,stream,3);assert(g_eax==0);
    assert(gather_vertex_attribute(1,4,12,3,&fetch,&gathered)==0);
    for(unsigned i=0;i<3;i++)assert(!memcmp(gathered+i*12,colors[0].color,12));free(gathered);
    stream[0]=16;call(0x102580,stream,3);assert(g_eax==(uint32_t)D3DERR_INVALIDCALL);
    /* Story09: retained1280-byte stream1/stride160, indexed0..47. The real
     * shader multiplies every v6 use by c122.x=0. A stale bound buffer must
     * not reject this exact-dead input; live nonzero constants still reject. */
    create[0]=1280;call(0x100D70,create,5);assert(g_eax==0);
    uint32_t stale=read32(create[4]);
    stream[0]=1;stream[1]=stale;stream[2]=160;call(0x102580,stream,3);assert(g_eax==0);
    write32(0x10F290,0);
    write32(object+8,3);write32(object+12,13);write32(object+0x18+6*16,0);
    uint32_t dead_words[][4]={
        {0,(1u<<21)|0x1b,2u<<26,(15u<<12)|(1u<<11)},
        {0,(1u<<21)|(121u<<13)|0x1b,3u<<26,15u<<24},
        {0,(4u<<21)|(122u<<13)|(6u<<9),(3u<<26)|(0x1bu<<17)|(2u<<11)|(0x1bu<<2),
            (1u<<28)|(15u<<12)|(1u<<11)|(3u<<3)|1}
    };
    write32(object+0x114,(12u<<18)|0xB00);memcpy(guest_ptr(object+0x118),dead_words,sizeof(dead_words));
    write32(object+0x148,0);call(0x102940,shader,1);assert(g_eax==0);
    const float baseline[4]={.25f,.5f,.75f,1};memcpy(s_vertex_constants[121],baseline,sizeof(baseline));
    memset(s_vertex_constants[122],0,sizeof(s_vertex_constants[122]));
    const uint16_t stale_indices[]={0,47,1};
    struct VertexFetch stale_fetch={.indices=(const uint8_t *)stale_indices};
    assert(gather_vertex_attribute(1,0,12,3,&stale_fetch,&gathered)==D3DERR_INVALIDCALL);
    call(0x100EA0,clear,6);
    assert(draw_vertices_data_fetch(5,3,positions+1,16,&stale_fetch)==0);
    glReadPixels(60,240-50,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
    assert(pixel[0]==64 && pixel[1]==128 && pixel[2]==191);
    s_vertex_constants[122][0]=1e-30f;
    assert(draw_vertices_data_fetch(5,3,positions+1,16,&stale_fetch)==D3DERR_INVALIDCALL);
    s_vertex_constants[122][0]=1;
    assert(draw_vertices_data_fetch(5,3,positions+1,16,&stale_fetch)==D3DERR_INVALIDCALL);
    s_vertex_constants[122][0]=-0.0f;
    assert(draw_vertices_data_fetch(5,3,positions+1,16,&stale_fetch)==0);
    glReadPixels(60,240-50,1,1,GL_RGBA,GL_UNSIGNED_BYTE,other);assert(!memcmp(pixel,other,4));
    uint32_t stale_release[]={stale};call(0x103AD0,stale_release,1);
    stream[1]=secondary;stream[2]=0;call(0x102580,stream,3);assert(!resource(stale));
    puts("PASS: retained1280-byte/stride160 stream with index47, exactzero GPU output and live nonzero/nearzero bounds rejection");
    /* One resource bound twice survives unbinding either stream. */
    stream[0]=2;stream[2]=20;call(0x102580,stream,3);assert(g_eax==0 && resource(secondary)->bindings==2);
    uint32_t release[]={secondary};call(0x103AD0,release,1);assert(resource(secondary));
    stream[0]=1;stream[1]=0;call(0x102580,stream,3);assert(resource(secondary));
    stream[0]=2;call(0x102580,stream,3);assert(!resource(secondary));
    release[0]=primary;call(0x103AD0,release,1);stream[0]=0;call(0x102580,stream,3);assert(!resource(primary));
    write32(0x10F284,0);write32(0x10F290,0);
    assert(glGetError()==GL_NO_ERROR);
    puts("PASS: real multistream GPU colors, indexed/base/start/quad numbering, offsets, zero stride, bounds and multiple-binding lifetime");
}
