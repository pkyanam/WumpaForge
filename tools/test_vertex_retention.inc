/* Original array-address persistence: real GPU reads, no substitute attributes. */
static void test_vertex_retention(void)
{
    memset(s_vertex_arrays,0,sizeof(s_vertex_arrays));s_vertex_arrays_ready=0;
    uint32_t off[]={0},target[]={s_backbuffer_handle,0};
    call(0xFEF20,target,2);call(0xFDAD0,off,1);call(0xFE5C0,off,1);call(0xFE660,off,1);
    test_state(0x304,0);test_state(0x300,0);test_state(0x358,0x1010101);
    write32(0x10EF60,0);
    D3DVIEWPORT8 vp={0,0,320,240,0,1};memcpy(guest_ptr(0x20000),&vp,sizeof(vp));
    uint32_t viewport[]={0x20000};call(0xFF860,viewport,1);
    for(unsigned i=0;i<4;++i){uint32_t unbind[]={i,0};call(0xFFC90,unbind,2);}
    uint32_t stages[4][32]={{0}};
    for(unsigned i=0;i<4;++i)stages[i][12]=stages[i][16]=stages[i][20]=1;
    stages[0][12]=stages[0][16]=2;memcpy(guest_ptr(0x10EC18),stages,sizeof(stages));
    uint32_t object=0x20200;memset(guest_ptr(object),0,0x180);
    write32(object,1);write32(object+4,0x10);write32(object+8,2);write32(object+12,9);
    for(unsigned i=0;i<16;++i)write32(object+0x1c+i*16,2);
    write32(object+0x1c,0x42);
    write32(object+0x14+4*16,1);write32(object+0x18+4*16,4);write32(object+0x1c+4*16,0x32);
    uint32_t words[][4]={
        {0,(1u<<21)|0x1b,2u<<26,(15u<<12)|(1u<<11)},
        {0,(1u<<21)|(121u<<13)|0x1b,3u<<26,(15u<<12)|(1u<<11)|(3u<<3)|1}
    };
    write32(object+0x114,(8u<<18)|0xB00);memcpy(guest_ptr(object+0x118),words,sizeof(words));
    uint32_t shader[]={object+1};call(0x102940,shader,1);call(0x102BB0,off,1);
    const float white[4]={1,1,1,1};memcpy(s_vertex_constants[121],white,16);
    const float triangle[3][4]={{40,40,0,1},{80,40,0,1},{40,80,0,1}};
    uint32_t create[]={9*16,0,0,0,0x20400};call(0x100D70,create,5);assert(g_eax==0);
    uint32_t primary=read32(create[4]);
    for(unsigned i=0;i<3;++i)memcpy(guest_ptr(read32(primary+4)+i*sizeof(triangle)),triangle,sizeof(triangle));
    uint32_t old_heap=test_heap;create[0]=160;call(0x100D70,create,5);assert(g_eax==0);
    uint32_t secondary=read32(create[4]),secondary_data=read32(secondary+4);
    uint32_t stream[]={0,primary,16};call(0x102580,stream,3);write32(0x10F284,0);
    stream[0]=1;stream[1]=secondary;stream[2]=16;call(0x102580,stream,3);write32(0x10F290,8);
    const uint16_t indices[]={0,1,2};memcpy(guest_ptr(0x20420),indices,sizeof(indices));
    uint32_t bind[]={0,2};call(0xFFEA0,bind,2);
    uint32_t draw[]={5,3,0x20420};call(0x101BC0,draw,3);assert(g_eax==0);
    /* v4 is not read by the first program, but its array address is committed. */
    assert(s_vertex_arrays[4].handle==secondary && s_vertex_arrays[4].offset==44);
    uint64_t generation=s_vertex_arrays[4].generation;
    assert(generation && resource(secondary)->references==1);
    const float red[3]={1,0,0},green[3]={0,1,0};
    for(unsigned i=0;i<3;++i){
        memcpy(guest_ptr(secondary_data+44+i*12),red,12);
        memcpy(guest_ptr(secondary_data+80+i*12),green,12);
    }
    stream[1]=0;stream[2]=12;call(0x102580,stream,3);
    assert(resource(secondary)->bindings==0 && resource(secondary)->references==1);
    /* New declaration offset100 must not replace retained address44. */
    write32(object+0x18+4*16,100);
    words[1][1]=(1u<<21)|(4u<<9)|0x1b;words[1][2]=2u<<26;
    memcpy(guest_ptr(object+0x118),words,sizeof(words));call(0x102940,shader,1);
    bind[1]=3;call(0xFFEA0,bind,2);
    uint32_t clear[]={0,0,0xF0,0,0,0};call(0x100EA0,clear,6);call(0x101BC0,draw,3);assert(g_eax==0);
    uint8_t pixel[4];glReadPixels(50,240-50,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
    assert(pixel[0]==255 && pixel[1]==0 && pixel[2]==0);
    assert(s_vertex_arrays[4].offset==44 && s_vertex_arrays[4].stride==12);
    uint32_t direct[]={5,3,3};call(0x100EA0,clear,6);call(0x101B20,direct,3);assert(g_eax==0);
    glReadPixels(50,240-50,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
    assert(pixel[0]==0 && pixel[1]==255 && pixel[2]==0); /* current nonindexed start */
    /* Changed type/stride takes effect; old base/offset remains unchanged. */
    for(unsigned i=0;i<3;++i)write32(secondary_data+44+i*4,0xFF0000FF);
    stream[2]=4;call(0x102580,stream,3);write32(object+0x1c+4*16,0x40);call(0x102940,shader,1);
    call(0x100EA0,clear,6);call(0x101BC0,draw,3);assert(g_eax==0);
    glReadPixels(50,240-50,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
    assert(pixel[0]==0 && pixel[1]==0 && pixel[2]==255);
    uint8_t *gathered=NULL;struct VertexFetch fetch={.first=3,.indices=(const uint8_t *)indices};
    assert(gather_retained_vertex_attribute(5,&s_vertex_object.slots[4],3,&fetch,&gathered)==D3DERR_INVALIDCALL);
    struct VertexFetch too_far={.first=100};
    assert(gather_retained_vertex_attribute(4,&s_vertex_object.slots[4],3,&too_far,&gathered)==D3DERR_INVALIDCALL);
    /* Release must invalidate even if the guest heap later recycles the exact
     * handle and data addresses. Rebinding/drawing establishes a new generation. */
    uint32_t release[]={secondary};call(0x103AD0,release,1);assert(!resource(secondary));
    assert(gather_retained_vertex_attribute(4,&s_vertex_object.slots[4],3,&fetch,&gathered)==D3DERR_INVALIDCALL);
    test_heap=old_heap;call(0x100D70,create,5);assert(g_eax==0 && read32(create[4])==secondary);
    assert(read32(secondary+4)==secondary_data);
    assert(gather_retained_vertex_attribute(4,&s_vertex_object.slots[4],3,&fetch,&gathered)==D3DERR_INVALIDCALL);
    write32(object+0x18+4*16,0);call(0x102940,shader,1);write32(0x10F290,0);
    for(unsigned i=0;i<9;++i)write32(secondary_data+i*4,0xFF00FF00);
    stream[1]=secondary;call(0x102580,stream,3);call(0x101BC0,draw,3);assert(g_eax==0);
    assert(s_vertex_arrays[4].generation!=generation);
    stream[1]=0;call(0x102580,stream,3);call(0x101BC0,draw,3);assert(g_eax==0);
    glReadPixels(50,240-50,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);assert(pixel[1]==255);
    /* Inline fixed vertices leave array state unchanged. An actual fixed array
     * draw establishes original slots0/3/4/9, not native GL layout0/1/2. */
    struct {float p[4];uint32_t color,specular;float uv[2];} fixed[3];
    for(unsigned i=0;i<3;++i){
        memcpy(fixed[i].p,triangle[i],16);fixed[i].color=0xFFFFFFFF;
        fixed[i].specular=0xFF123456;fixed[i].uv[0]=fixed[i].uv[1]=0;
    }
    struct VertexArrayAddress saved=s_vertex_arrays[4];
    uint32_t fvf[]={0x1C4};call(0x102940,fvf,1);memcpy(guest_ptr(0x20700),fixed,sizeof(fixed));
    uint32_t up[]={5,3,0x20700,sizeof(fixed[0])};call(0x1019C0,up,4);assert(g_eax==0);
    assert(!memcmp(&saved,&s_vertex_arrays[4],sizeof(saved)));
    stream[0]=0;stream[1]=primary;stream[2]=sizeof(fixed[0]);call(0x102580,stream,3);
    memcpy(guest_ptr(read32(primary+4)),fixed,sizeof(fixed));direct[1]=0;
    call(0x101B20,direct,3);assert(g_eax==0);
    assert(s_vertex_arrays[4].handle==primary && s_vertex_arrays[4].offset==20 &&
           s_vertex_arrays[4].format==0x40 && s_vertex_arrays[9].offset==24);
    call(0x103AD0,release,1);release[0]=primary;call(0x103AD0,release,1);
    stream[0]=0;stream[1]=0;call(0x102580,stream,3);assert(!resource(primary));
    write32(0x10F284,0);write32(0x10F290,0);
    assert(glGetError()==GL_NO_ERROR);
    puts("PASS: native retained attribute bytes; dead declaration writes, new stride/type, old indexed base, current array start, strict bounds and same-address allocation generations");
}
