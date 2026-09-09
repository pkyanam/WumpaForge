/* Actual shared D24S8 contents, with differently sized top-down color views. */
static void texture_depth_pixel(unsigned x,unsigned y,float depth,unsigned stencil)
{
    float actual=0;unsigned char value=0;
    glReadPixels(x,y,1,1,GL_DEPTH_COMPONENT,GL_FLOAT,&actual);
    glReadPixels(x,y,1,1,GL_STENCIL_INDEX,GL_UNSIGNED_BYTE,&value);
    assert(fabsf(actual-depth)<0.00001f && value==stencil);
}
static uint32_t texture_depth_surface(unsigned w,unsigned h,unsigned format,uint32_t *texture)
{
    uint32_t create[]={w,h,1,1,format,0,0x1E000};call(0xFE9C0,create,7);assert(g_eax==0);
    *texture=read32(0x1E000);
    uint32_t get[]={*texture,0,0x1E004};call(0x103C30,get,3);assert(g_eax==0);return read32(0x1E004);
}
static void test_texture_depth(void)
{
    uint32_t depth=s_depthbuffer_handle,target[]={s_backbuffer_handle,depth};
    call(0xFEF20,target,2);assert(g_eax==0);
    unsigned depth_bindings=resource(depth)->bindings;
    glDisable(GL_SCISSOR_TEST);glDepthMask(GL_TRUE);glStencilMask(255);
    glClearDepth(.8);glClearStencil(3);glClear(GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
    glEnable(GL_SCISSOR_TEST);glScissor(0,s_height-16,16,16);
    glClearDepth(.25);glClearStencil(7);glClear(GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
    uint32_t texture,surface=texture_depth_surface(16,16,6,&texture);
    target[0]=surface;call(0xFEF20,target,2);assert(g_eax==0 && glIsEnabled(GL_SCISSOR_TEST));
    texture_depth_pixel(0,0,.25,7);texture_depth_pixel(15,15,.25,7);
    /* Render with real depth/stencil tests, not merely attachment inspection. */
    const char *vs="#version 410 core\nuniform float depth;void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2-1,depth*2-1,1);}";
    const char *fs="#version 410 core\nuniform vec4 color;out vec4 c;void main(){c=color;}";
    GLuint shaders[2]={glCreateShader(GL_VERTEX_SHADER),glCreateShader(GL_FRAGMENT_SHADER)};
    glShaderSource(shaders[0],1,&vs,NULL);glShaderSource(shaders[1],1,&fs,NULL);
    for(unsigned i=0;i<2;++i){GLint ok;glCompileShader(shaders[i]);glGetShaderiv(shaders[i],GL_COMPILE_STATUS,&ok);assert(ok);}
    GLuint program=glCreateProgram(),vao;glAttachShader(program,shaders[0]);glAttachShader(program,shaders[1]);glLinkProgram(program);
    GLint ok;glGetProgramiv(program,GL_LINK_STATUS,&ok);assert(ok);glGenVertexArrays(1,&vao);glBindVertexArray(vao);glUseProgram(program);
    glDisable(GL_SCISSOR_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glColorMask(1,1,1,1);glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
    glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LESS);glEnable(GL_STENCIL_TEST);glStencilFunc(GL_EQUAL,7,255);glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP);
    glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
    glUniform4f(glGetUniformLocation(program,"color"),1,0,0,1);glUniform1f(glGetUniformLocation(program,"depth"),.5);glDrawArrays(GL_TRIANGLES,0,3);
    unsigned char rgba[4];glReadPixels(8,8,1,1,GL_RGBA,GL_UNSIGNED_BYTE,rgba);assert(rgba[0]==0);
    glUniform4f(glGetUniformLocation(program,"color"),0,1,0,1);glUniform1f(glGetUniformLocation(program,"depth"),.1);glDrawArrays(GL_TRIANGLES,0,3);
    glReadPixels(8,8,1,1,GL_RGBA,GL_UNSIGNED_BYTE,rgba);assert(rgba[0]==0&&rgba[1]==255);
    glStencilFunc(GL_ALWAYS,9,255);glStencilOp(GL_KEEP,GL_KEEP,GL_REPLACE);glDepthFunc(GL_ALWAYS);glDrawArrays(GL_TRIANGLES,0,3);
    texture_depth_pixel(8,8,.1,9);
    /* NULL on the same color flushes the view, detaches both planes, and a
     * later rebind restores the original depth contents despite intervening clears. */
    target[0]=0;target[1]=0;call(0xFEF20,target,2);assert(g_eax==0);
    GLint attached;glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,&attached);assert(!attached);
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,GL_STENCIL_ATTACHMENT,GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,&attached);assert(!attached);
    glClearDepth(.99);glClearStencil(44);glClear(GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
    target[1]=depth;call(0xFEF20,target,2);assert(g_eax==0);texture_depth_pixel(8,8,.1,9);
    /* Foreign GL failure must leave the logical and actual target unchanged. */
    GLint before;glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&before);
    glEnable(0xFFFFFFFFu);target[0]=s_backbuffer_handle;call(0xFEF20,target,2);assert(g_eax==(uint32_t)D3DERR_INVALIDCALL);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&attached);assert(attached==before && read32(GUEST_DEVICE+0x2070)==surface && read32(GUEST_DEVICE+0x2074)==depth);
    call(0xFEF20,target,2);assert(g_eax==0);
    texture_depth_pixel(8,s_height-8,.1,9);texture_depth_pixel(20,s_height-8,.8,3);texture_depth_pixel(8,8,.8,3);
    /* Another wrapper aliases the same color bytes; switching resolves and
     * reloads those bytes, and shares the actual modified depth region. */
    uint32_t get[]={texture,0,0x1E008};call(0x103C30,get,3);assert(g_eax==0);uint32_t alias=read32(0x1E008);
    target[0]=alias;call(0xFEF20,target,2);assert(g_eax==0);texture_depth_pixel(8,8,.1,9);
    glReadPixels(8,8,1,1,GL_RGBA,GL_UNSIGNED_BYTE,rgba);assert(rgba[1]==255);
    target[0]=s_backbuffer_handle;call(0xFEF20,target,2);assert(g_eax==0);
    uint32_t full_texture,full=texture_depth_surface(s_width,s_height,0x12,&full_texture);
    target[0]=full;call(0xFEF20,target,2);assert(g_eax==0 && !resource(full)->target_depth);
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,&attached);assert((GLuint)attached==xbox_D3D8GLDepthBuffer());
    texture_depth_pixel(8,s_height-8,.1,9);texture_depth_pixel(8,8,.8,3);
    unsigned width=resource(surface)->width;resource(surface)->width=s_width+1;
    target[0]=surface;call(0xFEF20,target,2);assert(g_eax==(uint32_t)D3DERR_INVALIDCALL && read32(GUEST_DEVICE+0x2070)==full);resource(surface)->width=width;
    unsigned format=resource(depth)->format;resource(depth)->format=0x2B;
    call(0xFEF20,target,2);assert(g_eax==(uint32_t)D3DERR_INVALIDCALL && read32(GUEST_DEVICE+0x2070)==full);resource(depth)->format=format;
    target[0]=s_backbuffer_handle;call(0xFEF20,target,2);assert(g_eax==0 && resource(depth)->bindings==depth_bindings);
    uint32_t release[]={surface};call(0x103AD0,release,1);release[0]=alias;call(0x103AD0,release,1);release[0]=texture;call(0x103AD0,release,1);
    release[0]=full;call(0x103AD0,release,1);release[0]=full_texture;call(0x103AD0,release,1);
    assert(!resource(surface)&&!resource(alias)&&!resource(texture)&&!resource(full)&&!resource(full_texture));
    glUseProgram(0);glBindVertexArray(0);glDeleteVertexArrays(1,&vao);glDeleteProgram(program);glDeleteShader(shaders[0]);glDeleteShader(shaders[1]);
    glDisable(GL_STENCIL_TEST);glDisable(GL_DEPTH_TEST);assert(glGetError()==GL_NO_ERROR);
    puts("PASS: shared D24S8 texture target, top-left depth/stencil, real occlusion, outside preservation, NULL/rebind, alias resolve, exact-size sharing, GL rollback and lifetime");
}
