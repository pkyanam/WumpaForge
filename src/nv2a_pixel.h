#ifndef WRATH_NV2A_PIXEL_H
#define WRATH_NV2A_PIXEL_H
#include <stddef.h>
#include <stdint.h>
/* Exact Xbox XDK wire layout: 60 little-endian DWORDs, no host pointers. */
typedef struct Nv2aPixelDef {
    uint32_t alpha_inputs[8], final_abcd, final_efg;
    uint32_t constant0[8], constant1[8], alpha_outputs[8], rgb_inputs[8];
    uint32_t compare_mode, final_constant0, final_constant1, rgb_outputs[8];
    uint32_t combiner_count, texture_modes, dot_mapping, input_texture;
    uint32_t c0_mapping, c1_mapping, final_constants;
} Nv2aPixelDef;
typedef struct Nv2aPixelOptions {
    /* Zero final words request the verified XDK fog/specular final combiner. */
    unsigned fog_enabled, specular_enabled;
    /* Linear Xbox texture coordinates are texels; normal textures are normalized. */
    unsigned rectangle_texture_mask;
} Nv2aPixelOptions;
typedef struct Nv2aPixelInfo {
    unsigned stages, texture_mask, sampler_dimension[4], uses_default_final;
    float constants[18][4]; /* upload as u_psconstants[18], RGBA */
    uint32_t c0_mapping, c1_mapping, final_constants;
} Nv2aPixelInfo;
/* GLSL410: vD0/vD1/vT0..vT3 vec4, vFog float; output fragColor.
 * Uniforms: tex0..tex3, u_psconstants[18], u_fogcolor (vec3), u_alpha_enable/u_alpha_func (int),
 * u_alpha_ref (normalized float); u_mirror_once0..3 (ivec2, PROJECT2D axes).
 * Returns 1 on success, 0 with bounded diagnostic and empty source on failure.
 * No GL calls or allocation. Supply at least 64 KiB source capacity. */
int nv2a_pixel_generate(const Nv2aPixelDef *definition, const Nv2aPixelOptions *options,
                        char *source, size_t source_capacity,
                        Nv2aPixelInfo *info, char *error, size_t error_capacity);
/* Decode the packed wire definition explicitly, allowing unaligned source. */
int nv2a_pixel_read_definition(const void *bytes, size_t length, Nv2aPixelDef *definition);
/* SetPixelShaderConstant compatibility: update an ACTIVE state copy, never the
 * immutable guest shader definition. All four-bit API mappings0..15 are supported.
 * Mapping15 is an ordinary match in the original SDK, not an ignored sentinel.
 * Mapping nibbles select all8 C0/C1 slots and the2 final slots. Returns0 on bad
 * arguments/range without changes. packed colors also feed device+0x3EC state. */
uint32_t nv2a_pixel_pack_constant(const float rgba[4]);
int nv2a_pixel_set_constants(Nv2aPixelDef *active, uint32_t index,
                             const float *rgba, uint32_t count);
/* Lower Xbox 4361 fixed texture stages to register-combiner equations.
 * states are the four 32-DWORD SDK caches (not PC D3D enum ordering).
 * dimensions: 0 unbound, 2 texture2D, 3 volume, 4 cube. Texture factor is ARGB.
 * Supports disabled/select/modulate/scaled-modulate/add/signed-add/subtract;
 * unsupported operations/state return0 with a zeroed definition and diagnostic.
 * ALPHAOP1 preserves current alpha; ALPHAOP0 is invalid, not an alias for1. */
int nv2a_pixel_fixed_definition(const uint32_t states[4][32],
                                const unsigned dimensions[4], uint32_t texture_factor,
                                Nv2aPixelDef *definition, char *error, size_t error_capacity);
#endif
