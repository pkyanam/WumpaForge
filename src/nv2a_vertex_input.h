#ifndef WRATH_NV2A_VERTEX_INPUT_H
#define WRATH_NV2A_VERTEX_INPUT_H
#include <stddef.h>
#include <stdint.h>

typedef enum Nv2aVertexKind {
    NV2A_VERTEX_NONE,
    NV2A_VERTEX_FLOAT32,
    NV2A_VERTEX_BGRA8,
    NV2A_VERTEX_SINT16,
    NV2A_VERTEX_UNORM8,
    NV2A_VERTEX_SNORM11_11_10,
    NV2A_VERTEX_FLOAT2H
} Nv2aVertexKind;

typedef struct Nv2aVertexSlot {
    uint32_t stream, offset, format;
    Nv2aVertexKind kind;
    uint8_t components, byte_count, normalized;
    uint8_t tessellation_type, tessellation_source;
} Nv2aVertexSlot;

typedef struct Nv2aVertexObject {
    uint32_t flags, instruction_count, packet_dwords;
    uint8_t dimensionality[4];
    Nv2aVertexSlot slots[16];
    uint32_t words[136][4];
    uint32_t constant_words[192][4];
    uint64_t constant_mask[3]; /* Physical indices; apply when shader loads. */
} Nv2aVertexObject;

/* Xbox 4361 object bytes, beginning at untagged object address. available must
 * include the zero terminator after packet_dwords. Misaligned input supported.
 * Returns1 success or0 with error. On error *result is zeroed, not partial.
 * Caller still validates actual stream bounds/stride and supports each kind.
 * SNORM11_11_10 requires packed decoding; FLOAT2H stores x,y,w in three floats.
 */
int nv2a_vertex_object_decode(const void *bytes, size_t available,
                             Nv2aVertexObject *result,
                             char *error, size_t error_capacity);

/* Decode a standalone Xbox declaration format into kind/components/byte_count/
 * normalized. Leaves stream/offset/tessellation fields untouched. */
int nv2a_vertex_format_decode(uint32_t format, Nv2aVertexSlot *slot,
                             char *error, size_t error_capacity);

/* Conservative live-uniform proof, not vertex-program execution. Returns1
 * only when every actual read of input is MUL/MAD A or B and its other operand
 * is a direct constant swizzled entirely from exact zero components. Unused
 * inputs also return1. Unknown opcodes, missing FINAL, relative constants and
 * every other use return0. Caller must recompute after constant changes. */
int nv2a_vertex_input_is_dead(const uint32_t *words, size_t count, unsigned input,
                             const float constants[192][4]);
#endif
