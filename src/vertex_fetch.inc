/* Real secondary-stream fetches use the same original vertex numbering as
 * stream0. Include after resource helpers and before the shader bridge. */
struct VertexFetch {
    uint32_t first;
    const uint8_t *indices; /* Optional literal little-endian Xbox uint16 indices. */
    int quads;
};
static uint64_t vertex_fetch_index(const struct VertexFetch *fetch, uint32_t output)
{
    uint32_t source = output;
    if (fetch && fetch->quads) {
        static const uint8_t order[6] = {0,1,2,0,2,3};
        source = output / 6 * 4 + order[output % 6];
    }
    uint64_t vertex = source;
    if (fetch && fetch->indices)
        vertex = fetch->indices[(size_t)source*2] | ((uint32_t)fetch->indices[(size_t)source*2+1]<<8);
    return vertex + (fetch ? fetch->first : 0);
}
static HRESULT gather_vertex_attribute(unsigned stream, uint32_t offset, uint32_t bytes,
                                       uint32_t count, const struct VertexFetch *fetch, uint8_t **out)
{
    *out = NULL;
    if (!stream || stream >= 16 || !bytes || !count || (uint64_t)count*bytes > 64u*1024u*1024u)
        return D3DERR_INVALIDCALL;
    uint32_t record = 0x10F280 + stream*12;
    uint32_t stride = read32(record), displacement = read32(record+4), handle = read32(record+8);
    struct Resource *r = resource(handle);
    if (!r || r->type != RESOURCE_VERTEX_BUFFER || stride > 1024 ||
        r->data < 0x1000u || (uint64_t)r->data+r->bytes > 0x4000000u)
        return D3DERR_INVALIDCALL;
    /* Zero stride deliberately repeats one actual buffer element. A missing
     * handle is different: NV2A retains its address, which we do not invent. */
    uint64_t start = (uint64_t)displacement + offset;
    for (uint32_t i=0; i<count; ++i) {
        uint64_t address = start + vertex_fetch_index(fetch,i)*stride;
        if (address > r->bytes || bytes > r->bytes-address) return D3DERR_INVALIDCALL;
    }
    uint8_t *result = malloc((size_t)count*bytes);
    if (!result) return (HRESULT)0x8007000Eu;
    const uint8_t *source = guest_ptr(r->data);
    for (uint32_t i=0; i<count; ++i) {
        uint64_t address = start + vertex_fetch_index(fetch,i)*stride;
        memcpy(result+(size_t)i*bytes,source+(size_t)address,bytes);
    }
    *out = result;
    return 0;
}
