/* Focused native DirectSound buffer/decoder test; no supplied game assets. */
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "fixtures/xbox_adpcm_golden.h"
#include "../third_party/xboxrecomp/src/audio/dsound_device.c"

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

int main(void)
{
    decoder_tests();
    assert(SDL_setenv("SDL_AUDIODRIVER", "dummy", 1) == 0);
    IDirectSound8 *device;
    assert(xbox_DirectSoundCreate(NULL, &device, NULL) == S_OK);
    XBOX_WAVEFORMATEX format = {0x69, 1, 22050, 12403, 36, 4, 2};
    _Static_assert(sizeof(format) >= 20, "native format capacity");
    uint16_t samples = 64;
    memcpy((uint8_t *)&format + 18, &samples, 2);
    DSBUFFERDESC description = {0};
    description.dwSize = sizeof(description);
    description.lpwfxFormat = &format;
    IDirectSoundBuffer8 *buffers[252];
    for (unsigned i = 0; i < 252; ++i) {
        assert(device->lpVtbl->CreateSoundBuffer(device, &description, &buffers[i], NULL) == S_OK);
        assert(buf_from_iface(buffers[i])->mixer_slot == -1);
    }
    int slots[APU_MIXER_MAX_VOICES];
    for (unsigned i = 0; i < APU_MIXER_MAX_VOICES; ++i) {
        slots[i] = apu_mixer_alloc_voice();
        assert(slots[i] == (int)i); /* Reserved, even before PCM is installed. */
    }
    assert(apu_mixer_alloc_voice() == -1);
    for (unsigned i = 0; i < APU_MIXER_MAX_VOICES; ++i) apu_mixer_free_voice(slots[i]);
    IDirectSoundBuffer8 *failed = (void *)1;
    format.wFormatTag = 0xFFFF;
    assert(device->lpVtbl->CreateSoundBuffer(device, &description, &failed, NULL) == DSERR_BADFORMAT && !failed);
    format.wFormatTag = 0x69;
    uint16_t bad_samples = 65;
    memcpy((uint8_t *)&format + 18, &bad_samples, 2);
    assert(device->lpVtbl->CreateSoundBuffer(device, &description, &failed, NULL) == DSERR_BADFORMAT);
    memcpy((uint8_t *)&format + 18, &samples, 2);
    XBOX_WAVEFORMATEX pcm_format = {1, 2, 48000, 192000, 4, 16, 0};
    DSBUFFERDESC pcm_desc = description;
    pcm_desc.lpwfxFormat = &pcm_format;
    pcm_desc.dwBufferBytes = 16;
    IDirectSoundBuffer8 *pcm_buffer;
    assert(device->lpVtbl->CreateSoundBuffer(device, &pcm_desc, &pcm_buffer, NULL) == S_OK);
    int16_t pcm_samples[8] = {100, -200, 300, -400, 500, -600, 700, -800};
    assert(pcm_buffer->lpVtbl->SetBufferData(pcm_buffer, pcm_samples, sizeof(pcm_samples)) == S_OK);
    assert(buf_from_iface(pcm_buffer)->pcm_data == buf_from_iface(pcm_buffer)->buffer_data);
    assert(memcmp(buf_from_iface(pcm_buffer)->pcm_data, pcm_samples, sizeof(pcm_samples)) == 0);
    assert(pcm_buffer->lpVtbl->Release(pcm_buffer) == 0);
    pcm_format.wBitsPerSample = 8;
    assert(device->lpVtbl->CreateSoundBuffer(device, &pcm_desc, &failed, NULL) == DSERR_BADFORMAT);
    for (unsigned i = 0; i < 65; ++i) {
        assert(buffers[i]->lpVtbl->SetBufferData(buffers[i], golden_mono_adpcm, 36) == S_OK);
        DSoundBuffer *internal = buf_from_iface(buffers[i]);
        assert(internal->pcm_frames == 64 && internal->mixer_slot == -1);
        assert(memcmp(internal->pcm_data, golden_mono_pcm, 128) == 0);
    }
    assert(buffers[0]->lpVtbl->Play(buffers[0], 0, 0, 1) == E_FAIL); /* No APU producer. */
    MCPXAPUState *apu = mcpx_apu_init_standalone(NULL);
    assert(apu);
    for (unsigned i = 0; i < 64; ++i)
        assert(buffers[i]->lpVtbl->Play(buffers[i], 0, 0, 1) == S_OK);
    assert(buffers[64]->lpVtbl->Play(buffers[64], 0, 0, 1) == DSERR_ALLOCATED);
    uint8_t malformed[36];memcpy(malformed, golden_mono_adpcm, 36);malformed[2] = 89;
    int original_slot = buf_from_iface(buffers[0])->mixer_slot;
    int16_t *original_pcm = buf_from_iface(buffers[0])->pcm_data;
    assert(buffers[0]->lpVtbl->SetBufferData(buffers[0], malformed, 36) == DSERR_BADFORMAT);
    assert(buf_from_iface(buffers[0])->mixer_slot == original_slot);
    assert(buf_from_iface(buffers[0])->pcm_data == original_pcm);
    assert(buffers[0]->lpVtbl->Play(buffers[0], 0, 0, 0) == S_OK);
    SDL_Delay(30); /* One-shot really completes in the native producer. */
    assert(buffers[64]->lpVtbl->Play(buffers[64], 0, 0, 1) == S_OK); /* Reaps completed slot. */
    assert(buf_from_iface(buffers[0])->mixer_slot == -1);
    for (unsigned i = 0; i < 65; ++i) assert(buffers[i]->lpVtbl->Stop(buffers[i]) == S_OK);
    assert(buffers[0]->lpVtbl->SetBufferData(buffers[0], golden_mono_adpcm, sizeof(golden_mono_adpcm)) == S_OK);
    assert(buffers[0]->lpVtbl->SetCurrentPosition(buffers[0], 36) == S_OK);
    DWORD position;
    assert(buffers[0]->lpVtbl->GetCurrentPosition(buffers[0], &position, NULL) == S_OK && position == 36);
    assert(buffers[0]->lpVtbl->SetCurrentPosition(buffers[0], 1) == E_INVALIDARG);
    assert(buffers[0]->lpVtbl->SetFrequency(buffers[0], 0) == S_OK);
    assert(buf_from_iface(buffers[0])->frequency == 22050);
    assert(buffers[0]->lpVtbl->SetVolume(buffers[0], -2000) == S_OK);
    assert(buffers[0]->lpVtbl->Play(buffers[0], 0, 0, 1) == S_OK);
    void *locked;DWORD bytes;
    assert(buffers[0]->lpVtbl->Lock(buffers[0], 0, 36, &locked, &bytes, NULL, NULL, 0) == S_OK);
    assert(buf_from_iface(buffers[0])->mixer_slot == -1);
    ((uint8_t *)locked)[0] ^= 1;
    assert(buffers[0]->lpVtbl->Unlock(buffers[0], locked, bytes, NULL, 0) == S_OK);
    assert(buf_from_iface(buffers[0])->pcm_data[0] == (int16_t)(golden_mono_pcm[0] ^ 1));
    IDirectSoundStream *stream = (void *)1;
    assert(device->lpVtbl->CreateSoundStream(device, NULL, &stream, NULL) == E_NOTIMPL && !stream);
    for (unsigned i = 0; i < 252; ++i) assert(buffers[i]->lpVtbl->Release(buffers[i]) == 0);
    assert(!s_buffers);
    for (unsigned i = 0; i < 64; ++i) {slots[i] = apu_mixer_alloc_voice();assert(slots[i] >= 0);}
    for (unsigned i = 0; i < 64; ++i) apu_mixer_free_voice(slots[i]);
    mcpx_apu_shutdown(apu);
    puts("PASS: vgmstream mono/stereo golden ADPCM, malformed input, 252 idle objects, 64 active voices, real completion/reclaim, compressed cursors, lock re-decode, explicit errors, cleanup");
    return 0;
}
