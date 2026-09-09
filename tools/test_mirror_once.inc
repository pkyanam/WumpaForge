/* Synthetic native GPU readback: address folding occurs after projective
 * division, with unwrapped gradients for LOD and independent U/V controls. */
static void test_mirror_once(void)
{
    const char *vs_source="#version 410 core\n"
        "out vec4 vD0,vD1,vT0,vT1,vT2,vT3;out float vFog;"
        "uniform vec4 coordinates;uniform float projection;"
        "void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);"
        "gl_Position=vec4(p*2.0-1.0,0,1);vD0=vec4(1);vD1=vec4(0);vFog=1;"
        "vT0=vec4(p*coordinates.zw+coordinates.xy,0,projection);"
        "vT1=vT2=vT3=vT0;}";
    Nv2aPixelDef d={0};d.texture_modes=1;d.combiner_count=1;
    d.rgb_inputs[0]=0x08200000;d.alpha_inputs[0]=0x18300000;
    d.rgb_outputs[0]=d.alpha_outputs[0]=0xC0;
    GLuint texture,vao;glGenTextures(1,&texture);glGenVertexArrays(1,&vao);glBindVertexArray(vao);
    glActiveTexture(GL_TEXTURE0);glBindSampler(0,0);glBindTexture(GL_TEXTURE_2D,texture);
    const uint8_t texels[16]={255,0,0,255, 0,255,0,255, 0,0,255,255, 255,255,0,255};
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,texels);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glViewport(0,0,16,16);
    for(unsigned linear=0;linear<2;++linear){
        char source[65536],error[256];Nv2aPixelInfo info;Nv2aPixelOptions options={0};options.rectangle_texture_mask=linear;
        assert(nv2a_pixel_generate(&d,&options,source,sizeof(source),&info,error,sizeof(error)));
        GLuint vs=shader_compile(GL_VERTEX_SHADER,vs_source),ps=shader_compile(GL_FRAGMENT_SHADER,source),program=glCreateProgram();
        glAttachShader(program,vs);glAttachShader(program,ps);glLinkProgram(program);
        GLint linked;glGetProgramiv(program,GL_LINK_STATUS,&linked);assert(linked);
        glDeleteShader(vs);glDeleteShader(ps);glUseProgram(program);
        glUniform1i(glGetUniformLocation(program,"tex0"),0);
        GLint coords=glGetUniformLocation(program,"coordinates"),projection=glGetUniformLocation(program,"projection"),mirror=glGetUniformLocation(program,"u_mirror_once0");
        /* Both signs outside [0,1], independent axes, and negative q. */
        const float cases[][5]={{-.75f,-.25f,1,1,1},{-.25f,-.75f,1,1,1},{-1.25f,-1.25f,1,1,1},
            {1.25f,1.25f,1,1,1},{-.75f,-.75f,1,1,0},{-.75f,-.75f,1,0,1},{1.5f,.5f,-2,1,1},{-.75f,-.75f,1,0,0}};
        const unsigned expected[]={1,2,3,3,1,2,1,0};
        for(unsigned c=0;c<sizeof(cases)/sizeof(cases[0]);++c){
            float factor=linear?2:1;glUniform4f(coords,cases[c][0]*factor,cases[c][1]*factor,0,0);
            glUniform1f(projection,cases[c][2]);glUniform2i(mirror,(int)cases[c][3],(int)cases[c][4]);
            glDrawArrays(GL_TRIANGLES,0,3);uint8_t pixel[4];glReadPixels(8,8,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
            if(memcmp(pixel,texels+expected[c]*4,4))fprintf(stderr,"mirror case%u linear%u pixel%u,%u,%u\n",c,linear,pixel[0],pixel[1],pixel[2]);
            assert(!memcmp(pixel,texels+expected[c]*4,4));
        }
        /* A quad straddles x=0: abs would erase its derivative and wrongly
         * select red level0. Original derivative is four texels -> green LOD2.
         * Repeat beyond the clamp edge, where clamped gradients would vanish. */
        uint8_t mip[64*64*4];
        for(unsigned level=0;level<=6;++level){unsigned side=64u>>level;
            for(unsigned i=0;i<side*side;++i){mip[i*4]=level==0?255:0;mip[i*4+1]=level==2?255:0;mip[i*4+2]=level!=0&&level!=2?255:0;mip[i*4+3]=255;}
            glTexImage2D(GL_TEXTURE_2D,level,GL_RGBA8,side,side,0,GL_RGBA,GL_UNSIGNED_BYTE,mip);
        }
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST_MIPMAP_NEAREST);
        glUniform1f(projection,1);glUniform2i(mirror,1,1);
        for(unsigned edge=0;edge<2;++edge){float factor=linear?64:1;
            glUniform4f(coords,(edge?2.0f:-9.0f/16)*factor,.25f*factor,factor,0);
            glDrawArrays(GL_TRIANGLES,0,3);uint8_t pixel[4];glReadPixels(8,8,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
            assert(pixel[0]==0&&pixel[1]==255&&pixel[2]==0&&pixel[3]==255);
        }
        glDeleteProgram(program);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,texels);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    }
    glDeleteTextures(1,&texture);glDeleteVertexArrays(1,&vao);glViewport(0,0,320,240);
    assert(shader_wrap(5)==GL_CLAMP_TO_EDGE && glGetError()==GL_NO_ERROR);
    puts("PASS: MIRRORONCE normalized/linear projected sampling, per-axis controls, negative q, outside clamp, original seam/clamp mip gradients");
}
