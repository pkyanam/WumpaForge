#ifndef WRATH_NV2A_VERTEX_H
#define WRATH_NV2A_VERTEX_H
#include <stddef.h>
#include <stdint.h>

typedef struct Nv2aVertexInfo {
    unsigned instruction_count;
    uint16_t input_mask;
    uint16_t output_mask;
    uint64_t constant_mask[3]; /* Physical c0..c191; all bits for relative reads. */
    unsigned relative_constants; /* Program uses A0-relative constant reads. */
} Nv2aVertexInfo;

/* Input consists of count four-DWORD NV2A instructions, without upload packet
 * headers. Stops at FINAL. Returns 1 on success; 0 with precise error otherwise.
 * No GL calls. Partial output is cleared on error. Source buffer >=64 KB advised.
 * Interface: locations0..15 vec4 v0..v15; u_vconstants[192];
 * u_nv2a_viewport=(x,y,width,height); u_nv2a_depth=(integerMin,integerMax).
 * Outputs: vec4 vD0,vD1,vT0..vT3; float vFog; native gl_Position/gl_PointSize.
 */
int nv2a_vertex_generate(const uint32_t *words, size_t count,
                        char *source, size_t capacity, Nv2aVertexInfo *info,
                        char *error, size_t error_capacity);
#endif
