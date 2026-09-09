/* LockRect must expose completed original texture memory, including an active
 * native render target whose GPU contents have not been resolved by a switch. */
static void test_texture_target_lock(void)
{
    for(unsigned variant=0;variant<2;++variant) {
        uint32_t format=variant?0x12:6,level=variant?0:1;
        uint32_t create[]={variant?16:32,variant?16:32,variant?1:2,1,format,0,0x1E100};
        call(0xFE9C0,create,7);assert(g_eax==0);uint32_t texture=read32(0x1E100);
        struct Resource *parent=resource(texture);
        for(unsigned i=0;i<parent->bytes;i+=4)write32(parent->data+i,0xFF0000FF);
        uint32_t get[]={texture,level,0x1E104};call(0x103C30,get,3);assert(g_eax==0);
        uint32_t surface=read32(0x1E104),target[]={surface,0};
        call(0xFEF20,target,2);assert(g_eax==0);
        glDisable(GL_SCISSOR_TEST);glColorMask(1,1,1,1);
        uint32_t clear[]={0,0,0xF0,0xFFFF0000,0x3F800000,0};call(0x100EA0,clear,6);assert(g_eax==0);
        glEnable(GL_SCISSOR_TEST);glScissor(0,8,8,8);glClearColor(0,1,0,1);glClear(GL_COLOR_BUFFER_BIT);glDisable(GL_SCISSOR_TEST);
        assert(read32(parent->data+parent->offsets[level])==0xFF0000FF);
        GLint before;glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&before);
        uint32_t lock[]={texture,level,0x1E110,0,0xA0};
        /* NOOVERWRITE skips the original wait; do not introduce a readback.
         * Reading here is only a test of the host staging remaining untouched,
         * not a promise about racing GPU reads on original hardware. */
        call(0x103C80,lock,5);assert(g_eax==0 && read32(parent->data+parent->offsets[level])==0xFF0000FF);
        if(!variant) {
            lock[1]=0;lock[4]=0x80;call(0x103C80,lock,5);assert(g_eax==0);
            assert(read32(read32(0x1E114))==0xFF0000FF && read32(parent->data+parent->offsets[level])==0xFF0000FF);
        }
        lock[1]=level;lock[4]=0x80;
        call(0x103C80,lock,5);assert(g_eax==0);
        uint32_t address=read32(0x1E114),pitch=read32(0x1E110),actual=read32(address);
        if(actual!=0xFF00FF00)fprintf(stderr,"TARGET-LOCK format%X level%u returned%08X expected GPU FF00FF00; pitch%u\n",format,level,actual,pitch);
        assert(actual==0xFF00FF00 && address==parent->data+parent->offsets[level] && pitch==parent->pitches[level]);
        for(unsigned y=0;y<16;++y)for(unsigned x=0;x<16;++x) {
            uint32_t offset=variant?y*pitch+x*4:morton_index(x,y,16,16)*4;
            assert(read32(address+offset)==(x<8&&y<8?0xFF00FF00:0xFFFF0000));
        }
        GLint after;glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&after);assert(after==before && read32(GUEST_DEVICE+0x2070)==surface);
        if(!variant)assert(read32(parent->data)==0xFF0000FF); /* Other mip unchanged. */
        if(variant) {
            D3DRECT rectangle={2,3,5,7};memcpy(guest_ptr(0x1E120),&rectangle,sizeof(rectangle));
            lock[3]=0x1E120;call(0x103C80,lock,5);assert(g_eax==0);
            assert(read32(0x1E114)==address+3*pitch+8 && read32(read32(0x1E114))==0xFF00FF00);
        }
        target[0]=s_backbuffer_handle;target[1]=s_depthbuffer_handle;call(0xFEF20,target,2);assert(g_eax==0);
        uint32_t release[]={surface};call(0x103AD0,release,1);release[0]=texture;call(0x103AD0,release,1);
        assert(!resource(surface)&&!resource(texture)&&glGetError()==GL_NO_ERROR);
    }
    puts("PASS active texture synchronized read locks: GPU contents, swizzled mip/linear rectangle, pitch/orientation, other mip, current target and lifetime");
}
