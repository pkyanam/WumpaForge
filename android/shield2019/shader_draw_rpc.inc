/* Android whole-shader seam: diagnostic context is not a guest register bank. */
#include "gl_rpc.h"
static _Thread_local int s_shader_rpc_diagnostic_valid;
static _Thread_local uint32_t s_shader_rpc_diagnostic_return;
static uint32_t shader_guest_return(void)
{
    return s_shader_rpc_diagnostic_valid ? s_shader_rpc_diagnostic_return : read32(g_esp);
}
extern void xbox_D3D8GLProfileHostCall(void (*callback)(void *),void *argument,uint64_t delta[2]);
extern void xbox_D3D8GLProfileMergeDraw(const uint64_t delta[2]);
