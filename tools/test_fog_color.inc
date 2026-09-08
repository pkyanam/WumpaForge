/* Exercise the fog register as an explicit pixel-combiner source while the
 * separate distance-fog enable remains off. The original color is always live. */
static void test_fog_color(const uint32_t *draw, const uint32_t *setps)
{
    Nv2aPixelDef definition = {0};
    definition.combiner_count = 1;
    /* Fog is an allowed final-combiner source, not a general-combiner source. */
    definition.final_abcd = 3; /* D=fog.rgb, A/B/C=0 */
    definition.final_efg = 0x1c80; /* alpha=r0.a=1 with no sampled textures */
    memcpy(guest_ptr(0xE00C), &definition, sizeof(definition));
    call(0x102BB0, setps, 1); assert(g_eax == 0);
    const uint32_t colors[] = {0x7f1234ab, 0xe1c08020};
    uint32_t before = read32(0x10EFF0), after = read32(0x10EFF8);
    uint32_t put = read32(GUEST_DEVICE), old_color = read32(0x10EFF4);
    assert(read32(0x10EF60) == 0);
    for (unsigned i = 0; i < 2; ++i) {
        call(0xFDA80, &colors[i], 1); assert(g_eax == 0);
        assert(read32(0x10EFF4) == colors[i] && read32(GUEST_DEVICE) == put);
        assert(read32(0x10EFF0) == before && read32(0x10EFF8) == after);
        call(0x1019C0, draw, 4); assert(g_eax == 0);
        uint8_t pixel[4]; glReadPixels(60,180,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
        assert(pixel[0] == ((colors[i] >> 16) & 255));
        assert(pixel[1] == ((colors[i] >> 8) & 255));
        assert(pixel[2] == (colors[i] & 255) && pixel[3] == 255);
    }
    call(0xFDA80, &old_color, 1);
    assert(glGetError() == GL_NO_ERROR);
    puts("PASS: original fog-color ABI/cache, no guest FIFO write, live nonblack RGB GPU combiner output");
}
