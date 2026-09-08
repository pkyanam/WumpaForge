/* Actual GPU readbacks after retained guest pointers change without Lock. */
static void test_texture_snapshot(void)
{
    size_t baseline=s_texture_snapshot_bytes;
    uint32_t create[]={4,4,0,0,6,0,0xB200};
    call(0xFE9C0,create,7); assert(g_eax==0);
    uint32_t handle=read32(0xB200),bind[]={0,handle};
    struct Resource *r=resource(handle);
    for(unsigned i=0;i<r->bytes;i+=4) write32(r->data+i,0xFFFF0000);
    call(0xFFC90,bind,2); assert(g_eax==0 && r->upload_serial==1 && r->snapshot_valid);
    for(unsigned i=0;i<20;++i) { call(0xFFC90,bind,2); assert(g_eax==0); }
    assert(r->upload_serial==1);
    write32(r->data,0xFF00FF00); write32(r->data+r->offsets[2],0xFF0000FF);
    assert(upload_texture(r)==0 && r->upload_serial==2); /* dirty was clear */
    uint32_t actual[16]; glBindTexture(GL_TEXTURE_2D,texture_gl_name(r,0));
    glGetTexImage(GL_TEXTURE_2D,0,GL_BGRA,GL_UNSIGNED_INT_8_8_8_8_REV,actual);
    assert(actual[0]==0xFF00FF00 && actual[1]==0xFFFF0000);
    glGetTexImage(GL_TEXTURE_2D,2,GL_BGRA,GL_UNSIGNED_INT_8_8_8_8_REV,actual);
    assert(actual[0]==0xFF0000FF);
    uint32_t off[]={0,0},release[]={handle};call(0xFFC90,off,2);call(0x103AD0,release,1);
    assert(!resource(handle) && s_texture_snapshot_bytes==baseline);

    uint32_t cube[]={4,0,0,6,0,0xB200};call(0xFE9F0,cube,6);assert(g_eax==0);
    handle=read32(0xB200); r=resource(handle);bind[1]=handle;
    call(0xFFC90,bind,2);assert(g_eax==0 && r->upload_serial==1);
    call(0xFFC90,bind,2);assert(r->upload_serial==1);
    write32(r->data+5*r->face_stride+r->offsets[2],0xFFAABBCC);
    assert(upload_cube_texture(r)==0 && r->upload_serial==2);
    glBindTexture(GL_TEXTURE_CUBE_MAP,r->cube_gl);
    glGetTexImage(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,2,GL_BGRA,GL_UNSIGNED_INT_8_8_8_8_REV,actual);
    assert(actual[0]==0xFFAABBCC);
    release[0]=handle;call(0xFFC90,off,2);call(0x103AD0,release,1);
    assert(s_texture_snapshot_bytes==baseline);

    uint32_t pal[2];
    for(unsigned stage=0;stage<2;++stage) {
        uint32_t cp[]={3,0xB200};call(0x100E30,cp,2);assert(g_eax==0);pal[stage]=read32(0xB200);
        struct Resource *p=resource(pal[stage]);write32(p->data,0xFFFF0000);
        write32(p->data+4,stage?0xFF112233:0xFF00FF00);
        uint32_t bp[]={stage,pal[stage]};call(0xFFE10,bp,2);
    }
    create[2]=1;create[4]=0x0B;call(0xFE9C0,create,7);assert(g_eax==0);
    handle=read32(0xB200);r=resource(handle);
    for(unsigned stage=0;stage<2;++stage) {
        bind[0]=stage;bind[1]=handle;call(0xFFC90,bind,2);
        assert(upload_texture_stage(r,stage)==0);
    }
    uint64_t serial=r->upload_serial;assert(serial==2);
    for(unsigned stage=0;stage<2;++stage) {
        bind[0]=stage;call(0xFFC90,bind,2);assert(upload_texture_stage(r,stage)==0);
    }
    assert(r->upload_serial==serial);
    *(uint8_t *)guest_ptr(r->data)=1; /* no Lock, index revision must propagate */
    for(unsigned stage=0;stage<2;++stage) {
        assert(upload_texture_stage(r,stage)==0);
        glBindTexture(GL_TEXTURE_2D,texture_gl_name(r,stage));
        glGetTexImage(GL_TEXTURE_2D,0,GL_BGRA,GL_UNSIGNED_INT_8_8_8_8_REV,actual);
        assert(actual[0]==(stage?0xFF112233:0xFF00FF00));
    }
    assert(r->upload_serial==serial+2);serial=r->upload_serial;
    write32(resource(pal[0])->data+4,0xFF0000FF); /* no palette Lock either */
    assert(upload_texture_stage(r,0)==0 && r->upload_serial==serial+1);
    assert(upload_texture_stage(r,1)==0 && r->upload_serial==serial+1);
    glBindTexture(GL_TEXTURE_2D,texture_gl_name(r,0));
    glGetTexImage(GL_TEXTURE_2D,0,GL_BGRA,GL_UNSIGNED_INT_8_8_8_8_REV,actual);assert(actual[0]==0xFF0000FF);
    for(unsigned stage=0;stage<2;++stage) {
        off[0]=stage;call(0xFFC90,off,2);call(0xFFE10,off,2);
        release[0]=pal[stage];call(0x103AD0,release,1);
    }
    release[0]=handle;call(0x103AD0,release,1);assert(s_texture_snapshot_bytes==baseline);

    /* A full shadow budget changes performance only, never upload correctness. */
    create[4]=6;call(0xFE9C0,create,7);assert(g_eax==0);
    handle=read32(0xB200);r=resource(handle);
    s_texture_snapshot_bytes=TEXTURE_SNAPSHOT_BUDGET;
    assert(upload_texture(r)==0 && !r->encoded_snapshot);
    write32(r->data,0xFFABCDEF);assert(upload_texture(r)==0 && r->upload_serial==2);
    glBindTexture(GL_TEXTURE_2D,texture_gl_name(r,0));
    glGetTexImage(GL_TEXTURE_2D,0,GL_BGRA,GL_UNSIGNED_INT_8_8_8_8_REV,actual);assert(actual[0]==0xFFABCDEF);
    s_texture_snapshot_bytes=baseline;release[0]=handle;call(0x103AD0,release,1);
    assert(s_texture_snapshot_bytes==baseline && glGetError()==GL_NO_ERROR);
    puts("PASS: exact texture snapshots, unchanged-bind reuse, direct CPU writes, mips/cube/P8 variants, release and budget fallback");
}
