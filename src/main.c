#include "kernel.h"
#include "xbox_memory_layout.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

extern void xbe_entry_point(void);
extern int recomp_dispatch_init(void);
extern void wrath_vblank_shutdown(void);
extern ptrdiff_t g_xbox_mem_offset;
extern RECOMP_TLS uint32_t g_esp;

int main(int argc, char **argv)
{
    char bundled_assets[4096];
    const char *directory = argc > 1 ? argv[1] : "local/assets";
#ifdef __APPLE__
    if (argc < 2) {
        char executable[4096]; uint32_t size = sizeof(executable);
        if (!_NSGetExecutablePath(executable, &size)) {
            char *base = strrchr(executable, '/');
            if (base) {
                *base = 0;
                snprintf(bundled_assets, sizeof(bundled_assets), "%s/../Resources/assets", executable);
                if (!access(bundled_assets, R_OK)) directory = bundled_assets;
            }
        }
    }
#endif
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    const char *boot_timeout = getenv("WRATH_BOOT_TIMEOUT");
    if (boot_timeout) alarm((unsigned)strtoul(boot_timeout, NULL, 10));
    char resolved[4096], xbe_path[4096], save_path[4096], run_path[4096];
    if (!realpath(directory, resolved)) {
        perror(directory);
        return 1;
    }
    snprintf(xbe_path, sizeof(xbe_path), "%s/default.xbe", resolved);
    snprintf(save_path, sizeof(save_path), "%s/../saves", resolved);
    snprintf(run_path, sizeof(run_path), "%s/../run", resolved);
    mkdir(run_path, 0755);
    if (chdir(run_path)) { perror(run_path); return 1; }
    FILE *input = fopen(xbe_path, "rb");
    if (!input) { perror(xbe_path); return 1; }
    if (fseek(input, 0, SEEK_END)) { fclose(input); return 1; }
    long length = ftell(input);
    if (length != 1777664) {
        fprintf(stderr, "Unexpected XBE size; this build targets the supplied USA Xbox disc.\n");
        fclose(input);
        return 1;
    }
    rewind(input);
    void *xbe = malloc((size_t)length);
    if (!xbe || fread(xbe, 1, (size_t)length, input) != (size_t)length) {
        fclose(input); free(xbe); return 1;
    }
    fclose(input);
    puts("Wrath of Cortex — native ARM64 static recompilation (development build)");
    if (!xbox_MemoryLayoutInit(xbe, (size_t)length)) { free(xbe); return 1; }
    g_xbox_mem_offset = xbox_GetMemoryOffset();
    xbox_kernel_init();
    xbox_path_init(resolved, save_path);
    xbox_kernel_bridge_init();
    g_esp = XBOX_STACK_TOP;
    if (!recomp_dispatch_init()) {
        fprintf(stderr, "Fast dispatch allocation failed; using compiled function lookup.\n");
    }
    xbox_WatchdogStart();
    printf("Calling recompiled Xbox entry point 0x000EF089, stack 0x%08X\n", g_esp);
    xbe_entry_point();
    puts("Game entry returned.");
    wrath_vblank_shutdown();
    xbox_kernel_shutdown();
    xbox_MemoryLayoutShutdown();
    free(xbe);
    return 0;
}
