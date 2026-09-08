/* Standalone macOS compatibility-layer regression: no game assets required. */
#include "platform/win32_compat.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>

extern void *w32_reserve_guest_address_space(size_t size);
extern void w32_release_guest_address_space(void);

int main(void)
{
    const size_t page = (size_t)sysconf(_SC_PAGESIZE);
    const size_t ram_size = 64u * 1024u * 1024u;
    unsigned char *foreign = mmap(NULL, page, PROT_READ | PROT_WRITE,
                                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(foreign != MAP_FAILED);
    foreign[0] = 0xA7;
    HANDLE mapping = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL,
                                        PAGE_READWRITE, 0, ram_size, NULL);
    assert(mapping);
    /* Occupied host memory must survive exact-address allocation attempts. */
    assert(!MapViewOfFileEx(mapping, FILE_MAP_ALL_ACCESS, 0, 0, page, foreign));
    assert(foreign[0] == 0xA7);
    assert(!VirtualAlloc(foreign, page, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    assert(foreign[0] == 0xA7);
    assert(!MapViewOfFileEx(mapping, FILE_MAP_ALL_ACCESS, 0, ram_size, page, NULL));

    unsigned char *base = w32_reserve_guest_address_space((size_t)1 << 32);
    assert(base);
    assert((uintptr_t)base >= ((uintptr_t)1 << 32));
    assert(MapViewOfFileEx(mapping, FILE_MAP_ALL_ACCESS, 0, 0, ram_size, base) == base);
    unsigned char *mirror = base + ram_size;
    unsigned char *tiled = base + 0xF0000000u;
    assert(MapViewOfFileEx(mapping, FILE_MAP_ALL_ACCESS, 0, 0, ram_size, mirror) == mirror);
    assert(MapViewOfFileEx(mapping, FILE_MAP_ALL_ACCESS, 0, 0, ram_size, tiled) == tiled);
    base[0x1000] = 0x39;
    assert(mirror[0x1000] == 0x39 && tiled[0x1000] == 0x39);
    tiled[0x1000] = 0x82;
    assert(base[0x1000] == 0x82 && mirror[0x1000] == 0x82);
    unsigned char *aperture = base + 0xFD000000u;
    assert(VirtualAlloc(aperture, 16u << 20, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE) == aperture);
    aperture[0] = 0x51;
    assert(VirtualAlloc(aperture + 0x1000, 4096, MEM_COMMIT, PAGE_READWRITE) == aperture + 0x1000);
    assert(aperture[0] == 0x51);
    assert(VirtualFree(aperture, 16u << 20, MEM_RELEASE));
    assert(VirtualAlloc(aperture, 16u << 20, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE) == aperture);
    assert(aperture[0] == 0);
    assert(UnmapViewOfFile(tiled));
    assert(UnmapViewOfFile(mirror));
    assert(UnmapViewOfFile(base));
    assert(CloseHandle(mapping));
    w32_release_guest_address_space();
    assert(foreign[0] == 0xA7);
    munmap(foreign, page);
    puts("PASS: native arm64 sparse reservation, RAM aliases, aperture commits, bounds and host mapping preservation");
    return 0;
}
