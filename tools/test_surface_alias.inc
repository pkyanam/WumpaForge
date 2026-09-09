/* Surface wrappers identify the same physical texture storage in the SDK. */
static void test_surface_alias(void)
{
    uint32_t texture,surface=texture_depth_surface(16,16,6,&texture);
    struct Resource *parent=resource(texture);
    for(unsigned i=0;i<parent->bytes;i+=4)write32(parent->data+i,0xFF0000FF);
    uint32_t get[]={texture,0,0x1E040};call(0x103C30,get,3);assert(g_eax==0);
    uint32_t alias=read32(0x1E040),other_texture,other=texture_depth_surface(16,16,0x12,&other_texture);
    assert(alias!=surface && resource(alias)->data==resource(surface)->data);
    uint32_t target[]={surface,0};call(0xFEF20,target,2);assert(g_eax==0);
    uint32_t clear[]={0,0,0xF0,0xFFFF0000,0x3F800000,0};call(0x100EA0,clear,6);assert(g_eax==0);
    /* The alias has never been bound; guest memory still contains blue. Its
     * CopyRects source must nevertheless observe the active storage's red. */
    assert(read32(parent->data)==0xFF0000FF);
    uint32_t copy[]={alias,0,0,other,0};call(0xFF580,copy,5);assert(g_eax==0);
    struct Resource *out=resource(other_texture);
    uint32_t actual=read32(out->data);
    if(actual!=0xFFFF0000)fprintf(stderr,"SURFACE-ALIAS source copied %08X; expected live GPU FFFF0000\n",actual);
    assert(actual==0xFFFF0000);
    /* The inverse alias writes into the live target, preserving its outside
     * pixels and surviving the later GPU→guest resolve. */
    for(unsigned y=0;y<16;++y)for(unsigned x=0;x<16;++x)write32(out->data+y*out->pitch+x*4,0xFF00FF00);
    D3DRECT rectangle={1,2,5,5};int32_t point[]={3,7};
    memcpy(guest_ptr(0x1E050),&rectangle,sizeof(rectangle));memcpy(guest_ptr(0x1E060),point,sizeof(point));
    copy[0]=other;copy[1]=0x1E050;copy[2]=1;copy[3]=alias;copy[4]=0x1E060;call(0xFF580,copy,5);assert(g_eax==0);
    glReadPixels(3,16-8,1,1,GL_BGRA,GL_UNSIGNED_INT_8_8_8_8_REV,&actual);assert(actual==0xFF00FF00);
    glReadPixels(2,16-8,1,1,GL_BGRA,GL_UNSIGNED_INT_8_8_8_8_REV,&actual);assert(actual==0xFFFF0000);
    target[0]=s_backbuffer_handle;target[1]=s_depthbuffer_handle;call(0xFEF20,target,2);assert(g_eax==0);
    for(unsigned y=0;y<16;++y)for(unsigned x=0;x<16;++x)
        assert(read32(parent->data+morton_index(x,y,16,16)*4)==(x>=3&&x<7&&y>=7&&y<10?0xFF00FF00:0xFFFF0000));
    uint32_t release[]={alias};call(0x103AD0,release,1);release[0]=surface;call(0x103AD0,release,1);release[0]=texture;call(0x103AD0,release,1);
    release[0]=other;call(0x103AD0,release,1);release[0]=other_texture;call(0x103AD0,release,1);
    assert(!resource(alias)&&!resource(surface)&&!resource(texture)&&!resource(other_texture)&&glGetError()==GL_NO_ERROR);
    puts("PASS active surface storage aliases: source CopyRects sees GPU pixels, destination subrect reaches live target and survives swizzled resolve; lifetimes preserved");
}
