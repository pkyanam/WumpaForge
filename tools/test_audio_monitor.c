/* Verify that each group of eight 32-frame VP steps emits its actual hardware
 * mix, plus 256 software frames, and clears the accumulation buffer. The
 * backend is captured here; tools/test_audio.c tests the real SDL backend. */
#include <assert.h>
#include "../third_party/xboxrecomp/src/apu/apu_core.c"

static int16_t captured[256][2];
static int calls;
int xa2_init(void) { return 1; }
void xa2_shutdown(void) {}
int xa2_is_active(void) { return 1; }
int xa2_get_buffer_size(void) { return 256; }
int xa2_submit_samples(const int16_t *pcm, int frames)
{
    assert(frames == 256);
    memcpy(captured, pcm, sizeof(captured));
    ++calls;
    return 1;
}

int main(void)
{
    mixer_init();
    MCPXAPUState *d = calloc(1, sizeof(*d));
    assert(d);
    int16_t pcm[1024][2];
    for (int i = 0; i < 1024; ++i) {
        pcm[i][0] = 10;
        pcm[i][1] = -10;
    }
    int slot = apu_mixer_alloc_voice();
    APUMixerVoice *voice = apu_mixer_get_voice(slot);
    voice->pcm_data = &pcm[0][0];
    voice->pcm_bytes = sizeof(pcm);
    voice->sample_rate = 48000;
    apu_mixer_play(slot, 1);
    for (int i = 0; i < 256; ++i) {
        d->monitor.frame_buf[i][0] = 123;
        d->monitor.frame_buf[i][1] = -456;
    }
    for (d->ep_frame_div = 0; d->ep_frame_div < 8; ++d->ep_frame_div)
        mcpx_apu_monitor_frame(d);
    assert(calls == 1 && (voice->play_offset >> 16) == 256);
    for (int i = 0; i < 256; ++i) {
        assert(captured[i][0] == 133 && captured[i][1] == -466);
        assert(d->monitor.frame_buf[i][0] == 0 && d->monitor.frame_buf[i][1] == 0);
    }
    g_audio_muted = 1;
    d->ep_frame_div = 15;
    d->monitor.frame_buf[0][0] = 123;
    mcpx_apu_monitor_frame(d);
    assert(calls == 2);
    for (int i = 0; i < 256; ++i)
        assert(captured[i][0] == 0 && captured[i][1] == 0);
    g_audio_muted = 0;
    apu_mixer_free_voice(slot);
    free(d);
    puts("PASS: hardware mix preserved, software mix advances 256 frames, mute, next-frame clearing");
    return 0;
}
