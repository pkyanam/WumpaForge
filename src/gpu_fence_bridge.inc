/* Original4361 InsertFence/BlockOnTime/IsBusy compatibility backed by real
 * native GPU command completion. No guest FIFO/MMIO acknowledgement. */
#define GPU_FENCE_CAPACITY 64u
static struct { uint32_t time; GLsync sync; } s_gpu_fences[GPU_FENCE_CAPACITY];
static unsigned s_gpu_fence_head,s_gpu_fence_count;
static uint32_t s_gpu_fence_completed_address;
static void gpu_fence_error(const char *message)
{
    fprintf(stderr,"[wrath fences] %s (guest return0x%X)\n",message,read32(g_esp));abort();
}
static void initialize_gpu_fences(void)
{
    s_gpu_fence_completed_address=xbox_HeapAlloc(4,4);
    if(!s_gpu_fence_completed_address)gpu_fence_error("completion allocation failed");
    /* Original1030F2/1030F9/1030FF initialize next5, completed3. */
    write32(GUEST_DEVICE+0x30,5);write32(GUEST_DEVICE+0x34,s_gpu_fence_completed_address);
    write32(s_gpu_fence_completed_address,3);write32(GUEST_DEVICE+0x48,3);
}
static int gpu_fence_signaled(GLsync sync,int wait)
{
    for(;;) {
        GLenum status=glClientWaitSync(sync,wait?GL_SYNC_FLUSH_COMMANDS_BIT:0,wait?1000000000ull:0);
        if(status==GL_ALREADY_SIGNALED || status==GL_CONDITION_SATISFIED)return 1;
        if(status==GL_TIMEOUT_EXPIRED) { if(wait)continue;return 0; }
        gpu_fence_error("native completion wait failed");
    }
}
static void gpu_fences_poll(int wait_oldest)
{
    while(s_gpu_fence_count) {
        unsigned index=s_gpu_fence_head;
        if(!gpu_fence_signaled(s_gpu_fences[index].sync,wait_oldest))break;
        write32(s_gpu_fence_completed_address,s_gpu_fences[index].time);
        glDeleteSync(s_gpu_fences[index].sync);s_gpu_fences[index].sync=NULL;
        s_gpu_fence_head=(index+1)%GPU_FENCE_CAPACITY;--s_gpu_fence_count;
        wait_oldest=0;
    }
}
static uint32_t gpu_insert_fence(void)
{
    gpu_fences_poll(s_gpu_fence_count==GPU_FENCE_CAPACITY);
    unsigned index=(s_gpu_fence_head+s_gpu_fence_count)%GPU_FENCE_CAPACITY;
    GLsync sync=glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);
    if(!sync)gpu_fence_error("native fence allocation failed");
    uint32_t time=read32(GUEST_DEVICE+0x30);
    s_gpu_fences[index].time=time;s_gpu_fences[index].sync=sync;++s_gpu_fence_count;
    write32(GUEST_DEVICE+0x30,time+2);glFlush();return time;
}
static int gpu_fence_pending(uint32_t time)
{
    gpu_fences_poll(0);
    uint32_t next=read32(GUEST_DEVICE+0x30),completed=read32(s_gpu_fence_completed_address);
    /* Original unsigned modular differences, including old/future/zero times. */
    return next-time < next-completed;
}
static void gpu_block_on_time(uint32_t time)
{
    if(time==read32(GUEST_DEVICE+0x30))gpu_insert_fence();
    while(gpu_fence_pending(time)) {
        if(!s_gpu_fence_count)gpu_fence_error("pending time has no submitted native fence");
        gpu_fences_poll(1);
    }
}
void sub_000FED90(void) /* InsertFence(), ret0, DWORD sequence result. */
{
    if(!s_device || !graphics_thread())gpu_fence_error("device unavailable");
    finish(0,gpu_insert_fence());
}
void sub_001034D0(void) /* BlockOnTime(time,avoid_interrupt), ret8, void. */
{
    uint32_t time=arg(0),avoid_interrupt=arg(1);
    if(!s_device || avoid_interrupt>1)gpu_fence_error("unsupported BlockOnTime arguments");
    /* Both original wait mechanisms have the same completion contract. */
    gpu_block_on_time(time);finish(8,0);
}
void sub_001037A0(void) /* BlockUntilIdle(), ret0, void. */
{
    if(!s_device || !graphics_thread())gpu_fence_error("device unavailable");
    gpu_block_on_time(read32(GUEST_DEVICE+0x30));finish(0,0);
}
void sub_00103B20(void) /* Resource_IsBusy(resource), ret4, BOOL. */
{
    uint32_t handle=arg(0);struct Resource *r=resource(handle);
    if(!s_device || !r)gpu_fence_error("IsBusy unknown resource");
    uint32_t common=read32(handle);
    if((common&0x70000)==0x50000 && r->owner) {
        if((common&0x780000)||(read32(r->owner)&0x780000)){finish(4,1);return;}
        handle=r->owner;r=resource(handle);common=read32(handle);
        if(!r)gpu_fence_error("IsBusy unknown surface parent");
    }
    if((common&0x780000) || ((common&0x70000)==0x50000 && r->owner && (read32(r->owner)&0x780000))) {
        finish(4,1);return;
    }
    uint32_t time=read32(handle+8);
    if(time && gpu_fence_pending(time)){finish(4,1);return;}
    write32(handle+8,0);finish(4,0);
}
