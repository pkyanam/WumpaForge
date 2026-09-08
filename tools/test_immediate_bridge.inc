static void immediate_test4(uint32_t reg,float x,float y,float z,float w)
{
    uint32_t args[5]={reg}; float value[4]={x,y,z,w}; memcpy(args+1,value,16);
    call(0x101E60,args,5); assert(g_eax==0);
}
static void immediate_test2(uint32_t reg,float x,float y)
{
    uint32_t args[3]={reg}; float value[2]={x,y}; memcpy(args+1,value,8);
    call(0x101E20,args,3); assert(g_eax==0);
}
static void test_immediate_bridge(void)
{
    D3DMATRIX saved_matrices[3]; const unsigned transforms[3]={6,0,1};
    for(unsigned t=0;t<3;++t) {
        uint32_t address=GUEST_DEVICE+0x750+transforms[t]*64;
        memcpy(&saved_matrices[t],guest_ptr(address),64);
        for(unsigned i=0;i<16;++i)write32(address+i*4,i%5==0?0x3F800000:0);
    }
    uint32_t target[]={s_backbuffer_handle,0};call(0xFEF20,target,2);
    uint32_t off[]={0};call(0xFDAD0,off,1);call(0xFE5C0,off,1);call(0xFE660,off,1);call(0x102BB0,off,1);
    test_state(0x304,0);test_state(0x300,0);test_state(0x358,0x1010101);
    for(unsigned i=0;i<4;++i) {uint32_t bind[]={i,0};call(0xFFC90,bind,2);}
    uint32_t fvf[]={0x142};call(0x102940,fvf,1);assert(g_eax==0);
    uint32_t texture_args[]={2,2,1,0,6,0,0x18000};call(0xFE9C0,texture_args,7);assert(g_eax==0);
    uint32_t texture=read32(0x18000),data=read32(texture+4);
    const uint32_t texels[4]={0xFFFF0000,0xFF00FF00,0xFF0000FF,0xFFFFFF00};
    memcpy(guest_ptr(data),texels,sizeof(texels));uint32_t bind[]={0,texture};call(0xFFC90,bind,2);assert(g_eax==0);
    uint32_t clear[]={0,0,0xF0,0,0x3F800000,0};call(0x100EA0,clear,6);
    immediate_test4(3,1,1,1,1); /* Non-position attributes are legal before Begin. */
    uint32_t begin[]={8};call(0x101EC0,begin,1);assert(g_eax==0);
    assert(read32(GUEST_DEVICE+8)&0x800);
    immediate_test2(9,0,0);assert(s_immediate_count==0);
    immediate_test4(UINT32_MAX,-0.5f,-0.5f,0,1);
    immediate_test2(9,1,0);immediate_test4(0,0.5f,-0.5f,0,1);
    immediate_test2(9,1,1);immediate_test4(UINT32_MAX,0.5f,0.5f,0,1);
    immediate_test2(9,0,1);immediate_test4(UINT32_MAX,-0.5f,0.5f,0,1);
    assert(s_immediate_count==4 && s_immediate_vertices[0].attributes[9][0]==0 &&
           s_immediate_vertices[1].attributes[9][0]==1);
    call(0x101F00,NULL,0);assert(g_eax==0 && !(read32(GUEST_DEVICE+8)&0x1800));
    /* Near texel centers; half-pixel raster centers allow a few linear-filter
     * quantization levels of the adjacent texels. */
    uint8_t pixel[4];glReadPixels(120,90,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
    assert(pixel[0]>=250 && pixel[1]<=5 && pixel[2]<=5);
    glReadPixels(200,150,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
    assert(pixel[0]>=250 && pixel[1]>=250 && pixel[2]<=5);
    bind[1]=0;call(0xFFC90,bind,2);uint32_t release[]={texture};call(0x103AD0,release,1);
    /* Actual programmed instructions consume immediate float registers directly,
     * regardless of the unrelated stream declaration stored in the object. */
    uint32_t words[8]={
        0,(1u<<21)|0x1b,2u<<26,(15u<<12)|(1u<<11),
        0,(1u<<21)|(3u<<9)|0x1b,2u<<26,(15u<<12)|(1u<<11)|(3u<<3)|1};
    uint32_t object=0x18100;memset(guest_ptr(object),0,0x150);
    write32(object,1);write32(object+4,0x10);write32(object+8,2);write32(object+12,9);
    for(unsigned i=0;i<16;++i)write32(object+0x14+i*16+8,2);
    write32(object+0x14+4,4);write32(object+0x14+8,0x32);
    write32(object+0x14+3*16+4,20);write32(object+0x14+3*16+8,0x40);
    write32(object+0x114,(8u<<18)|0xB00);memcpy(guest_ptr(object+0x118),words,sizeof(words));
    uint32_t vs[]={object+1};call(0x102940,vs,1);assert(g_eax==0);
    Nv2aPixelDef definition={0};definition.combiner_count=1;
    definition.rgb_inputs[0]=0x04200000;definition.alpha_inputs[0]=0x14200000;
    definition.rgb_outputs[0]=0xC0;definition.alpha_outputs[0]=0xC0;
    memcpy(guest_ptr(0x1900C),&definition,sizeof(definition));write32(0x19008,0x1900C);
    uint32_t ps[]={0x19000};call(0x102BB0,ps,1);assert(g_eax==0);
    uint64_t old_age=s_shader_age;
    call(0x100EA0,clear,6);immediate_test4(3,0.25f,0.5f,0.75f,1);
    call(0x101EC0,begin,1);
    immediate_test4(UINT32_MAX,40,40,0,1);immediate_test4(UINT32_MAX,80,40,0,1);
    immediate_test4(UINT32_MAX,80,80,0,1);immediate_test4(UINT32_MAX,40,80,0,1);
    call(0x101F00,NULL,0);assert(g_eax==0);
    assert(s_vertex_object.slots[0].offset==4 && s_vertex_object.slots[3].kind==NV2A_VERTEX_BGRA8);
    glReadPixels(60,180,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
    assert(pixel[0]>=63 && pixel[0]<=64 && pixel[1]>=127 && pixel[1]<=128 && pixel[2]>=191 && pixel[2]<=192);
    call(0x102BB0,off,1);call(0x102940,fvf,1);
    for(unsigned i=0;i<SHADER_CACHE_LIMIT;++i) if(s_shader_programs[i].program && s_shader_programs[i].age>old_age) {
        glDeleteProgram(s_shader_programs[i].program);memset(&s_shader_programs[i],0,sizeof(s_shader_programs[i]));
    }
    s_shader_age=old_age;
    for(unsigned t=0;t<3;++t)memcpy(guest_ptr(GUEST_DEVICE+0x750+transforms[t]*64),&saved_matrices[t],64);
    assert(glGetError()==GL_NO_ERROR);
    puts("PASS: immediate Begin/End GPU XYZ quads/UV/color, persistent attributes, position emission, programmable float registers and declaration restoration");
}
