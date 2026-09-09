/* Included by the Android-only title shader bridge after GL declarations.
 * Caller retains the graphics mutex and output array until synchronous return. */
#include "gl_rpc.h"
static void wumpa_shader_sampler_queries_execute(void *opaque)
{
    GLint *values=opaque;
    for(unsigned i=0;i<4;++i) {
        glActiveTexture(GL_TEXTURE0+i);
        glGetIntegerv(GL_SAMPLER_BINDING,&values[i]);
    }
}
static void wumpa_shader_sampler_queries(GLint values[4])
{
    /* Preserve all earlier commands before entering the owner callback. The
     * final active unit remains 3, exactly like the original four query pairs. */
    wumpa_gl_flush_state();
    if(wumpa_gl_rpc_active() && !wumpa_gl_rpc_owner())
        wumpa_gl_rpc_call_named("ShaderSamplerQueries",wumpa_shader_sampler_queries_execute,values);
    else
        wumpa_shader_sampler_queries_execute(values);
}
