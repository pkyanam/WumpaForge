#include <sys/mman.h>

/* Separate target-specific API use from the source-checkable mapping contract.
 * The NDK declaration is available from API 30. No syscall-number fallback. */
int wumpa_create_shared_backing(void)
{
    return memfd_create("wumpaforge-memory-probe", MFD_CLOEXEC);
}
