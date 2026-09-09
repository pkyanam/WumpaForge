/* Synthetic BGRA8 resources exercise padded rows and both CPU/GPU copy sides. */
static uint32_t linear_bgra_pattern(unsigned x,unsigned y)
{ return ((0x30u+y*17)<<24)|((x&255)<<16)|((y*31)<<8)|((x+y)&255); }
static void test_linear_bgra(void)
{
    const unsigned width=259,height=5; /* Cross row-flip chunks; odd height. */
    uint32_t create[]={width,height,1,0,0x12,0,0x1E000};call(0xFE9C0,create,7);assert(g_eax==0);
    uint32_t source=read32(0x1E000);struct Resource *r=resource(source);assert(r->pitch>width*4);
    memset(guest_ptr(r->data),0xCD,r->bytes);
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x)write32(r->data+y*r->pitch+x*4,linear_bgra_pattern(x,y));
    /* Foreign unpack state cannot change guest data interpretation and must
     * survive the direct upload, including a bound pixel-unpack buffer. */
    GLuint pbo;glGenBuffers(1,&pbo);glBindBuffer(GL_PIXEL_UNPACK_BUFFER,pbo);glBufferData(GL_PIXEL_UNPACK_BUFFER,4096,NULL,GL_STREAM_DRAW);
    glPixelStorei(GL_UNPACK_ROW_LENGTH,300);glPixelStorei(GL_UNPACK_SKIP_ROWS,1);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS,3);glPixelStorei(GL_UNPACK_ALIGNMENT,8);glPixelStorei(GL_UNPACK_SWAP_BYTES,1);
    assert(upload_texture(r)==0 && r->upload_serial==1 && r->snapshot_valid);
    GLint state;glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&state);assert(state==(GLint)pbo);
    glGetIntegerv(GL_UNPACK_ROW_LENGTH,&state);assert(state==300);
    glGetIntegerv(GL_UNPACK_SKIP_ROWS,&state);assert(state==1);
    glGetIntegerv(GL_UNPACK_SKIP_PIXELS,&state);assert(state==3);
    glGetIntegerv(GL_UNPACK_ALIGNMENT,&state);assert(state==8);
    glGetIntegerv(GL_UNPACK_SWAP_BYTES,&state);assert(state==1);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);glDeleteBuffers(1,&pbo);
    glPixelStorei(GL_UNPACK_ROW_LENGTH,0);glPixelStorei(GL_UNPACK_SKIP_ROWS,0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);glPixelStorei(GL_UNPACK_ALIGNMENT,4);glPixelStorei(GL_UNPACK_SWAP_BYTES,0);
    uint32_t actual[259*5];glBindTexture(GL_TEXTURE_2D,texture_gl_name(r,0));
    glGetTexImage(GL_TEXTURE_2D,0,GL_BGRA,GL_UNSIGNED_INT_8_8_8_8_REV,actual);
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x)assert(actual[y*width+x]==linear_bgra_pattern(x,y));
    assert(upload_texture(r)==0 && r->upload_serial==1);
    write32(r->data+4*r->pitch+258*4,0x76543210);assert(upload_texture(r)==0 && r->upload_serial==2);
    glGetTexImage(GL_TEXTURE_2D,0,GL_BGRA,GL_UNSIGNED_INT_8_8_8_8_REV,actual);assert(actual[4*width+258]==0x76543210);
    /* Full-budget fallback still uploads current guest bytes directly. */
    s_texture_snapshot_bytes-=r->bytes;free(r->encoded_snapshot);r->encoded_snapshot=NULL;r->snapshot_valid=0;
    size_t budget=s_texture_snapshot_bytes;s_texture_snapshot_bytes=TEXTURE_SNAPSHOT_BUDGET;
    write32(r->data,0x12345678);assert(upload_texture(r)==0 && !r->encoded_snapshot);
    glGetTexImage(GL_TEXTURE_2D,0,GL_BGRA,GL_UNSIGNED_INT_8_8_8_8_REV,actual);assert(actual[0]==0x12345678);
    s_texture_snapshot_bytes=budget;
    uint32_t get[]={source,0,0x1E004};call(0x103C30,get,3);assert(g_eax==0);uint32_t source_surface=read32(0x1E004);
    create[0]=create[1]=7;create[6]=0x1E008;call(0xFE9C0,create,7);assert(g_eax==0);
    uint32_t destination=read32(0x1E008);struct Resource *dest=resource(destination);
    memset(guest_ptr(dest->data),0xA5,dest->bytes);
    for(unsigned y=0;y<7;++y)for(unsigned x=0;x<7;++x)write32(dest->data+y*dest->pitch+x*4,0xFF0F0F0F);
    get[0]=destination;get[2]=0x1E00C;call(0x103C30,get,3);assert(g_eax==0);uint32_t destination_surface=read32(0x1E00C);
    D3DRECT rect={2,1,5,3};int32_t point[2]={1,3};
    memcpy(guest_ptr(0x1E020),&rect,sizeof(rect));memcpy(guest_ptr(0x1E030),point,sizeof(point));
    uint32_t copy[]={source_surface,0x1E020,1,destination_surface,0x1E030};call(0xFF580,copy,5);assert(g_eax==0);
    for(unsigned y=0;y<7;++y)for(unsigned x=0;x<7;++x)
        assert(read32(dest->data+y*dest->pitch+x*4)==(y>=3&&y<5&&x>=1&&x<4?linear_bgra_pattern(x+1,y-2):0xFF0F0F0F));
    for(unsigned y=0;y<7;++y)for(unsigned x=28;x<dest->pitch;++x)assert(((uint8_t *)guest_ptr(dest->data))[y*dest->pitch+x]==0xA5);
    /* Render target prepare/readback/resolve retains top-down guest rows and
     * visible alpha; unused pitch padding is never written by GPU resolves. */
    uint32_t target[]={source_surface,0};call(0xFEF20,target,2);assert(g_eax==0);
    uint32_t pixel;glReadPixels(0,height-1,1,1,GL_BGRA,GL_UNSIGNED_INT_8_8_8_8_REV,&pixel);assert(pixel==0x12345678);
    glReadPixels(258,0,1,1,GL_BGRA,GL_UNSIGNED_INT_8_8_8_8_REV,&pixel);assert(pixel==0x76543210);
    glPixelStorei(GL_PACK_ROW_LENGTH,300);glPixelStorei(GL_PACK_SKIP_ROWS,1);glPixelStorei(GL_PACK_SKIP_PIXELS,3);glPixelStorei(GL_PACK_ALIGNMENT,8);
    assert(surface_pixels(resource(source_surface),actual)==0 && actual[0]==0x12345678 && actual[4*width+258]==0x76543210);
    glGetIntegerv(GL_PACK_ROW_LENGTH,&state);assert(state==300);glGetIntegerv(GL_PACK_SKIP_ROWS,&state);assert(state==1);
    glGetIntegerv(GL_PACK_SKIP_PIXELS,&state);assert(state==3);glGetIntegerv(GL_PACK_ALIGNMENT,&state);assert(state==8);
    glPixelStorei(GL_PACK_ROW_LENGTH,0);glPixelStorei(GL_PACK_SKIP_ROWS,0);glPixelStorei(GL_PACK_SKIP_PIXELS,0);glPixelStorei(GL_PACK_ALIGNMENT,4);
    uint32_t clear[]={0,0,0xF0,0x40602010,0x3F800000,0};call(0x100EA0,clear,6);assert(g_eax==0);
    rect=(D3DRECT){0,0,width,1};memcpy(guest_ptr(0x1E020),&rect,sizeof(rect));
    clear[0]=1;clear[1]=0x1E020;clear[3]=0xA0102030;call(0x100EA0,clear,6);assert(g_eax==0);
    target[0]=s_backbuffer_handle;call(0xFEF20,target,2);assert(g_eax==0);
    for(unsigned y=0;y<height;++y){
        for(unsigned x=0;x<width;++x)assert(read32(r->data+y*r->pitch+x*4)==(y?0x40602010:0xA0102030));
        for(unsigned x=width*4;x<r->pitch;++x)assert(((uint8_t *)guest_ptr(r->data))[y*r->pitch+x]==0xCD);
    }
    assert(upload_texture(r)==0);uint64_t serial=r->upload_serial;
    glBindTexture(GL_TEXTURE_2D,texture_gl_name(r,0));glGetTexImage(GL_TEXTURE_2D,0,GL_BGRA,GL_UNSIGNED_INT_8_8_8_8_REV,actual);
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x)assert(actual[y*width+x]==(y?0x40602010:0xA0102030));
    assert(upload_texture(r)==0 && r->upload_serial==serial);
    uint32_t release[]={source_surface};call(0x103AD0,release,1);release[0]=destination_surface;call(0x103AD0,release,1);
    release[0]=source;call(0x103AD0,release,1);release[0]=destination;call(0x103AD0,release,1);
    assert(!resource(source)&&!resource(destination)&&glGetError()==GL_NO_ERROR);
    puts("PASS: direct linear BGRA8 GPU bytes/alpha, padded odd rows, unpack/pack state, guest writes, budget fallback, subrect copy, target orientation/resolve and upload reuse");
}
