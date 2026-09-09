/* Real OpenGL output-size regression; no game data or guest CPU code. */
#include "d3d/d3d8_xbox.h"
#include "presentation_filter.h"
#include <SDL.h>
#include <epoxy/gl.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef WRATH_PRESENTATION_MENU_TEST
extern int xbox_MacDisplayMenuTestActivate(int);
extern int xbox_MacDisplayMenuTestChecked(int);
#endif
extern int xbox_D3D8GLAcquire(void);
extern void xbox_D3D8GLRelease(void);
extern void xbox_D3D8GLPumpEvents(void);
extern GLuint xbox_D3D8GLBackBuffer(unsigned index);
extern void xbox_D3D8GLPresentImage(void);
extern int xbox_D3D8GLSetPresentationFilter(const char *,float);

static void pixel(int x,int y,unsigned r,unsigned green,unsigned b)
{
    unsigned char p[4]; glReadPixels(x,y,1,1,GL_RGBA,GL_UNSIGNED_BYTE,p);
    if(p[0]!=r || p[1]!=green || p[2]!=b)fprintf(stderr,"pixel%d,%d actual%u,%u,%u expected%u,%u,%u\n",x,y,p[0],p[1],p[2],r,green,b);
    assert(p[0]==r && p[1]==green && p[2]==b);
}
static void presented(IDirect3DDevice8 *device,SDL_Window *window)
{
    GLint read_fbo,read_buffer,draw_fbo,draw_buffer,actual;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read_fbo);glGetIntegerv(GL_READ_BUFFER,&read_buffer);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw_fbo);glGetIntegerv(GL_DRAW_BUFFER,&draw_buffer);
    assert(!device->lpVtbl->Present(device,NULL,NULL,NULL,NULL));
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&actual);assert(actual==draw_fbo);
    glGetIntegerv(GL_DRAW_BUFFER,&actual);assert(actual==draw_buffer);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&actual);assert(actual==read_fbo);
    glGetIntegerv(GL_READ_BUFFER,&actual);assert(actual==read_buffer);
    int dw,dh;SDL_GL_GetDrawableSize(window,&dw,&dh);assert(dw>0 && dh>0);
    int width=dw,height=dw*3/4;if(height>dh){height=dh;width=dh*4/3;}
    int x=(dw-width)/2,y=(dh-height)/2;
    glBindFramebuffer(GL_READ_FRAMEBUFFER,0);glReadBuffer(GL_FRONT);
    pixel(x+width/4,y+height/4,255,0,0);pixel(x+3*width/4,y+3*height/4,0,255,0);
    if(x>2)pixel(0,dh/2,0,0,0);
    if(y>2)pixel(dw/2,0,0,0,0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,read_fbo);glReadBuffer(read_buffer);
    assert(glGetError()==GL_NO_ERROR);
}
static void resized(IDirect3DDevice8 *device,SDL_Window *window,int w,int h)
{
    SDL_SetWindowSize(window,w,h);
    SDL_Delay(30); xbox_D3D8GLPumpEvents();
    int dw,dh; SDL_GL_GetDrawableSize(window,&dw,&dh); assert(dw>0 && dh>0);
    glBindFramebuffer(GL_FRAMEBUFFER,xbox_D3D8GLBackBuffer(0));
    glViewport(7,9,31,25); glEnable(GL_SCISSOR_TEST); glScissor(1,2,3,4);
    glColorMask(GL_FALSE,GL_TRUE,GL_FALSE,GL_TRUE); glClearColor(.25,.5,.75,.125);
    const GLenum enabled[]={GL_BLEND,GL_DEPTH_TEST,GL_STENCIL_TEST,GL_CULL_FACE,GL_RASTERIZER_DISCARD,GL_FRAMEBUFFER_SRGB};
    for(unsigned i=0;i<sizeof(enabled)/sizeof(*enabled);++i)glEnable(enabled[i]);
    glBlendFunc(GL_ZERO,GL_ZERO);glDepthFunc(GL_NEVER);glStencilFunc(GL_NEVER,0,~0u);glCullFace(GL_FRONT_AND_BACK);
    glActiveTexture(GL_TEXTURE3);
    xbox_D3D8GLPresentImage();
    for(unsigned i=0;i<sizeof(enabled)/sizeof(*enabled);++i)assert(glIsEnabled(enabled[i]));
    GLint active;glGetIntegerv(GL_ACTIVE_TEXTURE,&active);assert(active==GL_TEXTURE3);
    GLint fbo,viewport[4]; GLboolean mask[4]; GLfloat clear[4];
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&fbo);assert((GLuint)fbo==xbox_D3D8GLBackBuffer(0));
    glGetIntegerv(GL_VIEWPORT,viewport); assert(viewport[0]==7 && viewport[1]==9 && viewport[2]==31 && viewport[3]==25);
    glGetBooleanv(GL_COLOR_WRITEMASK,mask);assert(!mask[0] && mask[1] && !mask[2] && mask[3]);
    glGetFloatv(GL_COLOR_CLEAR_VALUE,clear);assert(clear[0]==.25 && clear[1]==.5 && clear[2]==.75 && clear[3]==.125);
    assert(glIsEnabled(GL_SCISSOR_TEST));
    glBindFramebuffer(GL_READ_FRAMEBUFFER,0);glReadBuffer(GL_BACK);
    int width=dw,height=dw*3/4; if(height>dh){height=dh;width=dh*4/3;}
    int x=(dw-width)/2,y=(dh-height)/2;
    pixel(x+width/4,y+height/4,255,0,0);pixel(x+3*width/4,y+3*height/4,0,255,0);
    if(x>2)pixel(0,dh/2,0,0,0);
    if(y>2)pixel(dw/2,0,0,0,0);
    /* Internal front/back pixels and depth keep their game coordinates. */
    for(unsigned index=0;index<2;++index){
        glBindFramebuffer(GL_READ_FRAMEBUFFER,xbox_D3D8GLBackBuffer(index));glReadBuffer(GL_COLOR_ATTACHMENT0);
        pixel(16,12,255,0,0);pixel(48,36,0,255,0);
    }
    glBindFramebuffer(GL_READ_FRAMEBUFFER,xbox_D3D8GLBackBuffer(0));
    float depth;glReadPixels(16,12,1,1,GL_DEPTH_COMPONENT,GL_FLOAT,&depth);
    assert(depth>.249 && depth<.251);
    assert(glGetError()==GL_NO_ERROR);
    presented(device,window);
}
int main(void)
{
    assert(xbox_D3D8GLAcquire());
    D3DPRESENT_PARAMETERS pp={0};pp.BackBufferWidth=64;pp.BackBufferHeight=48;pp.Windowed=1;
    IDirect3D8 *factory=xbox_Direct3DCreate8(4361);IDirect3DDevice8 *device=NULL;
    assert(!factory->lpVtbl->CreateDevice(factory,0,0,NULL,0,&pp,&device));
    SDL_Window *window=SDL_GL_GetCurrentWindow();assert(window);
    assert(SDL_GetWindowFlags(window)&SDL_WINDOW_RESIZABLE);
    assert(xbox_D3D8GLBackBuffer(0) && xbox_D3D8GLBackBuffer(1));
    glDisable(GL_SCISSOR_TEST);glClearColor(1,0,0,1);glClearDepth(.25);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glEnable(GL_SCISSOR_TEST);glScissor(32,24,32,24);glClearColor(0,1,0,1);glClear(GL_COLOR_BUFFER_BIT);
    resized(device,window,320,180); resized(device,window,160,240); resized(device,window,400,300);
    /* Same public keyboard event path as F11; repeat keydowns must not toggle. */
    SDL_Event key={0};key.type=SDL_KEYDOWN;key.key.windowID=SDL_GetWindowID(window);key.key.keysym.sym=SDLK_F11;
    assert(SDL_PushEvent(&key)==1);SDL_Delay(20);xbox_D3D8GLPumpEvents();
    assert(SDL_GetWindowFlags(window)&SDL_WINDOW_FULLSCREEN_DESKTOP);
    presented(device,window);
    key.key.repeat=1;assert(SDL_PushEvent(&key)==1);SDL_Delay(20);xbox_D3D8GLPumpEvents();
    assert(SDL_GetWindowFlags(window)&SDL_WINDOW_FULLSCREEN_DESKTOP);
    presented(device,window);
    key.key.repeat=0;assert(SDL_PushEvent(&key)==1);SDL_Delay(20);xbox_D3D8GLPumpEvents();
    assert(!(SDL_GetWindowFlags(window)&SDL_WINDOW_FULLSCREEN_DESKTOP));
    presented(device,window);
    resized(device,window,320,240);
    assert(xbox_D3D8GLSetPresentationFilter(wrath_presentation_filter_glsl,0));
    key.key.keysym.sym=SDLK_F10;
    assert(SDL_PushEvent(&key)==1);SDL_Delay(20);xbox_D3D8GLPumpEvents();
    presented(device,window);
    resized(device,window,1280,720);resized(device,window,1920,1080);resized(device,window,2560,1440);
    assert(SDL_PushEvent(&key)==1);SDL_Delay(20);xbox_D3D8GLPumpEvents();
    presented(device,window);
    resized(device,window,320,240);
#ifdef WRATH_PRESENTATION_MENU_TEST
    /* Native AppKit target/actions queue commands; SDL/Cocoa consumes them on
     * the same locked main-thread path as the real game. */
    const int presets[]={720,1080,1440};
    for(unsigned i=0;i<3;++i){
        assert(xbox_MacDisplayMenuTestActivate(presets[i]));
        SDL_Delay(30);xbox_D3D8GLPumpEvents();SDL_Delay(30);xbox_D3D8GLPumpEvents();
        int lw,lh,dw,dh;SDL_GetWindowSize(window,&lw,&lh);SDL_GL_GetDrawableSize(window,&dw,&dh);
        printf("PRESET %dp logical=%dx%d drawable=%dx%d\n",presets[i],lw,lh,dw,dh);
        assert(dw==presets[i]*16/9 && dh==presets[i]);
        assert(xbox_MacDisplayMenuTestChecked(presets[i]));
        char expected[64];snprintf(expected,sizeof(expected),"%d × %d",dw,dh);assert(strstr(SDL_GetWindowTitle(window),expected));
        presented(device,window);
    }
    const int choices[]={1025,1050,1075,1000};const char *names[]={"Light","Medium","Strong","Off"};
    for(unsigned i=0;i<4;++i){
        assert(xbox_MacDisplayMenuTestActivate(choices[i]));SDL_Delay(30);xbox_D3D8GLPumpEvents();
        assert(xbox_MacDisplayMenuTestChecked(choices[i]) && strstr(SDL_GetWindowTitle(window),names[i]));presented(device,window);
    }
    /* F10 remembers the last chosen strength, while its visible state agrees. */
    key.key.keysym.sym=SDLK_F10;assert(SDL_PushEvent(&key)==1);SDL_Delay(30);xbox_D3D8GLPumpEvents();
    assert(xbox_MacDisplayMenuTestChecked(1075) && strstr(SDL_GetWindowTitle(window),"Strong"));
    assert(xbox_MacDisplayMenuTestActivate(1));SDL_Delay(30);xbox_D3D8GLPumpEvents();
    assert(SDL_GetWindowFlags(window)&SDL_WINDOW_FULLSCREEN_DESKTOP);assert(xbox_MacDisplayMenuTestChecked(1));presented(device,window);
    assert(xbox_MacDisplayMenuTestActivate(1440));SDL_Delay(30);xbox_D3D8GLPumpEvents();SDL_Delay(30);xbox_D3D8GLPumpEvents();
    assert(!(SDL_GetWindowFlags(window)&SDL_WINDOW_FULLSCREEN_DESKTOP));assert(xbox_MacDisplayMenuTestChecked(1440));presented(device,window);
    puts("PASS native Display menu actions, exact Retina720/1080/1440 pixels, visible sharpening levels/checks, fullscreen exit and displayed4:3 pixels");
#endif
    xbox_D3D8GLRelease();SDL_Quit();
    puts("PASS native resize/fullscreen/filter transitions with displayed front pixels/letterbox and preserved internal color/depth/GL state");
}
