#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef void (*recomp_func_t)(void);
extern volatile uint32_t g_icall_trace[16];
extern volatile uint32_t g_icall_trace_idx;
extern recomp_func_t wrath_graphics_lookup(uint32_t address);
extern recomp_func_t wrath_input_lookup(uint32_t address);
extern recomp_func_t wrath_crt_lookup(uint32_t address);
extern void sub_000FEC90(void);

recomp_func_t recomp_lookup_manual(uint32_t address)
{
    if (address == 0x000FEC90u) return sub_000FEC90;
    recomp_func_t function = wrath_graphics_lookup(address);
    if (!function) function = wrath_input_lookup(address);
    return function ? function : wrath_crt_lookup(address);
}

static void unresolved(uint32_t address, const char *reason)
{
    fprintf(stderr, "Unresolved compiled call 0x%08X (%s)\n", address, reason);
    for (unsigned i = 0; i < 16; ++i) {
        unsigned index = (g_icall_trace_idx + i) & 15;
        fprintf(stderr, "  call[%u] = 0x%08X\n", i, g_icall_trace[index]);
    }
    /* Stop on missing logic instead of silently reporting a successful boot. */
    abort();
}

void recomp_icall_fail_log(uint32_t address) { unresolved(address, "no generated function"); }
void recomp_icall_not_code_log(uint32_t address) { unresolved(address, "outside code"); }
void recomp_unsupported_instruction(uint32_t address) { unresolved(address, "instruction not lifted"); }
