/* Golden decoder test independent of the platform audio loader. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "fixtures/xbox_adpcm_golden.h"
#include "../third_party/xboxrecomp/src/audio/xbox_adpcm.h"

static void decoder_tests(void)
{
    int16_t output[512];
    assert(xbox_adpcm_decode(golden_mono_adpcm, sizeof(golden_mono_adpcm), 1, output, 512));
    assert(memcmp(output, golden_mono_pcm, sizeof(golden_mono_pcm)) == 0);
    assert(xbox_adpcm_decode(golden_stereo_adpcm, sizeof(golden_stereo_adpcm), 2, output, 256));
    assert(memcmp(output, golden_stereo_pcm, sizeof(golden_stereo_pcm)) == 0);
    uint8_t block[36];memcpy(block, golden_mono_adpcm, 36);
    block[35] ^= 0xF0; /* Unused 64th nibble never becomes a 65th sample. */
    assert(xbox_adpcm_decode(block, 36, 1, output, 64));
    assert(memcmp(output, golden_mono_pcm, 64 * 2) == 0);
    memset(output, 0xA5, sizeof(output));block[2] = 89;
    assert(!xbox_adpcm_decode(block, 36, 1, output, 64));
    for (unsigned i = 0; i < 512; ++i) assert((uint16_t)output[i] == 0xA5A5);
    assert(!xbox_adpcm_decode(golden_mono_adpcm, 35, 1, output, 64));
    assert(!xbox_adpcm_decode(golden_mono_adpcm, 36, 1, output, 63));
    assert(!xbox_adpcm_decode(golden_mono_adpcm, 36, 3, output, 64));
}

int main(void) { decoder_tests(); puts("PASS: Xbox ADPCM golden and malformed input cases"); return 0; }
