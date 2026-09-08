/* Xbox4361 indexed draws. Include after draw_vertices_data and resource helpers.
 * Original SDK evidence:100D00 allocates length+12, Data=header+12, Common01010001;
 * FFEA0 binds+0x80000 and saves base at device+1C;101BC0 copies literal16-bit
 * indices into NV2A methods1800/1808.108F40 offsets stream data by base*stride.
 * Parent supplies RESOURCE_INDEX_BUFFER7, kind0x10000 in update_common, and
 * release_resource frees this contiguous allocation once, at its handle.
 * Shared100D40 palette/index lock is supplied by the palette bridge.
 */
static uint32_t s_index_handle;

static int index_range(uint32_t address, uint64_t bytes)
{
    return address>=0x1000u && address<0x4000000u && bytes<=0x4000000u-address;
}
void sub_00100D00(void) /* CreateIndexBuffer(length,usage,format,pool,out), ret20. */
{
    uint32_t bytes=arg(0),out=arg(4);
    if (!s_device || !bytes || (bytes&1) || bytes>0x4000000u-12 ||
        !index_range(out,4)) {
        finish(20,(uint32_t)D3DERR_INVALIDCALL); return;
    }
    write32(out,0);
    struct Resource *r=new_resource();
    if (!r) { finish(20,0x8007000Eu); return; }
    uint32_t handle=xbox_HeapAlloc(bytes+12,16);
    if (!handle) { finish(20,0x8007000Eu); return; }
    memset(r,0,sizeof(*r));
    r->handle=handle; r->data=handle+12; r->bytes=bytes;
    r->type=RESOURCE_INDEX_BUFFER; r->references=1; r->format=0x2c;
    /* Retail100D00 reads only length and out. Usage/format/pool are ignored;
     * D3DX111CC7 passes PC INDEX16=0x65, whereas game3A140 passes Xbox0x2c.
     * Both create the same header and16-bit index storage. */
    update_common(r); write32(handle+4,r->data); write32(handle+8,0);
    write32(out,handle); finish(20,0);
}
void sub_000FFEA0(void) /* SetIndices(buffer,baseVertex), ret8. */
{
    uint32_t handle=arg(0),base=arg(1);
    struct Resource *r=resource(handle);
    if (!s_device || (handle && (!r || r->type!=RESOURCE_INDEX_BUFFER))) {
        finish(8,(uint32_t)D3DERR_INVALIDCALL); return;
    }
    if (handle!=s_index_handle) {
        if (r) { ++r->bindings; update_common(r); }
        struct Resource *old=resource(s_index_handle);
        if (old) { --old->bindings; release_resource(old); }
        s_index_handle=handle;
    }
    write32(GUEST_DEVICE+0x38c,handle);
    write32(GUEST_DEVICE+0x1c,base);
    write32(0x10ec0c,r?r->data:0);
    finish(8,0);
}
static HRESULT draw_indexed_vertices(uint32_t type,uint32_t count,uint32_t indices)
{
    if (!count) return 0;
    struct Resource *vb=resource(s_stream_handle);
    uint32_t stride=s_stream_stride,base=read32(GUEST_DEVICE+0x1c);
    if (!s_device || !vb || vb->type!=RESOURCE_VERTEX_BUFFER || !stride ||
        stride>1024 || count>1024u*1024u || (uint64_t)count*stride>64u*1024u*1024u ||
        !index_range(indices,(uint64_t)count*2) || !index_range(vb->data,vb->bytes))
        return D3DERR_INVALIDCALL;
    /* The API accepts a literal CPU pointer, even with no index buffer bound.
     * If it points into a known resource, also enforce that resource's extent. */
    for (unsigned i=0;i<resource_capacity();++i) {
        struct Resource *r=resource_at(i);
        if (r->handle && r->type==RESOURCE_INDEX_BUFFER && indices>=r->data &&
            (uint64_t)indices<=(uint64_t)r->data+r->bytes &&
            (uint64_t)indices+(uint64_t)count*2>(uint64_t)r->data+r->bytes)
            return D3DERR_INVALIDCALL;
    }
    const uint8_t *in=guest_ptr(indices);
    /* Validate all fetches before allocation or drawing. 64-bit arithmetic
     * rejects base/index/stride overflow instead of wrapping into guest RAM. */
    for (uint32_t i=0;i<count;++i) {
        uint32_t index=in[i*2]|((uint32_t)in[i*2+1]<<8);
        if (((uint64_t)base+index+1)*stride>vb->bytes) return D3DERR_INVALIDCALL;
    }
    uint8_t *expanded=malloc((size_t)count*stride);
    if (!expanded) return (HRESULT)0x8007000Eu;
    const uint8_t *vertices=guest_ptr(vb->data);
    for (uint32_t i=0;i<count;++i) {
        uint32_t index=in[i*2]|((uint32_t)in[i*2+1]<<8);
        memcpy(expanded+(size_t)i*stride,vertices+((size_t)base+index)*stride,stride);
    }
    HRESULT result=draw_vertices_data(type,count,expanded,stride);
    free(expanded); return result;
}
void sub_00101BC0(void) /* DrawIndexedVertices(type,indexCount,uint16Pointer), ret12. */
{
    uint32_t type=arg(0),count=arg(1),indices=arg(2);
    HRESULT result=draw_indexed_vertices(type,count,indices);
    if (result<0) fprintf(stderr,"[wrath graphics] indexed draw rejected: type%u count%u indices0x%X base%u stream0x%X stride%u result0x%X\n",
        type,count,indices,read32(GUEST_DEVICE+0x1c),s_stream_handle,s_stream_stride,(unsigned)result);
    finish(12,(uint32_t)result);
}
