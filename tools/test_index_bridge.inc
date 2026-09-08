/* Include in graphics.c's smoke section after call() and test_state(). */
static void test_index_bridge(void)
{
    uint32_t target[]={s_backbuffer_handle,0}; call(0xFEF20,target,2); assert(g_eax==0);
    uint32_t off[]={0}; call(0xFDAD0,off,1);call(0xFE5C0,off,1);call(0xFE660,off,1);
    call(0x102BB0,off,1);
    test_state(0x304,0);test_state(0x300,0);test_state(0x358,0x1010101);
    uint32_t fill[]={GL_FILL};call(0xFDDF0,fill,1);
    for(unsigned i=0;i<4;++i){uint32_t bind[]={i,0};call(0xFFC90,bind,2);}
    D3DVIEWPORT8 vp={0,0,320,240,0,1};memcpy(guest_ptr(0x16000),&vp,sizeof(vp));
    uint32_t viewport[]={0x16000};call(0xFF860,viewport,1);
    uint32_t shader[]={D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1};call(0x102940,shader,1);
    struct {float x,y,z,rhw;uint32_t color;float u,v;} vertices[5]={
        {0,0,0,1,0xff0000ff,0,0}, /* Poison vertex: correct base1 never fetches it. */
        {40,40,0,1,0xff20c060,0,0},{80,40,0,1,0xff20c060,1,0},
        {80,80,0,1,0xff20c060,1,1},{40,80,0,1,0xff20c060,0,1}};
    uint32_t createvb[]={sizeof(vertices),0,shader[0],0,0x16040};call(0x100D70,createvb,5);assert(g_eax==0);
    uint32_t vb=read32(0x16040);memcpy(guest_ptr(read32(vb+4)),vertices,sizeof(vertices));
    uint32_t stream[]={0,vb,sizeof(vertices[0])};call(0x102580,stream,3);assert(g_eax==0);
    uint32_t create[]={14,0,0x2c,0,0x16044};call(0x100D00,create,5);assert(g_eax==0);
    uint32_t ib=read32(0x16044),data=read32(ib+4);
    assert(read32(ib)==0x01010001 && data==ib+12 && read32(ib+8)==0);
    uint16_t indices[]={0xffff,0,1,2,0,2,3};memcpy(guest_ptr(data),indices,sizeof(indices));
    uint32_t lock[]={ib,0x16048,0};call(0x100D40,lock,3);assert(g_eax==0 && read32(0x16048)==data);
    uint32_t bind[]={ib,1};call(0xFFEA0,bind,2);assert(g_eax==0);
    assert(read32(GUEST_DEVICE+0x38c)==ib && read32(GUEST_DEVICE+0x1c)==1 && read32(0x10ec0c)==data);
    assert(read32(ib)==0x01090001);
    call(0xFFEA0,bind,2);assert(resource(ib)->bindings==1); /* Same buffer does not leak a binding. */
    uint32_t release[]={ib};call(0x103AD0,release,1);
    assert(g_eax==0 && resource(ib) && read32(ib)==0x01090000);
    uint32_t clear[]={0,0,0xf0,0xff000000,0,0};call(0x100EA0,clear,6);
    uint32_t draw[]={5,6,data+2};call(0x101BC0,draw,3);assert(g_eax==0);
    uint8_t pixel[4];glReadPixels(60,180,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
    assert(pixel[0]==0x20 && pixel[1]==0xc0 && pixel[2]==0x60);
    /* End-of-IB, bad VB fetch, and base overflow fail before any draw. */
    draw[2]=data+4;call(0x101BC0,draw,3);assert(g_eax==(uint32_t)D3DERR_INVALIDCALL);
    draw[2]=data;call(0x101BC0,draw,3);assert(g_eax==(uint32_t)D3DERR_INVALIDCALL);
    bind[1]=UINT32_MAX;call(0xFFEA0,bind,2);
    draw[2]=data+2;call(0x101BC0,draw,3);assert(g_eax==(uint32_t)D3DERR_INVALIDCALL);
    /* A raw pointer is accepted without a bound IB; the base remains independent. */
    memcpy(guest_ptr(0x16060),indices+1,12);bind[0]=0;bind[1]=1;call(0xFFEA0,bind,2);
    assert(!resource(ib) && read32(0x10ec0c)==0);
    draw[2]=0x16060;call(0x101BC0,draw,3);assert(g_eax==0);
    glReadPixels(60,180,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
    assert(pixel[0]==0x20 && pixel[1]==0xc0 && pixel[2]==0x60);
    create[2]=0x2d;call(0x100D00,create,5);assert(g_eax==(uint32_t)D3DERR_INVALIDCALL);
    release[0]=vb;call(0x103AD0,release,1);assert(resource(vb));
    stream[1]=0;call(0x102580,stream,3);assert(!resource(vb));
    assert(glGetError()==GL_NO_ERROR);
    puts("PASS: indexed GPU readback, base vertex, literal offset/raw indices, INDEX16 header, bounds, binding lifetime");
}
