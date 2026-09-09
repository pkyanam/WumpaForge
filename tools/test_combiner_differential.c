/* Independent scalar equation oracle for native generated fragment programs.
 * Synthetic inputs only; NV_register_combiners tables/equations are the oracle.
 * Reuse only the existing test's GL compile/link helpers, not generator logic. */
#define main existing_pixel_smoke_main
#include "test_nv2a_pixel.c"
#undef main

static uint32_t random_state=0x57464f43;
static uint32_t random_word(void) { random_state^=random_state<<13;random_state^=random_state>>17;random_state^=random_state<<5;return random_state; }
static float bound(float x,float lo,float hi) { return fminf(hi,fmaxf(lo,x)); }
static float mapped_input(unsigned byte,unsigned component,int alpha,const float registers[16][4],const float constants[18][4],unsigned stage,uint32_t control)
{
    unsigned reg=byte&15,index=(byte&16)?3:alpha?2:component;
    float x;
    if(reg==1 || reg==2) {
        unsigned unique=control&(reg==1?0x1000:0x10000);
        x=constants[2*(unique?stage:0)+reg-1][index];
    } else x=registers[reg][index];
    switch(byte>>5) {
    case 0:return fmaxf(x,0);case 1:return 1-bound(x,0,1);
    case 2:return 2*fmaxf(x,0)-1;case 3:return 1-2*fmaxf(x,0);
    case 4:return fmaxf(x,0)-.5f;case 5:return .5f-fmaxf(x,0);
    case 6:return x;default:return -x;
    }
}
static float mapped_output(float x,unsigned word)
{
    unsigned mode=(word>>15)&7;
    if(mode==1 || mode==3)x-=.5f;
    if(mode==2 || mode==3)x*=2;else if(mode==4)x*=4;else if(mode==6)x*=.5f;
    return bound(x,-1,1);
}
static void scalar_oracle(const Nv2aPixelDef *d,const float texels[4][4],const float constants[18][4],float expected[4])
{
    float registers[16][4]={{0}};
    for(unsigned c=0;c<4;++c){registers[4][c]=.8f;registers[5][c]=.1f;}
    memcpy(registers+8,texels,16*sizeof(float));registers[12][3]=texels[0][3];
    for(unsigned stage=0;stage<(d->combiner_count&255);++stage) {
        float products[2][3][4]={{{0}}};
        float selector=bound(registers[12][3],0,1)*255.0f;
        int choose_cd=(d->combiner_count&0x100)?registers[12][3]>=.5f:((unsigned)selector&1)!=0;
        for(unsigned alpha=0;alpha<2;++alpha) {
            uint32_t inputs=alpha?d->alpha_inputs[stage]:d->rgb_inputs[stage];
            uint32_t outputs=alpha?d->alpha_outputs[stage]:d->rgb_outputs[stage];
            unsigned first=alpha?3:0,last=alpha?4:3;
            for(unsigned c=first;c<last;++c) {
                float operands[4];
                for(unsigned i=0;i<4;++i)operands[i]=mapped_input((inputs>>(24-8*i))&255,c,alpha,registers,constants,stage,d->combiner_count);
                products[alpha][0][c]=operands[0]*operands[1];products[alpha][1][c]=operands[2]*operands[3];
            }
            if(!alpha)for(unsigned pair=0;pair<2;++pair)if(outputs&(pair?0x1000:0x2000)) {
                float dot=products[0][pair][0]+products[0][pair][1]+products[0][pair][2];
                for(unsigned c=0;c<3;++c)products[0][pair][c]=dot;
            }
            for(unsigned c=first;c<last;++c) {
                products[alpha][2][c]=(outputs&0x4000)?products[alpha][choose_cd?1:0][c]:products[alpha][0][c]+products[alpha][1][c];
                for(unsigned pair=0;pair<3;++pair)products[alpha][pair][c]=mapped_output(products[alpha][pair][c],outputs);
            }
        }
        /* Both portions consume the previous register set before any write. */
        for(unsigned alpha=0;alpha<2;++alpha) {
            uint32_t word=alpha?d->alpha_outputs[stage]:d->rgb_outputs[stage];
            unsigned dest[3]={(word>>4)&15,word&15,(word>>8)&15};
            for(unsigned pair=0;pair<3;++pair)if(dest[pair])for(unsigned c=alpha?3:0;c<(alpha?4:3);++c)registers[dest[pair]][c]=products[alpha][pair][c];
        }
    }
    for(unsigned c=0;c<3;++c) {
        float color=opts.specular_enabled?bound(fmaxf(registers[12][c],0)+fmaxf(registers[5][c],0),0,1):fmaxf(registers[12][c],0);
        if(opts.fog_enabled)color=.25f*color+.75f*.2f;
        expected[c]=bound(color,0,1);
    }
    expected[3]=bound(registers[12][3],0,1);
}
static uint32_t random_inputs(void)
{
    static const unsigned regs[]={0,1,2,4,5,8,9,10,11,12,13};uint32_t word=0;
    for(unsigned i=0;i<4;++i)word=(word<<8)|regs[random_word()%11]|((random_word()%8)<<5)|((random_word()&1)<<4);
    return word;
}
int main(void)
{
    assert(!SDL_Init(SDL_INIT_VIDEO));SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,4);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window *window=SDL_CreateWindow("Combiner numerical oracle",0,0,16,16,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(window);
    SDL_GLContext context=SDL_GL_CreateContext(window);assert(context);glViewport(0,0,16,16);
    GLuint vao,textures[4];glGenVertexArrays(1,&vao);glBindVertexArray(vao);glGenTextures(4,textures);
    unsigned cases=0,max_error=0;
    for(unsigned variant=0;variant<96;++variant) {
        Nv2aPixelDef d={0};d.combiner_count=3+variant%3;
        d.combiner_count|=(variant&1?0x100:0)|(variant&2?0x1000:0)|(variant&4?0x10000:0);
        d.texture_modes=1|(1<<5)|(1<<10)|(1<<15);
        d.rgb_inputs[0]=0x08200920;d.alpha_inputs[0]=0x18201920;d.rgb_outputs[0]=d.alpha_outputs[0]=0xcd;
        static const unsigned maps[]={0,0x8000,0x10000,0x18000,0x20000,0x30000};
        for(unsigned s=1;s<(d.combiner_count&255);++s) {
            d.rgb_inputs[s]=random_inputs();d.alpha_inputs[s]=random_inputs();
            d.rgb_outputs[s]=0xc4d|maps[random_word()%6]|(random_word()&1?0x4000:0);
            d.alpha_outputs[s]=0xc4d|maps[random_word()%6]|(random_word()&1?0x4000:0);
            if(variant%7==0 && s==1)d.rgb_outputs[s]=0xcd|0x3000|maps[random_word()%6];
        }
        opts.fog_enabled=(variant>>3)&1;opts.specular_enabled=(variant>>4)&1;
        GLuint p=program(&d);glUseProgram(p);
        glUniform3f(glGetUniformLocation(p,"u_fogcolor"),.2,.2,.2);glUniform1i(glGetUniformLocation(p,"u_alpha_enable"),0);
        for(unsigned i=0;i<4;++i){char name[8];snprintf(name,sizeof(name),"tex%u",i);glUniform1i(glGetUniformLocation(p,name),i);}
        for(unsigned sample=0;sample<32;++sample) {
            float texels[4][4],constants[18][4],expected[4];
            for(unsigned i=0;i<4;++i)for(unsigned c=0;c<4;++c)texels[i][c]=(random_word()%257)/256.0f;
            for(unsigned i=0;i<18;++i)for(unsigned c=0;c<4;++c)constants[i][c]=(random_word()%257)/256.0f;
            scalar_oracle(&d,texels,constants,expected);
            for(unsigned i=0;i<4;++i){glActiveTexture(GL_TEXTURE0+i);glBindTexture(GL_TEXTURE_2D,textures[i]);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,1,1,0,GL_RGBA,GL_FLOAT,texels[i]);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);}
            glUniform4fv(glGetUniformLocation(p,"u_psconstants"),18,&constants[0][0]);glDrawArrays(GL_TRIANGLES,0,3);
            unsigned char pixel[4];glReadPixels(1,1,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);assert(glGetError()==GL_NO_ERROR);
            for(unsigned c=0;c<4;++c){unsigned delta=abs((int)pixel[c]-(int)lroundf(expected[c]*255));if(delta>max_error)max_error=delta;if(delta>1){fprintf(stderr,"FAIL variant%u sample%u channel%u actual%u expected%.9g control%X seed%X\n",variant,sample,c,pixel[c],expected[c],d.combiner_count,random_state);return 1;}}
            ++cases;
        }
        glDeleteProgram(p);
    }
    glDeleteTextures(4,textures);glDeleteVertexArrays(1,&vao);SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
    printf("PASS %u synthetic GPU/scalar cases across96 multi-stage combiners; signed mappings, bias/scales, dot, both mux modes, independent constants, RGB/alpha concurrency and final fog/specular; max error %u/255\n",cases,max_error);
}
