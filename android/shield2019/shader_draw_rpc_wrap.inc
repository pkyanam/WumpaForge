/* Caller keeps guest graphics/resource ownership while the owner runs the
 * audited host-only body. Never add SDK callbacks or event pumping here. */
struct WumpaShaderDrawRequest {
    GLenum primitive;
    uint32_t count,stride,diagnostic_return;
    const void *vertices;
    const struct VertexFetch *fetch;
    HRESULT result;
    uint64_t profile_delta[2];
};
static void wumpa_shader_draw_body(void *opaque)
{
    struct WumpaShaderDrawRequest *request=opaque;
    int old_valid=s_shader_rpc_diagnostic_valid;
    uint32_t old_return=s_shader_rpc_diagnostic_return;
    s_shader_rpc_diagnostic_valid=1;
    s_shader_rpc_diagnostic_return=request->diagnostic_return;
    request->result=shader_draw_impl(request->primitive,request->count,
                                    request->vertices,request->stride,request->fetch);
    s_shader_rpc_diagnostic_valid=old_valid;
    s_shader_rpc_diagnostic_return=old_return;
}
static void wumpa_shader_draw_execute(void *opaque)
{
    struct WumpaShaderDrawRequest *request=opaque;
    xbox_D3D8GLProfileHostCall(wumpa_shader_draw_body,request,request->profile_delta);
}
static HRESULT shader_draw(GLenum primitive,uint32_t count,const void *vertices,
                           uint32_t stride,const struct VertexFetch *fetch)
{
    const char *mode=getenv("WRATH_GL_RPC_SHADER");
    if(!mode || strcmp(mode,"1") || !wumpa_gl_rpc_active() || wumpa_gl_rpc_owner())
        return shader_draw_impl(primitive,count,vertices,stride,fetch);
    /* Production caller already owns this token. Direct fixture callers acquire
     * it here. No arbitrary guest execution occurs inside the owner callback. */
    int outer=xbox_D3D8GLBeginStateCall();
    wumpa_gl_flush_state();
    struct WumpaShaderDrawRequest request={.primitive=primitive,.count=count,
        .stride=stride,.diagnostic_return=shader_guest_return(),.vertices=vertices,.fetch=fetch};
    wumpa_gl_rpc_call_named("ShaderDraw",wumpa_shader_draw_execute,&request);
    xbox_D3D8GLProfileMergeDraw(request.profile_delta);
    xbox_D3D8GLEndCall(outer);
    return request.result;
}
