/* Test-only output sink for memory sanitizers, avoiding SDL's dynamic loader.
 * Links to the actual APU producer/mixer. Does not represent audible output. */
#include <assert.h>
#include <stdint.h>
#include <stdatomic.h>
#include "../third_party/xboxrecomp/src/apu/apu_xaudio2.h"
static atomic_int active;
int xa2_init(void) { atomic_store(&active, 1); return 1; }
void xa2_shutdown(void) { atomic_store(&active, 0); }
int xa2_is_active(void) { return atomic_load(&active); }
int xa2_get_buffer_size(void) { return 256; }
int xa2_submit_samples(const int16_t *samples, int frames)
{
    assert(samples && frames > 0 && frames <= 1024);
    /* Read samples so sanitizers check that the producer's span is live. */
    volatile int16_t value = samples[frames * 2 - 1]; (void)value;
    return atomic_load(&active);
}
