#!/usr/bin/env python3
"""Use actual profiler with deterministic clocks from alternating worker threads."""
from pathlib import Path
import shutil
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[3]
TEST = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
static _Thread_local uint64_t wall, cpu;
static int test_clock(clockid_t kind, struct timespec *out) {
    uint64_t value = kind == CLOCK_THREAD_CPUTIME_ID ? cpu : wall;
    out->tv_sec = value / 1000000000; out->tv_nsec = value % 1000000000;
    return 0;
}
#define clock_gettime test_clock
static int window_thread(void) { return 0; }
static uint32_t read32(uint32_t address) { (void)address; return 0; }
static void *guest_ptr(uint32_t address) { (void)address; return NULL; }
#define GUEST_SWAP_COUNT 0
#include "graphics_timing_probe.inc"
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed = PTHREAD_COND_INITIALIZER;
static int stage, failures;
static void advance(int value) {
    pthread_mutex_lock(&mutex); stage = value;
    pthread_cond_broadcast(&changed); pthread_mutex_unlock(&mutex);
}
static void await_stage(int value) {
    pthread_mutex_lock(&mutex);
    while(stage < value) pthread_cond_wait(&changed, &mutex);
    pthread_mutex_unlock(&mutex);
}
static void *first(void *unused) {
    (void)unused; wall = 1000; cpu = 500;
    wrath_profile_present_end(900);
    advance(1); await_stage(2);
    wall = 2000; cpu = 600; wrath_profile_present_begin();
    if(wrath_profile_current()->work_cpu != 100 ||
       wrath_profile_current()->work != 1000) ++failures;
    return NULL;
}
static void *second(void *unused) {
    (void)unused; await_stage(1); wall = 1500; cpu = 10;
    wrath_profile_present_begin();
    if(wrath_profile_current()->work_cpu != 0 ||
       wrath_profile_current()->last_end != 0) ++failures;
    wall = 1600; cpu = 20; wrath_profile_present_end(1500);
    wall = 1700; cpu = 30; wrath_profile_present_begin();
    if(wrath_profile_current()->work_cpu != 10 ||
       wrath_profile_current()->work != 100) ++failures;
    advance(2); return NULL;
}
int main(void) {
    setenv("WRATH_PROFILE", "1", 1);
    wrath_profile_enabled(); /* initialize once before the test workers */
    pthread_t a, b;
    assert(!pthread_create(&a, NULL, first, NULL));
    assert(!pthread_create(&b, NULL, second, NULL));
    assert(!pthread_join(a, NULL)); assert(!pthread_join(b, NULL));
    if(failures) { fprintf(stderr, "%d mixed-worker clock failures\n", failures); return 1; }
    puts("Independent worker CPU and wall-clock intervals passed"); return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='wumpa-profile-clock-') as temporary:
    work = Path(temporary)
    source = work / 'graphics_timing_probe.inc'
    shutil.copyfile(ROOT / 'src/graphics_timing_probe.inc', source)
    unit = work / 'check.c'; unit.write_text(TEST)
    for patched in (False, True):
        if patched:
            subprocess.run(['git', 'apply', str(ROOT / 'android/shield2019/patches/title-profile-thread-clock.patch')], cwd=work, check=True)
        binary = work / ('tls' if patched else 'baseline')
        subprocess.run(['clang', '-std=c11', '-D_DARWIN_C_SOURCE', '-O1', '-pthread', '-fsanitize=undefined', '-fno-sanitize-recover=all', str(unit), '-o', str(binary)], check=True)
        result = subprocess.run([str(binary)], timeout=10)
        assert result.returncode == (0 if patched else 1)
