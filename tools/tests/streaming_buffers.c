/* Ordered actual GPU draws sharing the production UP streaming buffers. */
#include "../../third_party/xboxrecomp/src/d3d/d3d8_gl.c"
#include <assert.h>
struct Vertex { float x,y,z,w; uint32_t color; };
static uint32_t tile_color(unsigned tile,unsigned pass)
{ return 0xFF000000u|(((tile*17+pass*31)&255)<<16)|(((tile*29+pass*37)&255)<<8)|((tile*47+pass*41)&255); }
int main(void)
{
    assert(xbox_D3D8GLAcquire());
    D3DPRESENT_PARAMETERS pp={0};pp.BackBufferWidth=64;pp.BackBufferHeight=64;pp.Windowed=1;
    IDirect3DDevice8 *device=NULL;IDirect3D8 *factory=xbox_Direct3DCreate8(4361);
    assert(factory->lpVtbl->CreateDevice(factory,0,0,NULL,0,&pp,&device)==0);
    SDL_HideWindow(g.window);
    GLuint fbo,color;glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
    glGenTextures(1,&color);glBindTexture(GL_TEXTURE_2D,color);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,64,64,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
    D3DVIEWPORT8 vp={0,0,64,64,0,1};device->lpVtbl->SetViewport(device,&vp);
    device->lpVtbl->SetRenderState(device,D3DRS_CULLMODE,D3DCULL_NONE);
    device->lpVtbl->SetRenderState(device,D3DRS_ZENABLE,0);
    device->lpVtbl->SetRenderState(device,D3DRS_ALPHABLENDENABLE,0);
    device->lpVtbl->SetVertexShader(device,D3DFVF_XYZRHW|D3DFVF_DIFFUSE);
    uint64_t begin=monotonic_ns();
    /* 1024 draws, one final synchronization/readback. Alternate full vertex
     * and indexed payload sizes. Mutate caller storage immediately after each
     * submission; queued draws must retain their own bytes and original order. */
    for(unsigned pass=0;pass<4;++pass) for(unsigned t=0;t<256;++t) {
        unsigned tile=pass&1?255-t:t,x=tile%16,y=tile/16;
        float x0=x/8.0f-1,x1=(x+1)/8.0f-1,y0=y/8.0f-1,y1=(y+1)/8.0f-1;
        uint32_t c=tile_color(tile,pass);
        struct Vertex v[6]={{x0,y0,0,1,c},{x1,y0,0,1,c},{x0,y1,0,1,c},{x1,y1,0,1,c}};
        uint16_t indices[6]={0,1,2,2,1,3};
        if(t&1) assert(device->lpVtbl->DrawIndexedPrimitiveUP(device,D3DPT_TRIANGLELIST,0,4,2,indices,D3DFMT_INDEX16,v,sizeof(v[0]))==0);
        else {v[5]=v[3];v[3]=v[2];v[4]=v[1];assert(device->lpVtbl->DrawPrimitiveUP(device,D3DPT_TRIANGLELIST,2,v,sizeof(v[0]))==0);}
        memset(v,0,sizeof(v));memset(indices,0,sizeof(indices));
    }
    uint32_t pixels[64*64];glReadPixels(0,0,64,64,GL_BGRA,GL_UNSIGNED_INT_8_8_8_8_REV,pixels);
    for(unsigned tile=0;tile<256;++tile) assert(pixels[(tile/16*4+2)*64+tile%16*4+2]==tile_color(tile,3));
    assert(glGetError()==GL_NO_ERROR);
    printf("PASS: 1024 ordered UP/indexed GPU draws, 256 distinct final pixels, mutable caller storage; %.3fms including final readback\n",(monotonic_ns()-begin)/1e6);
    glDeleteTextures(1,&color);glDeleteFramebuffers(1,&fbo);xbox_D3D8GLRelease();SDL_Quit();return 0;
}
