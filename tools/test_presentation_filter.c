/* Standalone hidden CGL GPU fixture: no game, SDL window, assets or audio. */
#define GL_SILENCE_DEPRECATION
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../src/presentation_filter.h"
#define SW 640
#define SH 480
static uint8_t source[SH][SW][4];
static GLuint shader(GLenum type,const char *text){
 GLuint s=glCreateShader(type);glShaderSource(s,1,&text,NULL);glCompileShader(s);
 GLint ok;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
 if(!ok){char log[4096];glGetShaderInfoLog(s,sizeof(log),NULL,log);fprintf(stderr,"%s\n",log);abort();}return s;
}
static float sample(float u,float v,int ch){
 float x=u*SW-.5f,y=v*SH-.5f;int x0=(int)floorf(x),y0=(int)floorf(y);
 float fx=x-x0,fy=y-y0,result=0;
 for(int j=0;j<2;++j)for(int i=0;i<2;++i){
  int a=x0+i,b=y0+j;if(a<0)a=0;if(a>=SW)a=SW-1;if(b<0)b=0;if(b>=SH)b=SH-1;
  result+=source[b][a][ch]/255.f*(i?fx:1-fx)*(j?fy:1-fy);
 }
 return result;
}
static float reference(float u,float v,int ch,float sharp){
 float c=sample(u,v,ch);if(ch==3||sharp==0)return c;
 float n=sample(u,v+1.f/SH,ch),e=sample(u+1.f/SW,v,ch);
 float w=sample(u-1.f/SW,v,ch),b=sample(u,v-1.f/SH,ch);
 float lo=fminf(c,fminf(fminf(n,e),fminf(w,b))),hi=fmaxf(c,fmaxf(fmaxf(n,e),fmaxf(w,b)));
 return fminf(hi,fmaxf(lo,c+sharp*(c-.25f*(n+e+w+b))));
}
int main(void){
 CGLPixelFormatAttribute attrs[]={kCGLPFAOpenGLProfile,(CGLPixelFormatAttribute)kCGLOGLPVersion_3_2_Core,kCGLPFAAccelerated,0};
 CGLPixelFormatObj pf;GLint n;CGLContextObj ctx;
 assert(CGLChoosePixelFormat(attrs,&pf,&n)==kCGLNoError&&n>0);
 assert(CGLCreateContext(pf,NULL,&ctx)==kCGLNoError);CGLDestroyPixelFormat(pf);
 assert(CGLSetCurrentContext(ctx)==kCGLNoError);
 printf("GPU: %s; GL %s\n",glGetString(GL_RENDERER),glGetString(GL_VERSION));
 const char *vs="#version 330 core\nout vec2 v_uv;void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);v_uv=p;gl_Position=vec4(p*2.0-1.0,0,1);}";
 GLuint v=shader(GL_VERTEX_SHADER,vs),f=shader(GL_FRAGMENT_SHADER,wrath_presentation_filter_glsl),program=glCreateProgram();
 glAttachShader(program,v);glAttachShader(program,f);glLinkProgram(program);GLint linked;glGetProgramiv(program,GL_LINK_STATUS,&linked);assert(linked);
 glUseProgram(program);glUniform1i(glGetUniformLocation(program,"u_source"),0);glUniform2f(glGetUniformLocation(program,"u_source_size"),SW,SH);
 GLint sharp=glGetUniformLocation(program,"u_sharpness");
 for(int y=0;y<SH;++y)for(int x=0;x<SW;++x){source[y][x][0]=(uint8_t)(x*255/(SW-1));source[y][x][1]=(uint8_t)(y*255/(SH-1));source[y][x][2]=(x/13+y/11)%2?210:40;source[y][x][3]=93;}
 GLuint textures[2],fb,vao;glGenTextures(2,textures);glGenFramebuffers(1,&fb);glGenVertexArrays(1,&vao);glBindVertexArray(vao);
 glBindTexture(GL_TEXTURE_2D,textures[0]);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,SW,SH,0,GL_RGBA,GL_UNSIGNED_BYTE,source);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
 glDisable(GL_BLEND);glDisable(GL_DEPTH_TEST);glDisable(GL_DITHER);glDisable(GL_FRAMEBUFFER_SRGB);
 const int sizes[][2]={{1280,720},{1920,1080},{2560,1440}};
 for(unsigned size=0;size<3;++size){
  int width=sizes[size][0],height=sizes[size][1],view=height*4/3,left=(width-view)/2;
  glBindTexture(GL_TEXTURE_2D,textures[1]);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
  glBindFramebuffer(GL_FRAMEBUFFER,fb);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,textures[1],0);assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
  glBindTexture(GL_TEXTURE_2D,textures[0]);glViewport(left,0,view,height);
  for(unsigned mode=0;mode<2;++mode){
   float strength=mode?.25f:0;glUniform1f(sharp,strength);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);glDrawArrays(GL_TRIANGLES,0,3);
   /* Sample source boundaries, sharp grid edges, interior gradients and alpha. */
   const float uv[][2]={{0,0},{1,1},{.5f,.5f},{.02f,.2f},{.8125f,.8f},{.4f,.022f}};
   for(unsigned p=0;p<6;++p){
    int x=(int)(uv[p][0]*(view-1)),y=(int)(uv[p][1]*(height-1));uint8_t actual[4];
    glReadPixels(left+x,y,1,1,GL_RGBA,GL_UNSIGNED_BYTE,actual);
    for(int ch=0;ch<4;++ch){float expected=reference((x+.5f)/view,(y+.5f)/height,ch,strength)*255;
     if(fabsf(actual[ch]-expected)>2.1f){fprintf(stderr,"size%u mode%u p%u ch%d actual%d expected%f\n",size,mode,p,ch,actual[ch],expected);abort();}}
   }
   uint8_t bar[4];glReadPixels(0,height/2,1,1,GL_RGBA,GL_UNSIGNED_BYTE,bar);assert(!bar[0]&&!bar[1]&&!bar[2]&&bar[3]==255);
   /* Timer query measures GPU work only. Bounded warmup and eight passes;
    * readback here is fixture instrumentation, never required by the module. */
   for(int i=0;i<3;++i)glDrawArrays(GL_TRIANGLES,0,3);glFinish();
   double ms[3];
   for(int run=0;run<3;++run){
    GLuint query;glGenQueries(1,&query);glBeginQuery(GL_TIME_ELAPSED,query);
    for(int i=0;i<8;++i)glDrawArrays(GL_TRIANGLES,0,3);
    glEndQuery(GL_TIME_ELAPSED);GLuint64 ns=0;glGetQueryObjectui64v(query,GL_QUERY_RESULT,&ns);glDeleteQueries(1,&query);
    ms[run]=ns/8000000.0;
   }
   for(int i=0;i<3;++i)for(int j=i+1;j<3;++j)if(ms[j]<ms[i]){double t=ms[i];ms[i]=ms[j];ms[j]=t;}
   printf("%dx%d viewport%dx%d strength%.2f GPU_ms_per_pass_median=%.6f range=%.6f..%.6f\n",width,height,view,height,strength,ms[1],ms[0],ms[2]);
   assert(glGetError()==GL_NO_ERROR);
  }
 }
 glDeleteFramebuffers(1,&fb);glDeleteTextures(2,textures);glDeleteVertexArrays(1,&vao);glDeleteProgram(program);glDeleteShader(v);glDeleteShader(f);
 CGLSetCurrentContext(NULL);CGLDestroyContext(ctx);puts("PASS: output-resolution sampling, source edges, bounded sharpening, sampled alpha, 4:3 black bars");
}
