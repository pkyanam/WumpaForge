/* Synthetic, hand-assembled NV2A instructions; no game bytes embedded.
 * Numeric results are checked on the native GPU through transform feedback. */
#include "../../src/nv2a_vertex.h"
#include <SDL.h>
#include <epoxy/gl.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char source[65536],error[256];
static GLuint compile(const char *text)
{
    GLuint shader=glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(shader,1,&text,NULL); glCompileShader(shader);
    GLint ok; glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
    if(!ok) { char log[4096]; glGetShaderInfoLog(shader,sizeof(log),NULL,log); fprintf(stderr,"%s\n",log); }
    assert(ok); return shader;
}
int main(int argc,char **argv)
{
    /* MOV r0.x,c2 + MOV oPos,v0; MAD oT0,r0.x,c3,v3;
     * ADD oPos,r12,c4. END. Source selector: R=1,V=2,C=3. */
    uint32_t words[12]={
        0,(1u<<25)|(1u<<21)|(2u<<13)|0x1b, (3u<<26)|(0x1bu<<2), (2u<<28)|(8u<<24)|(15u<<12)|(1u<<11)|4,
        0,(4u<<21)|(3u<<13)|(3u<<9), (1u<<26)|(0x1bu<<17)|(3u<<11)|(0x1bu<<2), (2u<<28)|(15u<<12)|(1u<<11)|(9u<<3),
        0,(3u<<21)|(4u<<13)|0x1b, (12u<<28)|(1u<<26)|(0x1bu<<2), (3u<<28)|(15u<<12)|(1u<<11)|1
    };
    Nv2aVertexInfo info;
    assert(nv2a_vertex_generate(words,3,source,sizeof(source),&info,error,sizeof(error)));
    assert(info.instruction_count==3 && info.input_mask==9 && info.constant_mask[0]==0x1c);
    assert(SDL_Init(SDL_INIT_VIDEO)==0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,4); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window *window=SDL_CreateWindow("NV2A compiler regression",0,0,32,32,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
    assert(window); SDL_GLContext context=SDL_GL_CreateContext(window); assert(context);
    GLuint shader=compile(source),program=glCreateProgram(); glAttachShader(program,shader);
    const char *feedback[]={"gl_Position","vT0"}; glTransformFeedbackVaryings(program,2,feedback,GL_INTERLEAVED_ATTRIBS);
    glLinkProgram(program); GLint ok; glGetProgramiv(program,GL_LINK_STATUS,&ok);
    if(!ok) { char log[4096]; glGetProgramInfoLog(program,sizeof(log),NULL,log); fprintf(stderr,"%s\n",log); }
    assert(ok); glUseProgram(program);
    float constants[192][4]={{0}}; constants[2][0]=2;
    for(unsigned i=0;i<4;i++)constants[3][i]=(float)i+1;
    glUniform4fv(glGetUniformLocation(program,"u_vconstants"),192,&constants[0][0]);
    glUniform4f(glGetUniformLocation(program,"u_nv2a_viewport"),0,0,640,480);
    glUniform2f(glGetUniformLocation(program,"u_nv2a_depth"),0,200);
    GLuint vao,buffer; glGenVertexArrays(1,&vao); glBindVertexArray(vao);
    glVertexAttrib4f(0,160,120,100,2); glVertexAttrib4f(3,.5f,.25f,0,1);
    glGenBuffers(1,&buffer); glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,buffer);
    glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER,8*sizeof(float),NULL,GL_STREAM_READ);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,buffer);
    glEnable(GL_RASTERIZER_DISCARD); glBeginTransformFeedback(GL_POINTS); glDrawArrays(GL_POINTS,0,1); glEndTransformFeedback();
    float result[8]; glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER,0,sizeof(result),result);
    const float expected[8]={-1,1,0,2,2.5f,4.25f,6,9};
    for(unsigned i=0;i<8;i++) { if(fabsf(result[i]-expected[i])>.00001f)fprintf(stderr,"component%u got%f expected%f\n",i,result[i],expected[i]); assert(fabsf(result[i]-expected[i])<.00001f); }
    glUniform2f(glGetUniformLocation(program,"u_nv2a_depth"),50,50);
    glBeginTransformFeedback(GL_POINTS); glDrawArrays(GL_POINTS,0,1); glEndTransformFeedback();
    glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER,0,sizeof(result),result);
    for(unsigned i=0;i<8;++i)assert(isfinite(result[i]));
    assert(result[2]==-2 && result[3]==2); /* Equal GL depth-range endpoints supply constant depth. */
    assert(glGetError()==GL_NO_ERROR);
    glDeleteBuffers(1,&buffer); glDeleteVertexArrays(1,&vao); glDeleteProgram(program); glDeleteShader(shader);
    uint32_t saved=words[1]; words[1]=(words[1]&~(15u<<21))|(15u<<21);
    assert(!nv2a_vertex_generate(words,3,source,sizeof(source),&info,error,sizeof(error)) && strstr(error,"MAC opcode 15") && !source[0]);
    words[1]=saved; words[3]|=2;
    assert(!nv2a_vertex_generate(words,3,source,sizeof(source),&info,error,sizeof(error)) && strstr(error,"relative"));
    words[3]&=~2u; words[11]&=~1u;
    assert(!nv2a_vertex_generate(words,3,source,sizeof(source),&info,error,sizeof(error)) && strstr(error,"FINAL"));
    words[11]|=1;
    assert(!nv2a_vertex_generate(words,3,source,20,&info,error,sizeof(error)) && strstr(error,"buffer"));
    if(argc>1) {
        FILE *f=fopen(argv[1],"rb"); assert(f); assert(!fseek(f,0x818,SEEK_SET));
        uint32_t game[24]; assert(fread(game,sizeof(game),1,f)==1); fclose(f);
        assert(nv2a_vertex_generate(game,6,source,sizeof(source),&info,error,sizeof(error)));
        assert(info.instruction_count==6 && info.input_mask==9);
        assert(info.constant_mask[1]==((1ull<<48)|(1ull<<49)|(1ull<<55)|(1ull<<56)));
        shader=compile(source); glDeleteShader(shader);
        f=fopen("local/reports/boot14-vertex.glsl","w"); assert(f); fputs(source,f); fclose(f);
        puts("PASS: real six-instruction startup shader compiled as native GPU GLSL; inputs v0/v3, constants c112/c113/c119/c120");
    }
    SDL_GL_DeleteContext(context); SDL_DestroyWindow(window); SDL_Quit();
    puts("PASS: native GPU verified paired MOV, MAD, ADD, swizzle/masks, R12 alias, constants, viewport/depth/w; malformed programs rejected");
}
