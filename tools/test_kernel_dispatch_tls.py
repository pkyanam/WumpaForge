#!/usr/bin/env python3
"""Interleave actual kernel lookups: shared selector fails, common Mac/Android TLS passes."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
PRELUDE = r'''
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint32_t ULONG;
typedef uint32_t DWORD;
typedef void (*bridge_func_t)(void);
typedef void (*recomp_func_t)(void);
#define RECOMP_TLS _Thread_local
#define XBOX_KERNEL_THUNK_TABLE_SIZE 400
#define KERNEL_VA_BASE 0x80000000u
#define KERNEL_VA_END (KERNEL_VA_BASE + XBOX_KERNEL_THUNK_TABLE_SIZE * 4)
#define KERNEL_LOG_ON() 0
#define BRIDGE_MEM32(address) (0x12345678u + (uint32_t)(address))
static RECOMP_TLS uint32_t g_eax, g_esp, g_xbox_kernel_caller;
static ULONG g_slot_ordinals[400];
static bridge_func_t g_slot_bridges[400];
static unsigned g_slot_arg_bytes[400], g_slot_arg_unknown[400];
static unsigned long long g_ordinal_calls[400];
static int g_kernel_call_count;
static uint32_t g_kernel_watch_va;
static void kernel_watch_arm_once(void) {}
static DWORD GetTickCount(void) { return 1; }
'''
TEST = r'''
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed = PTHREAD_COND_INITIALIZER;
static int stage;
static unsigned actual_ordinal[2], actual_stack[2];
static void enter_bridge(void) { g_eax = 277; }
static void delay_bridge(void) { g_eax = 99; }
static void advance(int value) {
    pthread_mutex_lock(&mutex); stage = value;
    pthread_cond_broadcast(&changed); pthread_mutex_unlock(&mutex);
}
static void await_stage(int value) {
    pthread_mutex_lock(&mutex);
    while (stage < value) pthread_cond_wait(&changed, &mutex);
    pthread_mutex_unlock(&mutex);
}
static void *thread_enter(void *unused) {
    (void)unused; g_esp = 0x1000;
    recomp_func_t call = recomp_lookup_kernel(KERNEL_VA_BASE + 277 * 4);
    advance(1); await_stage(2); call();
    actual_ordinal[0] = g_eax; actual_stack[0] = g_esp;
    advance(3); return NULL;
}
static void *thread_delay(void *unused) {
    (void)unused; await_stage(1); g_esp = 0x2000;
    recomp_func_t call = recomp_lookup_kernel(KERNEL_VA_BASE + 99 * 4);
    advance(2); await_stage(3); call();
    actual_ordinal[1] = g_eax; actual_stack[1] = g_esp;
    return NULL;
}
static void nested_bridge(void) {
    /* Outer slot was selected before this nested lookup replaced the TLS slot. */
    recomp_func_t nested = recomp_lookup_kernel(KERNEL_VA_BASE + 99 * 4);
    uint32_t outer_stack = g_esp;
    g_esp = 0x3000; nested(); assert(g_esp == 0x3010);
    g_esp = outer_stack; g_eax = 277;
}
int main(void) {
    g_slot_ordinals[277] = 277; g_slot_bridges[277] = enter_bridge;
    g_slot_arg_bytes[277] = 4;
    g_slot_ordinals[99] = 99; g_slot_bridges[99] = delay_bridge;
    g_slot_arg_bytes[99] = 12;
    pthread_t enter, delay;
    assert(!pthread_create(&enter, NULL, thread_enter, NULL));
    assert(!pthread_create(&delay, NULL, thread_delay, NULL));
    assert(!pthread_join(enter, NULL)); assert(!pthread_join(delay, NULL));
    if (actual_ordinal[0] != 277 || actual_stack[0] != 0x1008 ||
        actual_ordinal[1] != 99 || actual_stack[1] != 0x2010) {
        fprintf(stderr, "wrong bridge/stack: %u/%x and %u/%x\n",
                actual_ordinal[0], actual_stack[0], actual_ordinal[1], actual_stack[1]);
        return 1;
    }
    g_slot_bridges[277] = nested_bridge;
    g_esp = 0x4000; recomp_lookup_kernel(KERNEL_VA_BASE + 277 * 4)();
    assert(g_eax == 277 && g_esp == 0x4008);
    assert(recomp_lookup_kernel(KERNEL_VA_BASE - 1) == NULL);
    assert(recomp_lookup_kernel(KERNEL_VA_END) == NULL);
    puts("TLS interleaved bridge/stack and nested cleanup checks passed");
    return 0;
}
'''

def fragment(source):
    start = source.index('static ', source.index('/* Current dispatching slot */')) if '/* Current dispatching slot */' in source else source.index('static RECOMP_TLS int g_kernel_dispatch_slot')
    end = source.index('/* ── Initialization', start)
    return source[start:end]

def main():
    upstream = ROOT / 'third_party/xboxrecomp/src/kernel/kernel_bridge.c'
    original = upstream.read_text()
    shared = 'static int g_kernel_dispatch_slot = -1;'
    tls = 'static RECOMP_TLS int g_kernel_dispatch_slot = -1;'
    if original.count(tls) == 1:
        # Exercise the same patch even when bootstrap already applied it.
        comment_start = original.index('/* Lookup and immediate invocation')
        comment_end = original.index(tls, comment_start) + len(tls)
        original = original[:comment_start] + '/* Current dispatching slot */\n' + shared + original[comment_end:]
    assert original.count(shared) == 1 and tls not in original
    # Replay inside ignored build/, where parent Git discovery once silently
    # skipped this fix. Fence discovery and verify forward AND reverse replay.
    build = ROOT / 'build'
    build.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='wumpa-kernel-tls-', dir=build) as temporary:
        work = Path(temporary)
        target = work / 'src/kernel/kernel_bridge.c'
        target.parent.mkdir(parents=True)
        target.write_text(original)
        env = os.environ.copy()
        for key in ('GIT_DIR', 'GIT_WORK_TREE', 'GIT_INDEX_FILE'):
            env.pop(key, None)
        env['GIT_CEILING_DIRECTORIES'] = str(build)
        patch = ROOT / 'patches/xboxrecomp-zz-kernel-dispatch-tls.patch'
        for flags in (['--check'], [], ['--reverse', '--check']):
            subprocess.run(['git', 'apply', *flags, str(patch)], cwd=work, env=env, check=True)
        assert target.read_text().count(tls) == 1
        for name, source, expected in [('baseline', original, 1), ('tls', target.read_text(), 0)]:
            unit = work / f'{name}.c'
            unit.write_text(PRELUDE + fragment(source) + TEST)
            binary = work / name
            subprocess.run(['clang', '-std=c11', '-O1', '-g', '-pthread', '-fsanitize=undefined', '-fno-sanitize-recover=all', str(unit), '-o', str(binary)], check=True)
            result = subprocess.run([str(binary)], timeout=10)
            assert result.returncode == expected, (name, result.returncode)
        print('Baseline deterministically reproduces race; patched actual dispatcher passes.')


if __name__ == '__main__':
    main()
