    { /* Exact production shader path: baseline pixels versus real indexed GPU. */
        const char *previous_mode=getenv("WRATH_INDEXED_SHADER");
        char *saved_mode=previous_mode?strdup(previous_mode):NULL;
        uint32_t saved_stream=s_stream_handle,saved_stride=s_stream_stride;
        uint32_t saved_offset=read32(0x10F284),saved_base=read32(GUEST_DEVICE+0x1c);
        uint32_t saved_index=s_index_handle;
        uint32_t make[]={sizeof(vertices)+2*sizeof(vertices[0]),0,0,0,0x17000};
        call(0x100D70,make,5);assert(g_eax==0);
        uint32_t indexed_vb=read32(0x17000),vertex_data=read32(indexed_vb+4);
        memset(guest_ptr(vertex_data),0,make[0]);
        memcpy(guest_ptr(vertex_data+2*sizeof(vertices[0])),vertices,sizeof(vertices));
        uint32_t bind_stream[]={0,indexed_vb,sizeof(vertices[0])};call(0x102580,bind_stream,3);
        write32(0x10F284,sizeof(vertices[0]));
        write32(0x10EC10,read32(0x10EC10)|0x40);
        uint32_t make_ib[]={14,0,0,0,0x17004};call(0x100D00,make_ib,5);assert(g_eax==0);
        uint32_t indexed_ib=read32(0x17004),index_data=read32(indexed_ib+4);
        uint16_t original_indices[]={0xffff,0,1,2,0,2,3};
        memcpy(guest_ptr(index_data),original_indices,sizeof(original_indices));
        uint32_t bind_index[]={indexed_ib,1};call(0xFFEA0,bind_index,2);
        uint32_t indexed_draw[]={5,6,index_data+2};
        uint8_t baseline[60*60*4],optimized[sizeof(baseline)],first[sizeof(baseline)];
        GLboolean old_restart=glIsEnabled(GL_PRIMITIVE_RESTART);
        GLboolean old_fixed=glIsEnabled(GL_PRIMITIVE_RESTART_FIXED_INDEX);
        glEnable(GL_PRIMITIVE_RESTART);glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
        for(unsigned mutation=0;mutation<3;++mutation) {
            if(mutation==1) { /* Retained guest pointer changes position without Unlock. */
                for(unsigned i=0;i<4;++i) {
                    float x=vertices[i].x+10;
                    memcpy(guest_ptr(vertex_data+(i+2)*sizeof(vertices[0])),&x,sizeof(x));
                }
            }
            if(mutation==2) { /* Alias into the live IB changes the second triangle. */
                uint16_t degenerate[]={0,0,0};memcpy(guest_ptr(index_data+8),degenerate,sizeof(degenerate));
            }
            setenv("WRATH_INDEXED_SHADER","0",1);
            call(0x100EA0,clear,6);call(0x101BC0,indexed_draw,3);assert(g_eax==0);
            glReadPixels(35,240-90,60,60,GL_RGBA,GL_UNSIGNED_BYTE,baseline);
            uint64_t before=s_indexed_shader_hits;
            setenv("WRATH_INDEXED_SHADER","1",1);
            call(0x100EA0,clear,6);call(0x101BC0,indexed_draw,3);assert(g_eax==0);
            assert(s_indexed_shader_hits==before+1); /* Explicit eligible hit. */
            glReadPixels(35,240-90,60,60,GL_RGBA,GL_UNSIGNED_BYTE,optimized);
            assert(!memcmp(baseline,optimized,sizeof(baseline)));
            assert(glIsEnabled(GL_PRIMITIVE_RESTART) && glIsEnabled(GL_PRIMITIVE_RESTART_FIXED_INDEX));
            if(!mutation) {
                memcpy(first,baseline,sizeof(first));
                unsigned lit=0;for(unsigned i=0;i<sizeof(first);i+=4)lit+=first[i]||first[i+1]||first[i+2];
                assert(lit>100); /* Comparing two blank outputs cannot pass. */
            } else assert(memcmp(first,baseline,sizeof(first)));
        }
        /* A subsequent ordinary shader draw must ignore the temporary EBO. */
        call(0x100EA0,clear,6);call(0x1019C0,draw,4);assert(g_eax==0);
        glReadPixels(35,240-90,60,60,GL_RGBA,GL_UNSIGNED_BYTE,optimized);
        assert(!memcmp(first,optimized,sizeof(first)));
        uint64_t before=s_indexed_shader_hits;
        indexed_draw[2]=index_data+4;call(0x101BC0,indexed_draw,3);
        assert(g_eax==(uint32_t)D3DERR_INVALIDCALL && s_indexed_shader_hits==before);
        bind_index[1]=UINT32_MAX;call(0xFFEA0,bind_index,2);
        indexed_draw[2]=index_data+2;call(0x101BC0,indexed_draw,3);
        assert(g_eax==(uint32_t)D3DERR_INVALIDCALL && s_indexed_shader_hits==before);
        if(!old_restart)glDisable(GL_PRIMITIVE_RESTART);
        if(!old_fixed)glDisable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
        bind_index[0]=saved_index;bind_index[1]=saved_base;call(0xFFEA0,bind_index,2);
        bind_stream[1]=saved_stream;bind_stream[2]=saved_stride;call(0x102580,bind_stream,3);
        write32(0x10F284,saved_offset);write32(0x10EC10,read32(0x10EC10)|0x40);
        uint32_t drop[]={indexed_vb};call(0x103AD0,drop,1);assert(!resource(indexed_vb));
        drop[0]=indexed_ib;call(0x103AD0,drop,1);assert(!resource(indexed_ib));
        if(saved_mode){setenv("WRATH_INDEXED_SHADER",saved_mode,1);free(saved_mode);}
        else unsetenv("WRATH_INDEXED_SHADER");
        assert(glGetError()==GL_NO_ERROR);
        puts("PASS: indexed shader eligible hits, exact baseline pixels, base/offset, guest alias mutations, restart/VAO state, bounds and lifetime");
    }
