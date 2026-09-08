/* Include after test_state/call helpers and run after test_shader_bridge(). */
static void test_mixed_shader_bridge(void)
{
    uint32_t old_stages[4][32];
    memcpy(old_stages, guest_ptr(0x10EC18), sizeof(old_stages));
    uint32_t old_factor = read32(0x10F01C);
    uint32_t off[] = {0}, target[] = {s_backbuffer_handle, 0};
    call(0xFEF20, target, 2); call(0xFDAD0, off, 1);
    call(0xFE5C0, off, 1); call(0xFE660, off, 1);
    test_state(0x304, 0); test_state(0x300, 0); test_state(0x358, 0x1010101);
    write32(0x10EF60, 0); write32(0x10EF8C, 0);
    uint32_t textures[2];
    const uint32_t texels[2] = {0xFF4080C0, 0x40402010};
    for (unsigned i = 0; i < 2; ++i) {
        uint32_t create[] = {1, 1, 1, 0, 6, 0, 0xB000};
        call(0xFE9C0, create, 7); assert(g_eax == 0);
        textures[i] = read32(0xB000); write32(read32(textures[i] + 4), texels[i]);
        uint32_t bind[] = {i, textures[i]}; call(0xFFC90, bind, 2); assert(g_eax == 0);
    }
    uint32_t stages[4][32] = {{0}};
    for (unsigned i = 0; i < 4; ++i) {
        stages[i][0] = stages[i][1] = stages[i][3] = stages[i][4] = 1;
        stages[i][12] = stages[i][16] = stages[i][20] = 1;
        stages[i][28] = i;
    }
    stages[0][12] = stages[0][16] = 4; stages[0][14] = stages[0][18] = 2;
    stages[1][12] = 7; stages[1][14] = 2; stages[1][15] = 1;
    memcpy(guest_ptr(0x10EC18), stages, sizeof(stages));
    /* Four genuine NV2A MOV instructions pass position, diffuse and both UVs.
     * The original vertex declaration maps v3 to float4 diffuse at offset16. */
    uint32_t object = 0xC000; memset(guest_ptr(object), 0, 0x180);
    write32(object, 1); write32(object + 4, 0x10); write32(object + 8, 4); write32(object + 12, 17);
    for (unsigned i = 0; i < 16; ++i) write32(object + 0x14 + i * 16 + 8, 2);
    const unsigned regs[4] = {0, 3, 9, 10}, offsets[4] = {0, 16, 32, 40};
    for (unsigned i = 0; i < 4; ++i) {
        write32(object + 0x14 + regs[i] * 16 + 4, offsets[i]);
        write32(object + 0x14 + regs[i] * 16 + 8, i < 2 ? 0x42 : 0x22);
        uint32_t instruction[4] = {0, (1u << 21) | (regs[i] << 9) | 0x1b,
            2u << 26, (15u << 12) | (1u << 11) | (regs[i] << 3) | (i == 3)};
        memcpy(guest_ptr(object + 0x118 + i * 16), instruction, sizeof(instruction));
    }
    write32(object + 0x114, (16u << 18) | 0xB00);
    uint32_t setvs[] = {object + 1}; call(0x102940, setvs, 1); assert(g_eax == 0);
    call(0x102BB0, off, 1); assert(g_eax == 0 && s_vertex_handle && !s_pixel_handle);
    struct { float position[4], color[4], uv0[2], uv1[2]; } vertices[4] = {
        {{40,40,0,1},{.5,.25,.75,.5},{.5,.5},{.5,.5}},
        {{80,40,0,1},{.5,.25,.75,.5},{.5,.5},{.5,.5}},
        {{80,80,0,1},{.5,.25,.75,.5},{.5,.5},{.5,.5}},
        {{40,80,0,1},{.5,.25,.75,.5},{.5,.5},{.5,.5}}};
    memcpy(guest_ptr(0xF000), vertices, sizeof(vertices));
    uint32_t draw[] = {8, 4, 0xF000, sizeof(vertices[0])};
    call(0x1019C0, draw, 4); assert(g_eax == 0);
    uint8_t pixel[4]; glReadPixels(60, 240 - 60, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    const unsigned expected[4] = {96, 64, 160, 128};
    for (unsigned i = 0; i < 4; ++i) assert(abs((int)pixel[i] - (int)expected[i]) <= 1);
    /* Texture-factor-only variant then changes its uniform without compiling a
     * new program. A stale previously bound programmed-pixel definition fails. */
    stages[0][12] = stages[0][16] = 2; stages[0][14] = stages[0][18] = 3;
    stages[1][12] = 1; memcpy(guest_ptr(0x10EC18), stages, sizeof(stages));
    write32(0x10F01C, 0xFF2080C0); call(0x1019C0, draw, 4); assert(g_eax == 0);
    unsigned before = 0; for (unsigned i = 0; i < SHADER_CACHE_LIMIT; ++i) before += s_shader_programs[i].program != 0;
    write32(0x10F01C, 0xFF402010); call(0x1019C0, draw, 4); assert(g_eax == 0);
    unsigned after = 0; for (unsigned i = 0; i < SHADER_CACHE_LIMIT; ++i) after += s_shader_programs[i].program != 0;
    assert(before == after);
    glReadPixels(60, 240 - 60, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    assert(pixel[0] == 64 && pixel[1] == 32 && pixel[2] == 16 && pixel[3] == 255);
    for (unsigned i = 0; i < 2; ++i) {
        uint32_t unbind[] = {i, 0}, release[] = {textures[i]};
        call(0xFFC90, unbind, 2); call(0x103AD0, release, 1); assert(!resource(textures[i]));
    }
    uint32_t fvf[] = {D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1}; call(0x102940, fvf, 1);
    memcpy(guest_ptr(0x10EC18), old_stages, sizeof(old_stages)); write32(0x10F01C, old_factor);
    assert(glGetError() == GL_NO_ERROR);
    puts("PASS: programmed vertex + actual fixed texture-stage equations, preserved alpha, live factor uniforms and program cache reuse");
}
