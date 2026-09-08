/* Original flag-controlled texture-mode adjustment and native sampler cache. */
static void test_inactive_sampler(const uint32_t *draw,const uint32_t *setps,
                                  const Nv2aPixelDef *original,uint32_t texture)
{
    Nv2aPixelDef definition=*original;
    definition.final_constants|=0x100;
    definition.texture_modes|=1u<<5; /* Declared t1, never referenced. */
    uint32_t bind[]={1,0};call(0xFFC90,bind,2);
    memcpy(guest_ptr(0xE00C),&definition,sizeof(definition));call(0x102BB0,setps,1);
    Nv2aPixelDef adjusted=definition;shader_adjust_texture_modes(&adjusted);
    assert(adjusted.texture_modes==1 && definition.texture_modes==0x21);
    struct ShaderProgram *program=shader_program(&adjusted);
    assert(program->pixel.texture_mask==1 && program->sampler_location[0]>=0 && program->sampler_location[1]==-1);
    call(0x1019C0,draw,4);assert(g_eax==0);
    uint8_t pixel[4];glReadPixels(60,180,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
    assert(pixel[0]==0x40 && pixel[1]==0xA0 && pixel[2]==0x20 && pixel[3]==255);
    /* Flag-clear mode retains its live sampler and production resource check;
     * the flagged variant follows original null/bound mode transitions. */
    definition.final_abcd=9;definition.final_efg=0x1c80;
    memcpy(guest_ptr(0xE00C),&definition,sizeof(definition));call(0x102BB0,setps,1);
    adjusted=definition;adjusted.final_constants&=~0x100u;shader_adjust_texture_modes(&adjusted);
    assert(adjusted.texture_modes==0x21);
    program=shader_program(&adjusted);assert(program->sampler_location[1]>=0);
    bind[1]=texture;call(0xFFC90,bind,2);call(0x1019C0,draw,4);assert(g_eax==0);
    glReadPixels(60,180,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
    assert(pixel[0]==0x40 && pixel[1]==0xA0 && pixel[2]==0x20);
    bind[1]=0;call(0xFFC90,bind,2);call(0x1019C0,draw,4);assert(g_eax==0);
    glReadPixels(60,180,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
    assert(pixel[0]==0 && pixel[1]==0 && pixel[2]==0 && pixel[3]==255);
    /* NONE alpha is one even when explicitly sourced by the final combiner. */
    definition.final_efg=0x1980;memcpy(guest_ptr(0xE00C),&definition,sizeof(definition));call(0x102BB0,setps,1);
    call(0x1019C0,draw,4);glReadPixels(60,180,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);assert(pixel[3]==255);
    adjusted=definition;adjusted.texture_modes=4u<<5;shader_adjust_texture_modes(&adjusted);assert(adjusted.texture_modes==(4u<<5));
    adjusted.texture_modes=5u<<5;shader_adjust_texture_modes(&adjusted);assert(adjusted.texture_modes==(5u<<5));
    memcpy(guest_ptr(0xE00C),original,sizeof(*original));call(0x102BB0,setps,1);
    assert(glGetError()==GL_NO_ERROR);
    puts("PASS: original flagged texture-mode adjustment, null NONE black/opaque registers, flag-clear preservation, live binding/rebinding, cached native sampler locations");
}
