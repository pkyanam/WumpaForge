#!/usr/bin/env python3
"""Verify production Morton axis tables against the original indexing loop."""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]


def main():
    with tempfile.TemporaryDirectory() as directory:
        work = Path(directory)
        shutil.copytree(ROOT / 'src', work, dirs_exist_ok=True)
        for patch in sorted((ROOT / 'android/shield2019/patches').glob('title-*.patch')):
            subprocess.run(['git', 'apply', str(patch)], cwd=work, check=True)
        source = (work / 'graphics.c').read_text()
        original = re.search(r'static uint32_t morton_index\([^\n]+\)\n\{.*?\n\}', source, re.S).group(0)
        helper = re.search(r'static uint32_t \*morton_x_offsets\([^\n]+\)\n\{.*?\n\}', source, re.S).group(0)
        fixture = r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static int fail_allocation;
static void *guarded_malloc(size_t size) {
    if(fail_allocation)return NULL;
    uint8_t *p=malloc(size+32);assert(p);memset(p,0xAD,size+32);return p+16;
}
ORIGINAL
#define malloc guarded_malloc
HELPER
#undef malloc
static void release_offsets(uint32_t *p,unsigned width) {
    uint8_t *base=(uint8_t *)p-16;
    for(unsigned i=0;i<16;++i)assert(base[i]==0xAD && base[16+width*4+i]==0xAD);
    free(base);
}
static void check(unsigned width,unsigned height) {
    uint32_t *table=morton_x_offsets(width,height);assert(table);
    unsigned largest=morton_index(width-1,height-1,width,height);
    uint8_t *input=malloc(((size_t)largest+1)*4+32);assert(input);
    memset(input,0xCE,((size_t)largest+1)*4+32);
    for(unsigned i=0;i<=(largest*4+3);++i)input[16+i]=(uint8_t)(i*31+i/17);
    for(unsigned bpp=1;bpp<=4;bpp*=2) {
        size_t bytes=(size_t)width*height*bpp;
        uint8_t *expected=malloc(bytes+32),*actual=malloc(bytes+32);assert(expected&&actual);
        memset(expected,0xCA,bytes+32);memset(actual,0xCA,bytes+32);
        for(unsigned y=0;y<height;++y) {
            unsigned yo=morton_index(0,y,width,height);
            for(unsigned x=0;x<width;++x) {
                unsigned old=morton_index(x,y,width,height),fast=table[x]|yo;
                assert(old==fast && !(table[x]&yo));
                memcpy(expected+16+((size_t)y*width+x)*bpp,input+16+old*bpp,bpp);
                memcpy(actual+16+((size_t)y*width+x)*bpp,input+16+fast*bpp,bpp);
            }
        }
        assert(!memcmp(expected,actual,bytes+32));
        for(unsigned i=0;i<16;++i)assert(actual[i]==0xCA && actual[16+bytes+i]==0xCA);
        free(expected);free(actual);
    }
    for(unsigned i=0;i<16;++i)assert(input[i]==0xCE && input[16+((size_t)largest+1)*4+i]==0xCE);
    free(input);release_offsets(table,width);
}
int main(void) {
    for(unsigned w=1;w<=33;++w)for(unsigned h=1;h<=33;++h)check(w,h);
    for(unsigned w=1;w<=512;w*=2)for(unsigned h=1;h<=512;h*=2)check(w,h);
    uint32_t *large=morton_x_offsets(32768,1);assert(large);release_offsets(large,32768);
    assert(!morton_x_offsets(32769,1) && !morton_x_offsets(0,1));
    fail_allocation=1;assert(!morton_x_offsets(16,16));
    puts("PASS: Morton offsets/pixels for exhaustive1..33 rectangles and all power-of-two pairs through512, 1/2/4-byte pixels, guards, allocation cap/OOM");
}
'''.replace('ORIGINAL', original).replace('HELPER', helper)
        test = work / 'test.c'
        test.write_text(fixture)
        binary = work / 'test'
        subprocess.run(['cc', '-std=c11', '-O2', '-fsanitize=undefined', str(test), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    main()
