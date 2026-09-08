/* Native SDL audio ring and real APU producer tests, without game assets.
 * Compile with apu_vp.c, apu_dsp.c and libplatform.a (see docs/AUDIO.md).
 * Including implementations gives deterministic callback/cadence assertions.
 */
#include <assert.h>
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include "../third_party/xboxrecomp/src/apu/apu_xaudio2.c"
#include "../third_party/xboxrecomp/src/apu/apu.h"

static void ring_tests(void)
{
    assert(SDL_InitSubSystem(SDL_INIT_AUDIO) == 0);
    assert(xa2_init());
    assert(xa2_init());
    assert(xa2_is_active());
    assert(xa2_get_buffer_size() == 256);
    /* Keep the actual device paused for deterministic callback checks. */
    g_sdl_started = 1;
    int16_t input[SDL_PCM_CAPACITY][2], output[SDL_PCM_CAPACITY][2];
    for (unsigned i = 0; i < SDL_PCM_CAPACITY; ++i) {
        input[i][0] = (int16_t)i;
        input[i][1] = (int16_t)-(int)i;
    }
    assert(!xa2_submit_samples(NULL, 256));
    assert(!xa2_submit_samples(&input[0][0], 0));
    assert(!xa2_submit_samples(&input[0][0], -1));
    assert(!xa2_submit_samples(&input[0][0], SDL_PCM_CAPACITY + 1));
    assert(xa2_submit_samples(&input[0][0], SDL_PCM_CAPACITY));
    assert(!xa2_submit_samples(&input[0][0], 1));
    sdl_audio_callback(NULL, (Uint8 *)output, 2731 * 4);
    assert(memcmp(input, output, 2731 * 4) == 0);
    assert(xa2_submit_samples(&input[0][0], 1000));
    sdl_audio_callback(NULL, (Uint8 *)output, sizeof(output));
    assert(memcmp(input[2731], output, 341 * 4) == 0);
    assert(memcmp(input, output[341], 1000 * 4) == 0);
    for (unsigned i = 1341; i < SDL_PCM_CAPACITY; ++i)
        assert(output[i][0] == 0 && output[i][1] == 0);
    assert(g_sdl_queued == 0 && g_sdl_underruns == 1 && g_sdl_overflows == 1);
    xa2_shutdown();
    xa2_shutdown();
    assert(!xa2_is_active());
    assert(!xa2_submit_samples(&input[0][0], 1));
    assert(SDL_WasInit(SDL_INIT_AUDIO)); /* Other owner's reference survives. */
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    assert(!SDL_WasInit(SDL_INIT_AUDIO));
}

static void producer_test(void)
{
    /* Software voice path doesn't address Xbox RAM, so no 64MB allocation. */
    MCPXAPUState *d = mcpx_apu_init_standalone(NULL);
    assert(d && xa2_is_active());
    int16_t pcm[48000][2] = {{0}}; /* Silent one-second looping PCM. */
    int slot = apu_mixer_alloc_voice();
    assert(slot >= 0);
    APUMixerVoice *voice = apu_mixer_get_voice(slot);
    voice->pcm_data = &pcm[0][0];
    voice->pcm_bytes = sizeof(pcm);
    voice->num_channels = 2;
    voice->sample_rate = 48000;
    apu_mixer_play(slot, 1);
    Uint64 begin = SDL_GetPerformanceCounter();
    SDL_Delay(180);
    double elapsed = (double)(SDL_GetPerformanceCounter() - begin) /
                     (double)SDL_GetPerformanceFrequency();
    SDL_LockAudioDevice(g_sdl_device);
    uint64_t submitted = g_sdl_submitted;
    uint64_t played = g_sdl_played;
    uint64_t overflow = g_sdl_overflows;
    SDL_UnlockAudioDevice(g_sdl_device);
    double expected = elapsed * 48000;
    fprintf(stderr, "[TEST] %.3fs: submitted %llu, played %llu, expected %.0f frames\n",
            elapsed, (unsigned long long)submitted, (unsigned long long)played, expected);
    assert(submitted > expected * .75 && submitted < expected * 1.25);
    assert(played > 0 && overflow == 0);
    /* Must terminate even with an actively looping software voice. */
    mcpx_apu_shutdown(d);
    assert(!xa2_is_active());
    apu_mixer_free_voice(slot);
}

int main(int argc, char **argv)
{
    /* Use '--native' to open CoreAudio; all samples remain silent except the
     * deterministic ring test, whose device remains paused throughout. */
    if (argc < 2 || strcmp(argv[1], "--native") != 0)
        assert(SDL_setenv("SDL_AUDIODRIVER", "dummy", 1) == 0);
    ring_tests();
    producer_test();
    puts("PASS: PCM order, wrap, bounded overflow, silence on underrun, SDL ownership, 48kHz pacing, active shutdown");
    return 0;
}
