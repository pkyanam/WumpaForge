/* Real guest IndexBuffer APIs exercise page growth without thousands of GL
 * objects. The smoke's eight-page bound makes exhaustion small and repeatable. */
static void test_resource_pages(void)
{
    const unsigned limit = RESOURCE_PAGE_SLOTS * RESOURCE_MAX_PAGES;
    uint32_t *handles = calloc(limit, sizeof(*handles));
    assert(handles && limit > 1024);
    unsigned initial_live = 0;
    for (unsigned i = 0; i < resource_capacity(); ++i)
        initial_live += resource_at(i)->handle != 0;
    unsigned count = limit - initial_live;
    struct Resource *first = NULL, *boundary = NULL;
    uint32_t create[] = {2, 0, 0x2c, 0, 0x16044};
    for (unsigned i = 0; i < count; ++i) {
        call(0x100D00, create, 5); assert(g_eax == 0);
        handles[i] = read32(create[4]);
        struct Resource *r = resource(handles[i]);
        assert(r && r->type == RESOURCE_INDEX_BUFFER && r->references == 1);
        assert(read32(handles[i] + 4) == handles[i] + 12);
        uint16_t pattern = (uint16_t)(i ^ 0xa55a);
        memcpy(guest_ptr(r->data), &pattern, sizeof(pattern));
        if (!i) first = r;
        if (i == 1024) boundary = r;
        /* Existing pointers remain usable after every new page allocation. */
        assert(resource(handles[0]) == first && first->handle == handles[0]);
    }
    assert(count > 1024 && boundary && resource(handles[1024]) == boundary);
    assert(resource_capacity() == limit);
    call(0x100D00, create, 5);
    assert(g_eax == 0x8007000Eu && read32(create[4]) == 0);
    /* A supported texture fails with allocation evidence, not a format error. */
    uint32_t texture[] = {32, 32, 1, 0, 0x0f, 0, 0x16044};
    write32(texture[6], 0xdeadbeef);
    call(0xFE9C0, texture, 7);
    assert(g_eax == 0x8007000Eu && read32(texture[6]) == 0);
    /* Released storage is reused before growth, including beyond the old cap. */
    unsigned pages = s_resource_page_count;
    uint32_t released = handles[1024], release[] = {released};
    call(0x103AD0, release, 1); assert(g_eax == 0 && !resource(released));
    call(0x100D00, create, 5); assert(g_eax == 0);
    handles[1024] = read32(create[4]);
    assert(resource(handles[1024]) == boundary && s_resource_page_count == pages);
    /* Real guest data and header survived growth; no metadata aliasing. */
    for (unsigned i = 0; i < count; ++i) {
        struct Resource *r = resource(handles[i]);
        assert(r && read32(r->handle) == 0x01010001u);
        if (i != 1024) {
            uint16_t actual; memcpy(&actual, guest_ptr(r->data), sizeof(actual));
            assert(actual == (uint16_t)(i ^ 0xa55a));
        }
        release[0] = handles[i]; call(0x103AD0, release, 1);
        assert(g_eax == 0 && !resource(handles[i]));
    }
    unsigned remaining = 0;
    for (unsigned i = 0; i < resource_capacity(); ++i)
        remaining += resource_at(i)->handle != 0;
    assert(remaining == initial_live && s_resource_page_count == pages);
    free(handles);
    printf("PASS: %u real index resources, >1024 stable addresses/data, lazy pages, bounded exhaustion, exact release/reuse (record %zu bytes)\n", count, sizeof(struct Resource));
}
