/* Native GPU ordering plus original4361 guest fence/busy ABI. */
static void test_gpu_fences(void)
{
    assert(read32(GUEST_DEVICE+0x30)==5 && read32(s_gpu_fence_completed_address)==3);
    assert(read32(GUEST_DEVICE+0x34)==s_gpu_fence_completed_address);
    uint32_t untouched_offsets[]={0,4,8,0x2264,0x2268},untouched[5];
    for(unsigned i=0;i<5;++i)untouched[i]=read32(GUEST_DEVICE+untouched_offsets[i]);
    uint32_t target[]={s_backbuffer_handle,0};call(0xFEF20,target,2);assert(g_eax==0);
    GLuint pbo;glGenBuffers(1,&pbo);glBindBuffer(GL_PIXEL_PACK_BUFFER,pbo);glBufferData(GL_PIXEL_PACK_BUFFER,8,NULL,GL_STREAM_READ);
    uint32_t clear[]={0,0,0xF0,0xFF204060,0x3F800000,0};call(0x100EA0,clear,6);assert(g_eax==0);
    glReadPixels(0,0,1,1,GL_RGBA,GL_UNSIGNED_BYTE,(void *)0);
    call(0xFED90,NULL,0);uint32_t first=g_eax;assert(first==5 && read32(GUEST_DEVICE+0x30)==7);
    GLsync first_sync=s_gpu_fences[s_gpu_fence_head].sync;assert(glIsSync(first_sync));
    clear[3]=0xFF806040;call(0x100EA0,clear,6);glReadPixels(0,0,1,1,GL_RGBA,GL_UNSIGNED_BYTE,(void *)4);
    call(0xFED90,NULL,0);uint32_t second=g_eax;assert(second==7 && read32(GUEST_DEVICE+0x30)==9);
    uint32_t wait[]={second,0};call(0x1034D0,wait,2);
    assert(!gpu_fence_pending(first) && !gpu_fence_pending(second) && !s_gpu_fence_count);
    uint8_t pixels[8];glGetBufferSubData(GL_PIXEL_PACK_BUFFER,0,8,pixels);
    const uint8_t expected[]={0x20,0x40,0x60,255,0x80,0x60,0x40,255};assert(!memcmp(pixels,expected,8));
    glBindBuffer(GL_PIXEL_PACK_BUFFER,0);glDeleteBuffers(1,&pbo);
    /* More than64 fence insertions retire only signaled native objects while
     * retaining bounded ring storage. Old IDs stay complete after slot reuse. */
    uint32_t last=second;
    for(unsigned i=0;i<GPU_FENCE_CAPACITY*3+1;++i){
        call(0xFED90,NULL,0);assert(g_eax==last+2);last=g_eax;
        assert(s_gpu_fence_count<=GPU_FENCE_CAPACITY);
    }
    wait[0]=last;wait[1]=1;call(0x1034D0,wait,2);assert(!s_gpu_fence_count && !gpu_fence_pending(first));
    uint32_t next=read32(GUEST_DEVICE+0x30);call(0x1037A0,NULL,0);
    assert(read32(GUEST_DEVICE+0x30)==next+2 && read32(s_gpu_fence_completed_address)==next);
    /* Original uint32 modular arithmetic spans FFFFFFFD,FFFFFFFF,1. */
    write32(GUEST_DEVICE+0x30,0xFFFFFFFD);write32(s_gpu_fence_completed_address,0xFFFFFFFB);
    call(0xFED90,NULL,0);assert(g_eax==0xFFFFFFFD);
    call(0xFED90,NULL,0);assert(g_eax==0xFFFFFFFF);
    call(0xFED90,NULL,0);assert(g_eax==1 && read32(GUEST_DEVICE+0x30)==3);
    wait[0]=1;wait[1]=0;call(0x1034D0,wait,2);assert(!gpu_fence_pending(0xFFFFFFFD)&&!gpu_fence_pending(0xFFFFFFFF)&&!gpu_fence_pending(1));
    /* IsBusy must preserve actual original bound flags, including parent
     * texture binding when querying an otherwise-unbound surface. */
    uint32_t create[]={2,2,1,0,6,0,0x1F000};call(0xFE9C0,create,7);assert(g_eax==0);
    uint32_t texture=read32(0x1F000),get[]={texture,0,0x1F004};call(0x103C30,get,3);assert(g_eax==0);
    uint32_t surface=read32(0x1F004),bind[]={0,texture};call(0xFFC90,bind,2);
    uint32_t busy[]={texture};call(0x103B20,busy,1);assert(g_eax==1);
    busy[0]=surface;call(0x103B20,busy,1);assert(g_eax==1);
    bind[1]=0;call(0xFFC90,bind,2);call(0xFED90,NULL,0);uint32_t time=g_eax;
    write32(texture+8,time);wait[0]=time;call(0x1034D0,wait,2);
    busy[0]=texture;call(0x103B20,busy,1);assert(g_eax==0 && read32(texture+8)==0);
    busy[0]=surface;call(0x103B20,busy,1);assert(g_eax==0);
    uint32_t release[]={surface};call(0x103AD0,release,1);release[0]=texture;call(0x103AD0,release,1);
    for(unsigned i=0;i<5;++i)assert(read32(GUEST_DEVICE+untouched_offsets[i])==untouched[i]);
    assert(glGetError()==GL_NO_ERROR);
    puts("PASS: native fence/PBO pixel ordering, original odd sequence/init and32-bit wrap, >64 bounded retirement, idle/current-time waits, resource/parent busy bits, no guest FIFO/MMIO writes");
}
