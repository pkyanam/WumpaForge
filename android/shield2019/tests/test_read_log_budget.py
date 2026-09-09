#!/usr/bin/env python3
"""Run the actual POSIX read and guest bridge with real file I/O and log gating."""
from pathlib import Path
import subprocess, tempfile
ROOT = Path(__file__).resolve().parents[3]

def function(text, signature):
    start = text.index(signature)
    brace = text.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]

def main():
    runtime = ROOT / 'third_party/xboxrecomp/src/kernel'
    with tempfile.TemporaryDirectory(prefix='wumpa-read-log-') as directory:
        work = Path(directory)
        source = work / 'src/kernel/kernel_bridge.c'
        source.parent.mkdir(parents=True)
        source.write_text((runtime/'kernel_bridge.c').read_text())
        subprocess.run(['git', 'apply', str(ROOT/'android/shield2019/patches/runtime-read-log-budget.patch')], cwd=work, check=True)
        read = function((runtime/'kernel_file.c').read_text().split('#else /* !_WIN32 */')[1], 'NTSTATUS __stdcall xbox_NtReadFile(')
        bridge = function(source.read_text(), 'static void bridge_NtReadFile(void)')
        fixture = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#define __stdcall
#define WRATH_ANDROID_TV 1
#define XBOX_TRACE(...) ((void)0)
#define STATUS_SUCCESS 0u
#define STATUS_INVALID_PARAMETER 0xc000000du
#define STATUS_INVALID_HANDLE 0xc0000008u
#define STATUS_UNSUCCESSFUL 0xc0000001u
#define STATUS_END_OF_FILE 0xc0000011u
typedef uint32_t NTSTATUS, ULONG;
typedef uintptr_t ULONG_PTR;
typedef void *HANDLE, *PIO_APC_ROUTINE, *PVOID;
typedef struct {uint32_t Status; uintptr_t Information;} XBOX_IO_STATUS_BLOCK, *PXBOX_IO_STATUS_BLOCK;
typedef union {struct {uint32_t LowPart; int32_t HighPart;}; int64_t QuadPart;} LARGE_INTEGER, *PLARGE_INTEGER;
typedef int32_t LONG;
static _Alignas(8) unsigned char memory[512];
static uint32_t args[8], g_eax;
static int fd, budget, completions;
#define STACK_ARG(i) args[i]
#define BRIDGE_MEM32(i) (*(uint32_t*)(memory+(i)))
#define XBOX_TO_NATIVE(i) (memory+(i))
#define KERNEL_LOG_ON() budget
static HANDLE bridge_resolve_handle(uint32_t h) {(void)h;return (HANDLE)(uintptr_t)1;}
static int w32_handle_fd(HANDLE h) {(void)h;return fd;}
static void SetEvent(HANDLE e) {(void)e;}
static void bridge_write_iostatus(uint32_t a,uint32_t s,uint32_t n) {BRIDGE_MEM32(a)=s;BRIDGE_MEM32(a+4)=n;}
static void bridge_complete_file_io(uint32_t a,uint32_t b,uint32_t c,uint32_t d) {(void)a;(void)b;(void)c;(void)d;++completions;}
'''
        fixture += '\n'+read+'\n'+bridge+r'''
int main(void) {
    FILE *input=tmpfile();assert(input);fd=fileno(input);
    assert(write(fd,"abcdefghijkl",12)==12);
    FILE *logs=freopen("read.log","w+",stderr);assert(logs);
    args[4]=16;args[5]=64;args[7]=32;
    budget=0;args[6]=4;BRIDGE_MEM32(32)=0;
    bridge_NtReadFile();assert(g_eax==0 && BRIDGE_MEM32(16)==0 && BRIDGE_MEM32(20)==4);
    assert(!memcmp(memory+64,"abcd",4));assert(ftell(logs)==0);
    args[6]=20;BRIDGE_MEM32(32)=0;
    bridge_NtReadFile();assert(g_eax==0 && BRIDGE_MEM32(20)==12);
    assert(!memcmp(memory+64,"abcdefghijkl",12));long short_end=ftell(logs);assert(short_end>0);
    BRIDGE_MEM32(32)=12;bridge_NtReadFile();
    assert(g_eax==STATUS_END_OF_FILE && BRIDGE_MEM32(16)==STATUS_END_OF_FILE && BRIDGE_MEM32(20)==0);
    long eof_end=ftell(logs);assert(eof_end>short_end);
    int saved=fd;fd=-1;bridge_NtReadFile();fd=saved;
    assert(g_eax==STATUS_INVALID_HANDLE && BRIDGE_MEM32(20)==0);long invalid_end=ftell(logs);assert(invalid_end>eof_end);
    budget=1;args[6]=4;BRIDGE_MEM32(32)=0;bridge_NtReadFile();
    assert(g_eax==0 && BRIDGE_MEM32(20)==4 && ftell(logs)>invalid_end && completions==5);
    fclose(input);fclose(logs);
    puts("PASS: actual POSIX reads, guest status/bytes, completion, successful trace budget, short/EOF/error diagnostics");
}
'''
        c = work/'fixture.c';c.write_text(fixture)
        binary = work/'test'
        subprocess.run(['clang', '-std=c11', '-D_DARWIN_C_SOURCE', '-D_GNU_SOURCE', '-O1', '-fsanitize=undefined', '-fno-sanitize-recover=all', str(c), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], cwd=work, check=True)

if __name__ == '__main__':
    main()
