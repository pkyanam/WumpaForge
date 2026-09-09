/* Synthetic, hand-assembled NV2A instructions; no game bytes embedded.
 * Numeric results are checked on the native GPU through transform feedback. */
#include "../../src/nv2a_vertex.h"
#include "../../src/nv2a_vertex_input.h"
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
static void test_relative(void)
{
    /* ARL ignores its vector mask/destination. Each paired MOV must read the
     * previous A0, not the address produced alongside it. B is reserved mux0
     * in both ARLs because ARL has no B operand. */
    uint32_t words[][4]={
        {0,(1u<<21)|(5u<<13)|0x1b,3u<<26,(15u<<24)|(2u<<20)},
        {0,(13u<<21)|(1u<<25)|(100u<<13)|(1u<<9)|0x55,
            (2u<<26)|(0x1bu<<2),(3u<<28)|(15u<<24)|(2u<<20)|(15u<<12)|(1u<<11)|(9u<<3)|4|2},
        {0,(1u<<21)|(100u<<13)|0x1b,3u<<26,(15u<<12)|(1u<<11)|(10u<<3)|2},
        {0,(13u<<21)|(1u<<25)|(100u<<13)|(1u<<9)|(1u<<8)|0xaa,
            (2u<<26)|(0x1bu<<2),(3u<<28)|(15u<<12)|(1u<<11)|(11u<<3)|4|2},
        {0,(1u<<21)|(100u<<13)|0x1b,3u<<26,(15u<<12)|(1u<<11)|(12u<<3)|2},
        {0,(1u<<21)|0x1b,(2u<<28)|(1u<<26),(15u<<12)|(1u<<11)|(4u<<3)},
        {0,(1u<<21)|0x1b,2u<<26,(15u<<12)|(1u<<11)|1}
    };
    Nv2aVertexInfo info;
    assert(nv2a_vertex_generate(&words[0][0],7,source,sizeof(source),&info,error,sizeof(error)));
    assert(info.relative_constants && info.input_mask==3);
    for(unsigned i=0;i<3;i++)assert(info.constant_mask[i]==UINT64_MAX);
    GLuint shader=compile(source),program=glCreateProgram(); glAttachShader(program,shader);
    const char *feedback[]={"vT0","vT1","vT2","vT3","vD1"};
    glTransformFeedbackVaryings(program,5,feedback,GL_INTERLEAVED_ATTRIBS);
    glLinkProgram(program); GLint ok; glGetProgramiv(program,GL_LINK_STATUS,&ok); assert(ok);
    glUseProgram(program);
    float constants[192][4];
    for(unsigned i=0;i<192;i++)for(unsigned j=0;j<4;j++)constants[i][j]=(float)(i*4+j+1);
    for(unsigned j=0;j<4;j++)constants[5][j]=(j+1)*.25f;
    glUniform4fv(glGetUniformLocation(program,"u_vconstants"),192,&constants[0][0]);
    glUniform4f(glGetUniformLocation(program,"u_nv2a_viewport"),0,0,640,480);
    glUniform2f(glGetUniformLocation(program,"u_nv2a_depth"),0,200);
    GLuint vao,buffer; glGenVertexArrays(1,&vao); glBindVertexArray(vao);
    glVertexAttrib4f(0,160,120,100,1);
    glGenBuffers(1,&buffer); glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,buffer);
    glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER,20*sizeof(float),NULL,GL_STREAM_READ);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,buffer);
    const float addresses[]={-1.1f,-.0001f,0,.999999f,1.9999f,2,91,-100,-101,92};
    for(unsigned k=0;k<sizeof(addresses)/sizeof(addresses[0]);k++) {
        float a=addresses[k],b=.125f-k;
        glVertexAttrib4f(1,99,a,b,99);
        glBeginTransformFeedback(GL_POINTS); glDrawArrays(GL_POINTS,0,1); glEndTransformFeedback();
        float result[20]; glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER,0,sizeof(result),result);
        int indices[]={100,100+(int)floorf(a),100+(int)floorf(a),100+(int)floorf(-b),5};
        for(unsigned i=0;i<20;i++) {
            int c=indices[i/4]; float expected=c>=0&&c<192?constants[c][i%4]:0;
            if(result[i]!=expected)fprintf(stderr,"ARL case%u component%u: %g != %g\n",k,i,result[i],expected);
            assert(result[i]==expected);
        }
    }
    assert(glGetError()==GL_NO_ERROR);
    glDeleteBuffers(1,&buffer); glDeleteVertexArrays(1,&vao); glDeleteProgram(program); glDeleteShader(shader);
    /* ARL cannot be treated as a vector-producing opcode. */
    words[1][3]&=~4u;
    assert(!nv2a_vertex_generate(&words[0][0],7,source,sizeof(source),&info,error,sizeof(error)) && strstr(error,"ARL routed"));
    puts("PASS: GPU ARL exact floor, negative/swizzled inputs, zero/nonzero vector masks, paired old A0, relative bounds and untouched vector temporary");
}
static void test_captured_dead_input_gpu(const uint32_t *words,size_t count,const float captured[192][4])
{
    Nv2aVertexInfo info;
    assert(nv2a_vertex_generate(words,count,source,sizeof(source),&info,error,sizeof(error)));
    GLuint shader=compile(source),program=glCreateProgram();glAttachShader(program,shader);
    const char *feedback[]={"gl_Position","vD0","vD1","vT0","vT1","vT2","vT3","vFog"};
    glTransformFeedbackVaryings(program,8,feedback,GL_INTERLEAVED_ATTRIBS);
    glLinkProgram(program);GLint ok;glGetProgramiv(program,GL_LINK_STATUS,&ok);assert(ok);glUseProgram(program);
    glUniform4f(glGetUniformLocation(program,"u_nv2a_viewport"),0,0,640,480);
    glUniform2f(glGetUniformLocation(program,"u_nv2a_depth"),0,16777215);
    GLuint vao,buffer;glGenVertexArrays(1,&vao);glBindVertexArray(vao);
    glVertexAttrib4f(0,1,2,3,1);glVertexAttrib4f(1,0,.5f,0,1);
    glVertexAttrib4f(2,0,1,0,1);glVertexAttrib4f(3,0,0,0,1);
    glVertexAttrib4f(4,1,1,1,1);glVertexAttrib4f(5,.25f,.5f,0,1);
    glGenBuffers(1,&buffer);glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,buffer);
    glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER,29*sizeof(float),NULL,GL_STREAM_READ);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,buffer);
    float constants[192][4];memcpy(constants,captured,sizeof(constants));
    for(unsigned live=0;live<2;++live) {
        if(live)constants[122][0]=1;
        assert(((nv2a_vertex_dead_input_mask(words,count,constants)>>6)&1u)==!live);
        glUniform4fv(glGetUniformLocation(program,"u_vconstants"),192,&constants[0][0]);
        for(unsigned input=6;input<(count==88?16u:7u);++input) {
            float result[2][29];
            for(unsigned i=0;i<2;++i) {
                glVertexAttrib4f(input,i?100:0,i?-200:0,i?300:0,i?4:2);
                glBeginTransformFeedback(GL_POINTS);glDrawArrays(GL_POINTS,0,1);glEndTransformFeedback();
                glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER,0,sizeof(result[i]),result[i]);
                for(unsigned j=0;j<29;++j)assert(isfinite(result[i][j]));
            }
            assert((memcmp(result[0],result[1],sizeof(result[0]))!=0)==live);
            glVertexAttrib4f(input,0,0,0,2);
        }
    }
    assert(glGetError()==GL_NO_ERROR);
    glDeleteBuffers(1,&buffer);glDeleteVertexArrays(1,&vao);glDeleteProgram(program);glDeleteShader(shader);
    printf("PASS: captured%zu-instruction shader on native GPU: all29 outputs independent of each secondary input at captured zero weight, differ after live weight1\n",count);
}
static void test_dead_input_gpu(void)
{
    uint32_t words[][4]={
        {0,(1u<<21)|(121u<<13)|0x1b,3u<<26,15u<<24},
        {0,(4u<<21)|(122u<<13)|(6u<<9),(3u<<26)|(0x1bu<<17)|(2u<<11)|(0x1bu<<2),
            (1u<<28)|(15u<<12)|(1u<<11)|(9u<<3)},
        {0,(1u<<21)|0x1b,2u<<26,(15u<<12)|(1u<<11)|1}
    };
    Nv2aVertexInfo info;
    assert(nv2a_vertex_generate(&words[0][0],3,source,sizeof(source),&info,error,sizeof(error)));
    GLuint shader=compile(source),program=glCreateProgram(); glAttachShader(program,shader);
    const char *feedback[]={"vT0"};
    glTransformFeedbackVaryings(program,1,feedback,GL_INTERLEAVED_ATTRIBS);
    glLinkProgram(program); GLint ok; glGetProgramiv(program,GL_LINK_STATUS,&ok); assert(ok);
    glUseProgram(program);
    float constants[192][4]={{0}};
    for(unsigned j=0;j<4;j++)constants[121][j]=(j+1)*.25f;
    assert(nv2a_vertex_input_is_dead(&words[0][0],3,6,constants));
    glUniform4fv(glGetUniformLocation(program,"u_vconstants"),192,&constants[0][0]);
    glUniform4f(glGetUniformLocation(program,"u_nv2a_viewport"),0,0,640,480);
    glUniform2f(glGetUniformLocation(program,"u_nv2a_depth"),0,200);
    GLuint vao,buffer; glGenVertexArrays(1,&vao); glBindVertexArray(vao);
    glVertexAttrib4f(0,160,120,100,1);
    glGenBuffers(1,&buffer); glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,buffer);
    glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER,4*sizeof(float),NULL,GL_STREAM_READ);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,buffer);
    const float inputs[][4]={{0,0,0,1},{100,-200,300,-400},{NAN,INFINITY,-INFINITY,NAN}};
    for(unsigned i=0;i<3;i++) {
        glVertexAttrib4fv(6,inputs[i]);
        glBeginTransformFeedback(GL_POINTS);glDrawArrays(GL_POINTS,0,1);glEndTransformFeedback();
        float result[4];glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER,0,sizeof(result),result);
        for(unsigned j=0;j<4;j++)assert(result[j]==constants[121][j]);
    }
    constants[122][0]=1;
    assert(!nv2a_vertex_input_is_dead(&words[0][0],3,6,constants));
    glUniform4fv(glGetUniformLocation(program,"u_vconstants"),192,&constants[0][0]);
    glVertexAttrib4fv(6,inputs[1]);
    glBeginTransformFeedback(GL_POINTS);glDrawArrays(GL_POINTS,0,1);glEndTransformFeedback();
    float result[4];glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER,0,sizeof(result),result);
    for(unsigned j=0;j<4;j++)assert(result[j]==constants[121][j]+inputs[1][j]);
    assert(glGetError()==GL_NO_ERROR);
    glDeleteBuffers(1,&buffer);glDeleteVertexArrays(1,&vao);glDeleteProgram(program);glDeleteShader(shader);
    puts("PASS: unchanged GPU MAD gives equal output for zero-weight input variations including NaN/Inf, nonzero weight remains live");
}
static void test_bone_rows(void)
{
    /* Reached pattern: A0=floor(v2.x*c114.z), then three DP4 position rows
     * and three DP3 normal rows at physical constants124..126+A0. */
    uint32_t words[9][4]={
        {0,(2u<<21)|(114u<<13)|(2u<<9),(2u<<26)|(0xaau<<17)|(3u<<11),(8u<<24)|(5u<<20)},
        {0,(13u<<21)|(1u<<25)|(5u<<9),(5u<<28)|(1u<<26)|(0x1bu<<2),(2u<<28)|(15u<<12)|(1u<<11)|(12u<<3)|4}
    };
    for(unsigned row=0;row<6;row++) {
        uint32_t *w=words[row+2]; unsigned normal=row>=3;
        w[1]=((normal?5u:7u)<<21)|((124+row%3)<<13)|((normal?3u:0u)<<9)|0x1b;
        w[2]=(2u<<26)|(0x1bu<<17)|(3u<<11);
        w[3]=((8u>>(row%3))<<12)|(1u<<11)|((normal?10u:9u)<<3)|2;
    }
    words[8][1]=(1u<<21)|0x1b; words[8][2]=2u<<26;
    words[8][3]=(15u<<12)|(1u<<11)|1;
    Nv2aVertexInfo info;
    assert(nv2a_vertex_generate(&words[0][0],9,source,sizeof(source),&info,error,sizeof(error)));
    GLuint shader=compile(source),program=glCreateProgram(); glAttachShader(program,shader);
    const char *feedback[]={"vT0","vT1","vT3"};
    glTransformFeedbackVaryings(program,3,feedback,GL_INTERLEAVED_ATTRIBS);
    glLinkProgram(program); GLint ok; glGetProgramiv(program,GL_LINK_STATUS,&ok); assert(ok);
    glUseProgram(program);
    float constants[192][4]={{0}}; constants[114][2]=3;
    for(unsigned bone=0;bone<16;bone++)for(unsigned row=0;row<3;row++) {
        constants[124+3*bone+row][row]=(float)(bone+1);
        constants[124+3*bone+row][3]=(float)(10+row);
    }
    glUniform4fv(glGetUniformLocation(program,"u_vconstants"),192,&constants[0][0]);
    glUniform4f(glGetUniformLocation(program,"u_nv2a_viewport"),0,0,640,480);
    glUniform2f(glGetUniformLocation(program,"u_nv2a_depth"),0,200);
    GLuint vao,buffer,input_buffer; glGenVertexArrays(1,&vao); glBindVertexArray(vao);
    /* Same 56-byte primary declaration as captured69/88: FLOAT3 position,
     * FLOAT2 weights, FLOAT3 indices/normals, BGRA8 diffuse, FLOAT2 UV. */
    struct {float position[3],weights[2],indices[3],normal[3];uint32_t diffuse;float uv[2];} input={
        {2,3,4},{.25f,.75f},{0,1,2},{5,6,7},0xFF4080C0,{.25f,.5f}};
    _Static_assert(sizeof(input)==56,"captured primary stride");
    glGenBuffers(1,&input_buffer);glBindBuffer(GL_ARRAY_BUFFER,input_buffer);
    glBufferData(GL_ARRAY_BUFFER,sizeof(input),&input,GL_STREAM_DRAW);
    const unsigned formats[]={0x32,0x22,0x32,0x32,0x40,0x22},offsets[]={0,12,20,32,44,48};
    for(unsigned i=0;i<6;++i){Nv2aVertexSlot slot={0};
        assert(nv2a_vertex_format_decode(formats[i],&slot,error,sizeof(error)));
        glEnableVertexAttribArray(i);glVertexAttribPointer(i,i==4?GL_BGRA:slot.components,
            i==4?GL_UNSIGNED_BYTE:GL_FLOAT,slot.normalized,sizeof(input),(void *)(uintptr_t)offsets[i]);}
    assert(formats[2]==0x32);
    glGenBuffers(1,&buffer); glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,buffer);
    glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER,12*sizeof(float),NULL,GL_STREAM_READ);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,buffer);
    for(unsigned bone=0;bone<16;bone++) {
        input.indices[0]=(float)bone;input.indices[1]=(float)(15-bone);input.indices[2]=(float)(bone/2);
        glBindBuffer(GL_ARRAY_BUFFER,input_buffer);glBufferSubData(GL_ARRAY_BUFFER,0,sizeof(input),&input);
        glBeginTransformFeedback(GL_POINTS); glDrawArrays(GL_POINTS,0,1); glEndTransformFeedback();
        float result[12]; glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER,0,sizeof(result),result);
        for(unsigned row=0;row<3;row++) {
            assert(result[row]==(float)((row+2)*(bone+1)+10+row));
            assert(result[4+row]==(float)((row+5)*(bone+1)));
        }
        assert(result[3]==1 && result[7]==1);
        const float uv[4]={.25f,.5f,0,1};for(unsigned j=0;j<4;j++)assert(result[8+j]==uv[j]);
    }
    assert(glGetError()==GL_NO_ERROR);
    glDeleteBuffers(1,&input_buffer);glDeleteBuffers(1,&buffer); glDeleteVertexArrays(1,&vao); glDeleteProgram(program); glDeleteShader(shader);
    puts("PASS: captured56-byte declaration FLOAT3 bone indices, GPU16 bone matrices, physical relative B-source DP4/DP3 rows and paired ARL texture output");
}
static void test_numeric_edges(void)
{
    uint32_t words[][4]={
        {0,(1u<<21)|(1u<<9)|0x1b,2u<<26,(15u<<12)|(1u<<11)|(3u<<3)},
        {0,(1u<<21)|(1u<<9)|0x1b,2u<<26,(15u<<12)|(1u<<11)|(4u<<3)},
        {0,(4u<<25)|(1u<<9),0,(2u<<28)|(15u<<12)|(1u<<11)|(9u<<3)|4},
        {0,(1u<<21)|0x1b,2u<<26,(15u<<12)|(1u<<11)|1}
    };
    Nv2aVertexInfo info;assert(nv2a_vertex_generate(&words[0][0],4,source,sizeof(source),&info,error,sizeof(error)));
    GLuint shader=compile(source),program=glCreateProgram();glAttachShader(program,shader);
    const char *feedback[]={"vD0","vD1","vT0"};glTransformFeedbackVaryings(program,3,feedback,GL_INTERLEAVED_ATTRIBS);
    glLinkProgram(program);GLint ok;glGetProgramiv(program,GL_LINK_STATUS,&ok);assert(ok);glUseProgram(program);
    glUniform4f(glGetUniformLocation(program,"u_nv2a_viewport"),0,0,640,480);glUniform2f(glGetUniformLocation(program,"u_nv2a_depth"),0,200);
    GLuint vao,buffer;glGenVertexArrays(1,&vao);glBindVertexArray(vao);glVertexAttrib4f(0,160,120,100,1);
    glGenBuffers(1,&buffer);glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,buffer);glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER,12*sizeof(float),NULL,GL_STREAM_READ);glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,buffer);
    const float cases[]={0,-0.0f,.125f,.5f,.9999f,4,-4,INFINITY,-INFINITY,NAN,-NAN};unsigned mismatches=0;
    for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);++i){
        float x=cases[i];glVertexAttrib4f(1,x,x,x,x);glBeginTransformFeedback(GL_POINTS);glDrawArrays(GL_POINTS,0,1);glEndTransformFeedback();
        float result[12];glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER,0,sizeof(result),result);
        float color=isnan(x)?1:fminf(1,fmaxf(0,x));float rsq=x==0?INFINITY:isinf(x)?0:1/sqrtf(fabsf(x));
        for(unsigned j=0;j<12;++j){float want=j<8?color:rsq;
            if(!(result[j]==want || (isnan(result[j])&&isnan(want)))){
                fprintf(stderr,"VERTEX-EDGE case%u input%g output%u actual%g expected%g\n",i,x,j,result[j],want);++mismatches;}}
    }
    glDeleteBuffers(1,&buffer);glDeleteVertexArrays(1,&vao);glDeleteProgram(program);glDeleteShader(shader);
    assert(glGetError()==GL_NO_ERROR);assert(!mismatches);
    puts("PASS native GPU diffuse/specular NaN clamp and RSQ signed-zero/infinity/negative finite edge cases");
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
    test_relative();
    test_bone_rows();
    test_numeric_edges();
    test_dead_input_gpu();
    uint32_t saved=words[1]; words[1]=(words[1]&~(15u<<21))|(15u<<21);
    assert(!nv2a_vertex_generate(words,3,source,sizeof(source),&info,error,sizeof(error)) && strstr(error,"MAC opcode 15") && !source[0]);
    words[1]=saved; words[3]|=2;
    assert(nv2a_vertex_generate(words,3,source,sizeof(source),&info,error,sizeof(error)) && info.relative_constants);
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
    if(argc>2) {
        FILE *f=fopen(argv[2],"rb"); assert(f);
        uint32_t game[136][4]; size_t count=fread(game,sizeof(game[0]),136,f); assert(!ferror(f)); fclose(f);
        assert(count==69||count==88);
        if(!nv2a_vertex_generate(&game[0][0],count,source,sizeof(source),&info,error,sizeof(error))) {
            fprintf(stderr,"captured shader: %s\n",error); abort();
        }
        assert(info.instruction_count==count && info.relative_constants);
        if(argc>3) {
            float constants[192][4];
            f=fopen(argv[3],"rb"); assert(f); assert(fread(constants,sizeof(constants),1,f)==1); fclose(f);
            test_captured_dead_input_gpu(&game[0][0],count,constants);
            unsigned secondary=count==88?0xffc0:0x40;
            assert((nv2a_vertex_dead_input_mask(&game[0][0],count,constants)&secondary)==secondary);
            constants[122][0]=1e-30f;
            assert(!(nv2a_vertex_dead_input_mask(&game[0][0],count,constants)&secondary));
            constants[122][0]=1;
            assert(!(nv2a_vertex_dead_input_mask(&game[0][0],count,constants)&secondary));
            printf("PASS: actual%zu-instruction shader/constants prove secondary inputs dead only for captured exactzero weight\n",count);
        }
        shader=compile(source); glDeleteShader(shader);
        f=fopen("local/reports/boot35-vertex.glsl","w"); assert(f); fputs(source,f); fclose(f);
        printf("PASS: actual %zu-instruction captured shader with ARL compiles on native GPU\n",count);
    }
    SDL_GL_DeleteContext(context); SDL_DestroyWindow(window); SDL_Quit();
    puts("PASS: native GPU verified paired MOV, MAD, ADD, swizzle/masks, R12 alias, constants, viewport/depth/w; malformed programs rejected");
}
