/* Standalone native GLSL410 compiler/readback tests. Synthetic inputs only;
 * optional local diagnostic capture stays ignored and is never embedded. */
#include "../src/nv2a_pixel.h"
#include <SDL.h>
#include <epoxy/gl.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static char source[65536],error[512];
static Nv2aPixelInfo info;
static Nv2aPixelOptions opts;
static int alpha_func; static float alpha_ref;
static const char *vs_source =
 "#version 410 core\n"
 "out vec4 vD0,vD1,vT0,vT1,vT2,vT3;out float vFog;\n"
 "void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);"
 "gl_Position=vec4(p*2.0-1.0,0,1);vD0=vec4(0.8);vD1=vec4(0.1);"
 "vT0=vT1=vT2=vT3=vec4(0.5,0.5,0.5,1);vFog=0.25;}";
static GLuint compile(GLenum kind,const char *text)
{
 GLuint s=glCreateShader(kind);glShaderSource(s,1,&text,NULL);glCompileShader(s);GLint ok;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
 if(!ok){char log[4096];glGetShaderInfoLog(s,sizeof(log),NULL,log);fprintf(stderr,"%s\n%s\n",log,text);abort();}return s;
}
static GLuint program(const Nv2aPixelDef *d)
{
 if(!nv2a_pixel_generate(d,&opts,source,sizeof(source),&info,error,sizeof(error))){fprintf(stderr,"%s\n",error);abort();}
 GLuint p=glCreateProgram(),v=compile(GL_VERTEX_SHADER,vs_source),f=compile(GL_FRAGMENT_SHADER,source);
 glAttachShader(p,v);glAttachShader(p,f);glLinkProgram(p);GLint ok;glGetProgramiv(p,GL_LINK_STATUS,&ok);
 if(!ok){char log[4096];glGetProgramInfoLog(p,sizeof(log),NULL,log);fprintf(stderr,"%s\n",log);abort();}
 glDeleteShader(v);glDeleteShader(f);return p;
}
static void draw(const Nv2aPixelDef *d,const float texels[4][4],const float *constants,const float expected[4])
{
 GLuint p=program(d);glUseProgram(p);GLuint textures[4];glGenTextures(4,textures);
 for(unsigned i=0;i<4;++i){glActiveTexture(GL_TEXTURE0+i);glBindTexture(GL_TEXTURE_2D,textures[i]);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,1,1,0,GL_RGBA,GL_FLOAT,texels[i]);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  char name[8];snprintf(name,sizeof(name),"tex%u",i);glUniform1i(glGetUniformLocation(p,name),i);}
 glUniform4fv(glGetUniformLocation(p,"u_psconstants"),18,constants?constants:&info.constants[0][0]);
 glUniform3f(glGetUniformLocation(p,"u_fogcolor"),0.2,0.2,0.2);
 glUniform1i(glGetUniformLocation(p,"u_alpha_enable"),alpha_func!=0);glUniform1i(glGetUniformLocation(p,"u_alpha_func"),alpha_func);
 glUniform1f(glGetUniformLocation(p,"u_alpha_ref"),alpha_ref);glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT);glDrawArrays(GL_TRIANGLES,0,3);
 unsigned char pixel[4];glReadPixels(1,1,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
 assert(glGetError()==GL_NO_ERROR);
 for(unsigned c=0;c<4;++c)if(abs((int)pixel[c]-(int)lroundf(expected[c]*255))>1){fprintf(stderr,"pixel[%u]=%u expected%.4f\n",c,pixel[c],expected[c]);abort();}
 glDeleteTextures(4,textures);glDeleteProgram(p);
}
static Nv2aPixelDef copy_texture(void)
{
 Nv2aPixelDef d={0};d.combiner_count=1;d.texture_modes=1;
 d.rgb_inputs[0]=0x08200000;d.alpha_inputs[0]=0x18300000;
 d.rgb_outputs[0]=d.alpha_outputs[0]=0xc00;return d;
}
int main(int argc,char **argv)
{
 _Static_assert(sizeof(Nv2aPixelDef)==240,"wire layout");assert(SDL_Init(SDL_INIT_VIDEO)==0);
 SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,4);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
 SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
 SDL_Window *window=SDL_CreateWindow("Pixel generator test",0,0,16,16,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(window);
 SDL_GLContext context=SDL_GL_CreateContext(window);assert(context);glViewport(0,0,16,16);
 GLuint vao;glGenVertexArrays(1,&vao);glBindVertexArray(vao);
 const float texels[4][4]={{0.2,0.3,0.25,0.75},{0.5,0.5,0.5,0.5},{0.1,0.2,0.3,0.4},{0.8,0.7,0.6,0.5}};
 Nv2aPixelDef d=copy_texture();draw(&d,texels,NULL,texels[0]);
 opts.fog_enabled=1;float fogged[4]={0.2,0.225,0.2125,0.75};draw(&d,texels,NULL,fogged);opts.fog_enabled=0;
 opts.specular_enabled=1;float spec[4]={0.3,0.4,0.35,0.75};draw(&d,texels,NULL,spec);opts.specular_enabled=0;
 /* Both portions read the previous stage before either writes its result. */
 d.combiner_count=2;d.rgb_inputs[1]=0x20200000;d.alpha_inputs[1]=0xcc300000;d.rgb_outputs[1]=d.alpha_outputs[1]=0xc00;
 float parallel[4]={1,1,1,0.25};draw(&d,texels,NULL,parallel);
 /* All eight input mappings, independently tabulated at x=.25. */
 float quarter[4][4]={{.25,.25,.25,.25},{0},{0},{0}};
 const float mapped[8]={.25,.75,0,.5,0,.25,.25,0};
 for(unsigned m=0;m<8;++m){d=copy_texture();d.rgb_inputs[0]=((8u|(m<<5))<<24)|0x200000;
  float e[4]={mapped[m],mapped[m],mapped[m],.25};draw(&d,quarter,NULL,e);}
 const unsigned flags[6]={0,8,16,24,32,48};const float scaled[6]={.25,0,.5,0,1,.125};
 for(unsigned m=0;m<6;++m){d=copy_texture();d.rgb_outputs[0]|=flags[m]<<12;float e[4]={scaled[m],scaled[m],scaled[m],.25};draw(&d,quarter,NULL,e);}
 /* RGB dot product writes AB; alpha stays independent. */
 d=copy_texture();d.texture_modes=0x21;d.rgb_inputs[0]=0x08090000;d.rgb_outputs[0]=0x20c0;
 float dot[4]={.375,.375,.375,.75};draw(&d,texels,NULL,dot);
 /* MSB mux selects CD when the initial r0 alpha is .75. */
 d=copy_texture();d.combiner_count|=0x100;d.texture_modes=0x21;d.rgb_inputs[0]=0x08200920;d.rgb_outputs[0]|=0x4000;
 float mux[4]={.5,.5,.5,.75};draw(&d,texels,NULL,mux);
 /* Final E*F pseudo-register, plus G from texture0 alpha. */
 d=copy_texture();d.combiner_count=0;d.texture_modes=0x21;d.final_abcd=0xf;d.final_efg=0x08091800;
 float ef[4]={.1,.15,.125,.75};draw(&d,texels,NULL,ef);
 /* Per-stage constant bank selection and packed ARGB interpretation. */
 d=copy_texture();d.combiner_count=0x1002;d.constant0[0]=0xffff0000;d.constant0[1]=0xff4080c0;
 d.rgb_inputs[1]=0x01200000;d.rgb_outputs[1]=0xc00;d.alpha_outputs[1]=0;
 float constant[4]={64/255.0f,128/255.0f,192/255.0f,.75};draw(&d,texels,NULL,constant);
 /* Shader constant mapping updates the active copy, preserving unmapped slots. */
 Nv2aPixelDef original=copy_texture();original.c0_mapping=0xffff3333;original.c1_mapping=0xfffffff4;original.final_constants=0x134;
 Nv2aPixelDef active=original;
 float colors[8]={-1,.5,2,.25,1,0,.5,1};
 assert(nv2a_pixel_set_constants(&active,3,colors,2));
 assert(active.constant0[0]==0x400080ff && active.constant0[3]==0x400080ff && active.constant0[4]==0);
 assert(active.constant1[0]==0xffff0080 && active.final_constant0==0xffff0080 && active.final_constant1==0x400080ff);
 assert(original.constant0[0]==0 && original.final_constant0==0);
 /* Xbox mapping indices span four bits, independently of the eight stages. */
 active.c0_mapping=0xfedcba98;active.c1_mapping=0x89abcdef;active.final_constants=0x1f8;
 assert(nv2a_pixel_set_constants(&active,8,colors,1));
 assert(active.constant0[0]==0x400080ff && active.constant1[7]==0x400080ff && active.final_constant0==0x400080ff);
 assert(nv2a_pixel_set_constants(&active,15,colors+4,1));
 assert(active.constant0[7]==0xffff0080 && active.constant1[0]==0xffff0080 && active.final_constant1==0xffff0080);
 /* Updates can cross the old boundary; unmapped registers remain unchanged. */
 active.c0_mapping=0xfedcba87;
 assert(nv2a_pixel_set_constants(&active,7,colors,2));
 assert(active.constant0[0]==0x400080ff && active.constant0[1]==0xffff0080 && active.constant0[7]==0xffff0080);
 Nv2aPixelDef unchanged=active;
 assert(!nv2a_pixel_set_constants(&active,15,colors,2) && memcmp(&active,&unchanged,sizeof(active))==0);
 assert(!nv2a_pixel_set_constants(&active,16,colors,1));
 assert(!nv2a_pixel_set_constants(&active,UINT32_MAX,colors,1));
 assert(nv2a_pixel_set_constants(&active,16,NULL,0) && memcmp(&active,&unchanged,sizeof(active))==0);
 assert(nv2a_pixel_set_constants(&active,8,NULL,0) && memcmp(&active,&unchanged,sizeof(active))==0);
 float special[4]={NAN,INFINITY,-INFINITY,0.5f};assert(nv2a_pixel_pack_constant(special)==0x8000ff00);
 /* Alpha equality uses the hardware's8-bit comparison domain. */
 d=copy_texture();alpha_func=3;alpha_ref=64/255.0f;draw(&d,quarter,NULL,quarter[0]);
 alpha_func=5;alpha_ref=.5;float rejected[4]={0};draw(&d,quarter,NULL,rejected);alpha_func=0;
 /* Blue-to-alpha output, with no competing alpha destination. */
 d=copy_texture();d.rgb_outputs[0]=0x800c0;d.alpha_outputs[0]=0;float blue[4]={.2,.3,.25,.25};draw(&d,texels,NULL,blue);
 /* Unsupported sampling families are not silently converted to2D. */
 for(unsigned mode=0;mode<=5;++mode){d=copy_texture();d.texture_modes=mode;GLuint p=program(&d);glDeleteProgram(p);}
 /* Invalid definitions produce no shader text, never a silent fixed shader. */
 d=copy_texture();d.texture_modes=9;assert(!nv2a_pixel_generate(&d,&opts,source,sizeof(source),&info,error,sizeof(error)) && !source[0] && strstr(error,"unsupported"));
 d=copy_texture();d.rgb_inputs[0]=0x0d200000;assert(!nv2a_pixel_generate(&d,&opts,source,sizeof(source),&info,error,sizeof(error)) && strstr(error,"undefined"));
 d=copy_texture();d.rgb_outputs[0]=0xccc;assert(!nv2a_pixel_generate(&d,&opts,source,sizeof(source),&info,error,sizeof(error)) && strstr(error,"conflicting"));
 d=copy_texture();assert(!nv2a_pixel_generate(&d,&opts,source,32,&info,error,sizeof(error)) && !source[0]);
 if(argc==3){FILE *file=fopen(argv[1],"rb");assert(file);assert(!fseek(file,strtol(argv[2],NULL,0),SEEK_SET));uint8_t bytes[240];assert(fread(bytes,1,240,file)==240);fclose(file);
  assert(nv2a_pixel_read_definition(bytes,sizeof(bytes),&d));GLuint p=program(&d);assert(info.stages==8 && info.texture_mask==15 && info.uses_default_final);
  FILE *out=fopen("local/reports/boot14-pixel.glsl","w");assert(out);fputs(source,out);fclose(out);glDeleteProgram(p);
  float c[18][4]={{0}};for(unsigned i=0;i<4;++i){c[10][i]=.8;c[12][i]=.5;c[14][i]=.1;}
  float e[4]={1,.85,1,0};draw(&d,texels,&c[0][0],e);
 }
 glDeleteVertexArrays(1,&vao);SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
 puts("PASS: GLSL410 compile/link and synthetic pixel readbacks, parallel RGB/alpha, modifiers, scale/bias, dot/mux, final/default fog/specular, constants, explicit errors");
}
