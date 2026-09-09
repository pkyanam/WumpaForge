/* Future Android diagnostic: no game code, no guest execution, no graphics.
 * MAP_FIXED is used only after successfully owning the entire destination span.
 * A shell result must later be repeated inside the actual application sandbox. */
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

_Static_assert(sizeof(void *) == 8, "64-bit process required");
_Static_assert(sizeof(uint32_t) == 4, "Guest DWORD must remain 32 bits");

extern int wumpa_create_shared_backing(void);

static int owned_range(size_t offset, size_t length, size_t reserved, size_t page)
{
    return page && !(offset % page) && !(length % page) && length &&
           offset < reserved && length <= reserved - offset;
}

static void *alias(void *base, size_t reserved, size_t offset, size_t length,
                   size_t page, int fd)
{
    if (!owned_range(offset, length, reserved, page)) {
        errno = EINVAL;
        return MAP_FAILED;
    }
    return mmap((uint8_t *)base + offset, length, PROT_READ | PROT_WRITE,
                MAP_SHARED | MAP_FIXED, fd, 0);
}

int main(void)
{
    const size_t reserved = (size_t)UINT64_C(0x100000000);
    const size_t ram = 64 * 1024 * 1024;
    const size_t mirror_offset = (size_t)UINT64_C(0x80000000);
    long detected_page = sysconf(_SC_PAGESIZE);
    int fd = -1, result = 1;
    void *base = MAP_FAILED;
    const char *failure = "host page size";

    if (detected_page != 4096 && detected_page != 16384) goto done;
    size_t page = (size_t)detected_page;
    failure = "range contract";
    if (!owned_range(0, ram, reserved, page) ||
        !owned_range(mirror_offset, ram, reserved, page) ||
        owned_range(1, ram, reserved, page) ||
        owned_range(0, ram - 1, reserved, page) ||
        owned_range(reserved, page, reserved, page) ||
        owned_range(reserved - page, 2 * page, reserved, page)) goto done;

    failure = "sparse 4 GiB reservation";
    base = mmap(NULL, reserved, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (base == MAP_FAILED) goto done;
    failure = "memfd_create";
    fd = wumpa_create_shared_backing();
    if (fd < 0) goto done;
    failure = "shared backing size";
    if (ftruncate(fd, (off_t)ram)) goto done;

    failure = "64 MiB base and shared mirror";
    volatile uint8_t *low = alias(base, reserved, 0, ram, page, fd);
    volatile uint8_t *mirror = alias(base, reserved, mirror_offset, ram, page, fd);
    if ((void *)low == MAP_FAILED || (void *)mirror == MAP_FAILED) goto done;
    const size_t offsets[] = {0, page - 1, page, ram - 1};
    failure = "bidirectional shared alias bytes";
    for (size_t i = 0; i < sizeof(offsets) / sizeof(offsets[0]); ++i) {
        low[offsets[i]] = (uint8_t)(0x31 + i);
        if (mirror[offsets[i]] != (uint8_t)(0x31 + i)) goto done;
        mirror[offsets[i]] = (uint8_t)(0x91 + i);
        if (low[offsets[i]] != (uint8_t)(0x91 + i)) goto done;
    }

    failure = "mirror release and shared recommit";
    void *mirror_address = (uint8_t *)base + mirror_offset;
    if (mmap(mirror_address, ram, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
             -1, 0) == MAP_FAILED) goto done;
    mirror = alias(base, reserved, mirror_offset, ram, page, fd);
    if ((void *)mirror == MAP_FAILED) goto done;
    for (size_t i = 0; i < sizeof(offsets) / sizeof(offsets[0]); ++i)
        if (mirror[offsets[i]] != (uint8_t)(0x91 + i)) goto done;
    result = 0;

done:
    if (result) fprintf(stderr, "Memory probe failed at %s (errno=%d)\n", failure, errno);
    if (base != MAP_FAILED && munmap(base, reserved)) {
        perror("reservation cleanup");
        result = 1;
    }
    if (fd >= 0 && close(fd)) {
        perror("backing cleanup");
        result = 1;
    }
    if (!result) {
        printf("{\"probe\":\"sparse-memory-only\",\"pointer_bits\":64,"
               "\"host_page_size\":%ld,\"reserved_bytes\":%" PRIu64 ","
               "\"backing_bytes\":%zu,\"shared_aliases_passed\":true,"
               "\"android_game_supported\":false}\n",
               detected_page, (uint64_t)reserved, ram);
    }
    return result;
}
