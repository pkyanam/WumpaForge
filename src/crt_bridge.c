/* Native implementation of the audited Xbox CRT memmove at 0xF5DD0.
 * The XBE routine chooses forward/backward copies by overlap, returns dst,
 * preserves ESI/EDI/EBP and ends in plain RET (cdecl). Its inline jump tables
 * currently confuse function discovery; no game logic is bypassed here. */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern _Thread_local uint32_t g_eax, g_esp;
extern ptrdiff_t g_xbox_mem_offset;
extern size_t g_xbox_total_ram;
typedef void (*recomp_func_t)(void);

static void *checked(uint32_t address, uint32_t count)
{
    if (address < 0x1000 || address > g_xbox_total_ram ||
        count > g_xbox_total_ram - address) {
        fprintf(stderr, "[wrath CRT] invalid RAM range %08X + %u\n", address, count);
        abort();
    }
    return (void *)((uintptr_t)address + g_xbox_mem_offset);
}
void sub_000F5DD0(void)
{
    uint32_t args[3];
    memcpy(args, checked(g_esp + 4, sizeof(args)), sizeof(args));
    if (args[2]) memmove(checked(args[0], args[2]), checked(args[1], args[2]), args[2]);
    g_eax = args[0];
    g_esp += 4;
}
recomp_func_t wrath_crt_lookup(uint32_t address)
{
    return address == 0x000F5DD0 ? sub_000F5DD0 : NULL;
}
