#include "kernel/xbox_optical_mode.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    uint8_t cdb[16] = {0x5a, 0, 0x3e, 0, 0, 0, 0, 0, 28};
    uint8_t result[32];
    size_t count;
    memset(result, 0xa5, sizeof(result));
    assert(xbox_optical_mode_sense(cdb, sizeof(cdb), result, 28, &count) == 0);
    assert(count == 28 && result[1] == 26 && result[8] == 0x3e && result[9] == 18);
    assert(result[10] == 1 && result[11] == 1 && result[12] == 1);
    assert(result[28] == 0xa5); /* No overwrite after the transfer. */
    assert(xbox_optical_mode_sense(cdb, sizeof(cdb), result, 27, &count) == 0xc0000023u);
    assert(count == 0);
    cdb[8] = 27;
    assert(xbox_optical_mode_sense(cdb, sizeof(cdb), result, 28, &count) == 0xc0000023u);
    cdb[8] = 28; cdb[2] = 0x3f;
    assert(xbox_optical_mode_sense(cdb, sizeof(cdb), result, 28, &count) == 0xc00000bbu);
    cdb[2] = 0x7e;
    assert(xbox_optical_mode_sense(cdb, sizeof(cdb), result, 28, &count) == 0xc00000bbu);
    cdb[2] = 0x3e; cdb[0] = 0x55;
    assert(xbox_optical_mode_sense(cdb, sizeof(cdb), result, 28, &count) == 0xc00000bbu);
    puts("PASS: optical status layout, transfer bounds and unsupported command rejection");
}
