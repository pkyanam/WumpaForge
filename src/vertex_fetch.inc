/* Real secondary-stream fetches use the same original vertex numbering as
 * stream0. Include after resource helpers and before the shader bridge. */
#include "nv2a_vertex_input.h"
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
/* NV097 vertex-array addresses survive a NULL SetStreamSource. The original
 * 108F40 independently writes all formats/strides and only non-NULL addresses.
 * These are physical input slots (both4361 remapping tables are identity). */
struct VertexArrayAddress {
    uint32_t handle, data, bytes, stride, format;
    uint64_t generation, offset;
};
static struct VertexArrayAddress s_vertex_arrays[16];
static uint64_t s_vertex_generation;
static uint32_t s_vertex_array_base;
static int s_vertex_arrays_ready;
static void vertex_array_commit(const Nv2aVertexSlot slots[16], const struct VertexFetch *fetch)
{
    uint32_t dirty=read32(0x10EC10),base=fetch->indices?fetch->first:0;
    int formats=!s_vertex_arrays_ready || (dirty&0x10);
    int addresses=!s_vertex_arrays_ready || (dirty&0x40) || base!=s_vertex_array_base;
    /* 108F74..108F81 exits before the format loop too when neither dirty40
     * nor a changed base requests the array-address block. SetVertexShader
     * always sets70, covering declaration stream/offset and format changes. */
    if (!addresses) { write32(0x10EC10,dirty&~0x50u); return; }
    for (unsigned i=0;i<16;++i) {
        const Nv2aVertexSlot *slot=&slots[i];
        struct VertexArrayAddress *a=&s_vertex_arrays[i];
        if (slot->stream>=16) { a->generation=0; continue; }
        uint32_t record=0x10F280+12*slot->stream,stride=read32(record);
        if (formats) { a->stride=stride; a->format=slot->format; }
        uint32_t handle=read32(record+8);
        if (!addresses || slot->kind==NV2A_VERTEX_NONE || !handle) continue;
        struct Resource *r=resource(handle);
        if (!r || r->type!=RESOURCE_VERTEX_BUFFER) { a->generation=0; continue; }
        if (!r->vertex_generation) r->vertex_generation=++s_vertex_generation;
        a->handle=handle; a->data=r->data; a->bytes=r->bytes;
        a->generation=r->vertex_generation;
        /* Do not fetch shader-dead inputs here. Even an out-of-range address
         * can be programmed; its bytes are validated only if actually read. */
        a->offset=(uint64_t)read32(record+4)+slot->offset+(uint64_t)base*stride;
    }
    s_vertex_arrays_ready=1; s_vertex_array_base=base;
    write32(GUEST_DEVICE+0x20,base);
    write32(0x10EC10,dirty&~0x50u);
}
static int vertex_array_fvf_slots(uint32_t fvf, Nv2aVertexSlot slots[16])
{
    memset(slots,0,16*sizeof(*slots));
    for (unsigned i=0;i<16;++i) slots[i].format=2;
    unsigned position=fvf&0xE;
    if (position!=D3DFVF_XYZ && position!=D3DFVF_XYZRHW) return 0;
    unsigned offset=position==D3DFVF_XYZRHW?16:12;
    nv2a_vertex_format_decode(position==D3DFVF_XYZRHW?0x42:0x32,&slots[0],NULL,0);
    if (fvf&D3DFVF_NORMAL) {
        slots[2].offset=offset; offset+=12;
        nv2a_vertex_format_decode(0x32,&slots[2],NULL,0);
    }
    for (unsigned i=3;i<=4;++i) if(fvf&(i==3?D3DFVF_DIFFUSE:D3DFVF_SPECULAR)) {
        slots[i].offset=offset; offset+=4;
        nv2a_vertex_format_decode(0x40,&slots[i],NULL,0);
    }
    unsigned textures=(fvf>>8)&15,dimensions=fvf>>16;
    if (textures>7) return 0;
    const unsigned components[4]={2,3,4,1}; /* Original10B468, used by102030. */
    for (unsigned i=0;i<textures;++i) {
        unsigned n=components[(dimensions>>(2*i))&3];
        slots[9+i].offset=offset; offset+=4*n;
        nv2a_vertex_format_decode((n<<4)|2,&slots[9+i],NULL,0);
    }
    return 1;
}
static HRESULT gather_retained_vertex_attribute(unsigned attribute, const Nv2aVertexSlot *slot,
        uint32_t count,const struct VertexFetch *fetch,uint8_t **out)
{
    *out=NULL;
    if (attribute>=16 || !slot->byte_count || !count ||
        (uint64_t)count*slot->byte_count>64u*1024u*1024u) return D3DERR_INVALIDCALL;
    const struct VertexArrayAddress *a=&s_vertex_arrays[attribute];
    struct Resource *r=resource(a->handle);
    if (!a->generation || !r || r->type!=RESOURCE_VERTEX_BUFFER ||
        r->vertex_generation!=a->generation || r->data!=a->data || r->bytes!=a->bytes ||
        a->stride>1024 || a->format!=slot->format ||
        a->data<0x1000 || (uint64_t)a->data+a->bytes>0x4000000u) return D3DERR_INVALIDCALL;
    /* Indexed base was baked when this address last changed. The newly bound
     * index base cannot move a NULL address. DRAW_ARRAYS start still applies. */
    uint64_t indexed_base=fetch&&fetch->indices?fetch->first:0;
    for (uint32_t i=0;i<count;++i) {
        uint64_t index=vertex_fetch_index(fetch,i)-indexed_base;
        uint64_t offset=a->offset+index*a->stride;
        /* The original device binds one physical RAM DMA object to both
         * vertex inputs (104160), not a per-resource extent. A retained NULL
         * address can therefore reach adjacent RAM beyond its owner's size.
         * Keep ownership/generation checks above; validate the actual DMA
         * address before reading, without wrapping or inventing attributes. */
        uint64_t available=0x4000000ull-a->data;
        if (offset>available || slot->byte_count>available-offset) return D3DERR_INVALIDCALL;
    }
    uint8_t *data=malloc((size_t)count*slot->byte_count);
    if (!data) return (HRESULT)0x8007000Eu;
    for (uint32_t i=0;i<count;++i) {
        uint64_t offset=a->offset+(vertex_fetch_index(fetch,i)-indexed_base)*a->stride;
        memcpy(data+(size_t)i*slot->byte_count,(uint8_t *)guest_ptr(a->data)+offset,slot->byte_count);
    }
    *out=data;return 0;
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
