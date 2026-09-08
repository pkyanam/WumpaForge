/* Actual production fixed shader/GL upload regression, no game assets.
 * Direct3D row-vector convention and clip depth:
 * https://learn.microsoft.com/en-us/windows/win32/dxtecharts/the-direct3d-transformation-pipeline
 * GL_FALSE consumes column-major bytes:
 * https://wikis.khronos.org/opengl/GLAPI/glUniform
 * Include production C to capture its exact shader output without test hooks. */
#include "../../third_party/xboxrecomp/src/d3d/d3d8_gl.c"
#include <assert.h>

static void near(float actual,float expected)
{
    if(fabsf(actual-expected)>0.00002f){fprintf(stderr,"actual%g expected%g\n",actual,expected);abort();}
}
static void capture(GLuint program,GLuint feedback,DWORD fvf,const float pos[4],const float expected[4])
{
    glUseProgram(program);update_mvp(fvf);
    glDisableVertexAttribArray(0);glVertexAttrib4fv(0,pos);
    glEnable(GL_RASTERIZER_DISCARD);glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,feedback);
    glBeginTransformFeedback(GL_POINTS);glDrawArrays(GL_POINTS,0,1);glEndTransformFeedback();
    glDisable(GL_RASTERIZER_DISCARD);
    float actual[4];glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER,0,sizeof(actual),actual);
    for(unsigned i=0;i<4;++i)near(actual[i],expected[i]);
    assert(glGetError()==GL_NO_ERROR);
}
int main(void)
{
    assert(xbox_D3D8GLAcquire());
    D3DPRESENT_PARAMETERS pp={0};pp.BackBufferWidth=32;pp.BackBufferHeight=32;pp.Windowed=1;
    IDirect3DDevice8 *device=NULL;IDirect3D8 *factory=xbox_Direct3DCreate8(4361);
    assert(factory->lpVtbl->CreateDevice(factory,0,0,NULL,0,&pp,&device)==0);
    SDL_HideWindow(g.window);
    /* Scale then translation, rotation then translation, LH perspective n1/f11.
     * For p=(x,y,z,1), independent scalar oracle:
     * world=(2x+1,y+2,z+3);view=(-y-4,2x+2,z+4);
     * D3Dclip=(-2y-8,6x+6,1.1z+3.3,z+4). */
    D3DMATRIX world={.m={{2,0,0,0},{0,1,0,0},{0,0,1,0},{1,2,3,1}}};
    D3DMATRIX view={.m={{0,1,0,0},{-1,0,0,0},{0,0,1,0},{-2,1,1,1}}};
    D3DMATRIX projection={.m={{2,0,0,0},{0,3,0,0},{0,0,1.1f,1},{0,0,-1.1f,0}}};
    assert(device->lpVtbl->SetTransform(device,D3DTS_WORLD,&world)==0);
    assert(device->lpVtbl->SetTransform(device,D3DTS_VIEW,&view)==0);
    assert(device->lpVtbl->SetTransform(device,D3DTS_PROJECTION,&projection)==0);
    GLuint vs=compile_shader(GL_VERTEX_SHADER,VS_SRC),program=glCreateProgram();assert(vs);
    glAttachShader(program,vs);const char *varying="gl_Position";
    glTransformFeedbackVaryings(program,1,&varying,GL_INTERLEAVED_ATTRIBS);glLinkProgram(program);
    GLint linked;glGetProgramiv(program,GL_LINK_STATUS,&linked);assert(linked);
    GLint original_mvp=g.u_mvp,original_use=g.u_use_xform;
    g.u_mvp=glGetUniformLocation(program,"u_mvp");g.u_use_xform=glGetUniformLocation(program,"u_use_xform");
    GLuint feedback;glGenBuffers(1,&feedback);glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,feedback);
    glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER,16,NULL,GL_STREAM_READ);
    const float inputs[3][4]={{.25f,-.5f,1,1},{-1,-4,2,1},{2,3,-2,1}};
    for(unsigned i=0;i<3;++i){const float *p=inputs[i];float expected[]={-2*p[1]-8,6*p[0]+6,1.2f*p[2]+2.6f,p[2]+4};capture(program,feedback,D3DFVF_XYZ,p,expected);}
    const float preconverted[]={.2f,-.4f,-.7f,2};
    capture(program,feedback,D3DFVF_XYZRHW,preconverted,preconverted);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,0);glDeleteBuffers(1,&feedback);
    glDeleteProgram(program);glDeleteShader(vs);g.u_mvp=original_mvp;g.u_use_xform=original_use;
    /* Read actual depth from the production draw path, using an explicit FBO. */
    GLuint fbo,color,depth;glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
    glGenTextures(1,&color);glBindTexture(GL_TEXTURE_2D,color);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,32,32,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
    glGenRenderbuffers(1,&depth);glBindRenderbuffer(GL_RENDERBUFFER,depth);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,32,32);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
    D3DVIEWPORT8 vp={0,0,32,32,0,1};device->lpVtbl->SetViewport(device,&vp);
    device->lpVtbl->SetRenderState(device,D3DRS_CULLMODE,D3DCULL_NONE);
    device->lpVtbl->SetRenderState(device,D3DRS_ZENABLE,1);
    device->lpVtbl->SetRenderState(device,D3DRS_ZWRITEENABLE,1);
    device->lpVtbl->SetRenderState(device,D3DRS_ZFUNC,D3DCMP_ALWAYS);
    struct {float x,y,z;uint32_t color;} triangle[3]={{-1.5f,-3,2,0xffff0000},{-.5f,-3,2,0xffff0000},{-1,-5,2,0xffff0000}};
    device->lpVtbl->SetVertexShader(device,D3DFVF_XYZ|D3DFVF_DIFFUSE);
    assert(device->lpVtbl->DrawPrimitiveUP(device,D3DPT_TRIANGLELIST,1,triangle,sizeof(triangle[0]))==0);
    uint8_t pixel[4];float z;glReadPixels(16,16,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
    assert(pixel[0]==255 && pixel[1]==0 && pixel[2]==0);glReadPixels(16,16,1,1,GL_DEPTH_COMPONENT,GL_FLOAT,&z);near(z,11.0f/12.0f);
    vp.MinZ=.2f;vp.MaxZ=.8f;device->lpVtbl->SetViewport(device,&vp);
    device->lpVtbl->DrawPrimitiveUP(device,D3DPT_TRIANGLELIST,1,triangle,sizeof(triangle[0]));
    glReadPixels(16,16,1,1,GL_DEPTH_COMPONENT,GL_FLOAT,&z);near(z,.75f);
    assert(glGetError()==GL_NO_ERROR);
    glBindFramebuffer(GL_FRAMEBUFFER,0);glDeleteFramebuffers(1,&fbo);glDeleteTextures(1,&color);glDeleteRenderbuffers(1,&depth);
    xbox_D3D8GLRelease();SDL_Quit();
    puts("PASS: native fixed MVP noncommuting W/V/P transform feedback, perspective depth+viewport readbacks, unchanged XYZRHW");
}
