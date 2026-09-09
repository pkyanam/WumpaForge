"""Compile supplied-XBE CRT division routines into a CPU-only UBSan fixture."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
UPSTREAM = ROOT / 'third_party/xboxrecomp'
sys.path.insert(0, str(UPSTREAM))
from tools.recomp import config
from tools.recomp.translator import BatchTranslator


class CrtDivision(unittest.TestCase):
    def test_original_unsigned_signed_and_combined_remainders(self):
        xbe = ROOT/'local/assets/default.xbe'
        if not xbe.exists(): self.skipTest('requires the user-provided extracted XBE')
        config.configure_from_xbe(str(xbe))
        batch = BatchTranslator(xbe_path=str(xbe),
                                func_json_path=str(ROOT/'local/reports/disasm/functions.json'),
                                labels_json_path=str(ROOT/'local/reports/disasm/labels.json'),
                                identified_json_path=str(ROOT/'local/reports/func_id/identified_functions.json'),
                                abi_json_path=str(ROOT/'local/reports/abi/abi_functions.json'))
        functions = [batch.translator.translate_function(address, batch.func_db[address])
                     for address in (0xF61C0, 0xF6380, 0xF6430, 0xFB6C0)]
        for function in functions: self.assertNotIn('if (_flags', function)
        source = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#define RECOMP_GENERATED_CODE
#include "recomp_types.h"
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_ebx,g_esi,g_edi,g_esp,g_ebp,g_seh_ebp;
ptrdiff_t g_xbox_mem_offset;
static uint32_t memory[1024];
void recomp_unsupported_instruction(uint32_t address) { fprintf(stderr,"unsupported %08x\n",address);exit(42); }
''' + '\n'.join(functions) + r'''
static uint64_t run(void (*fn)(void),uint64_t dividend,uint64_t divisor,uint64_t *remainder,int combined) {
    g_esp=0x100;g_ebx=0xA5A5B0B0;g_esi=0xCCDDEE11;g_edi=0x11223344;
    MEM32(g_esp)=0xEEEEEEEE;MEM32(g_esp+4)=(uint32_t)dividend;MEM32(g_esp+8)=(uint32_t)(dividend>>32);
    MEM32(g_esp+12)=(uint32_t)divisor;MEM32(g_esp+16)=(uint32_t)(divisor>>32);
    fn();assert(g_esp==0x114 && g_esi==0xCCDDEE11 && g_edi==0x11223344);
    if(combined)*remainder=((uint64_t)g_ebx<<32)|g_ecx;
    else assert(g_ebx==0xA5A5B0B0);
    return ((uint64_t)g_edx<<32)|g_eax;
}
static void check(uint64_t dividend,uint64_t divisor) {
    if(!divisor)return; /* divide by zero and signed overflow are not valid oracle inputs */
    uint64_t remainder=0;
    assert(run(sub_000F6430,dividend,divisor,&remainder,0)==dividend/divisor);
    assert(run(sub_000F61C0,dividend,divisor,&remainder,0)==dividend%divisor);
    assert(run(sub_000FB6C0,dividend,divisor,&remainder,1)==dividend/divisor);
    assert(remainder==dividend%divisor);
    if(dividend!=UINT64_C(0x8000000000000000) || divisor!=UINT64_MAX)
        assert(run(sub_000F6380,dividend,divisor,&remainder,0)==(uint64_t)((int64_t)dividend/(int64_t)divisor));
}
int main(void) {
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    check(UINT64_C(0xfedcba9876543210),UINT64_C(0x123456789));
    const uint64_t values[]={0,1,2,3,0x7fffffff,0x80000000,0xffffffff,UINT64_C(0x100000000),
        UINT64_C(0x100000001),UINT64_C(0x7fffffffffffffff),UINT64_C(0x8000000000000000),
        UINT64_C(0x8000000000000001),UINT64_MAX,UINT64_C(0x123456789abcdef0),UINT64_C(0xfedcba9876543210)};
    for(unsigned a=0;a<sizeof(values)/sizeof(*values);a++)for(unsigned b=0;b<sizeof(values)/sizeof(*values);b++)check(values[a],values[b]);
    uint64_t random=UINT64_C(0x53e22af381965cb7);
    for(unsigned i=0;i<20000;i++) {
        random^=random<<13;random^=random>>7;random^=random<<17;uint64_t dividend=random;
        random^=random<<13;random^=random>>7;random^=random<<17;
        check(dividend,random);check(dividend,(uint32_t)random);
    }
    puts("PASS original CRT64 unsigned divide/remainder, signed divide and combined quotient/remainder, edge cases and40000 random divisor pairs");
}
'''
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary);file = directory/'division.c';file.write_text(source)
            binary = directory/'division'
            subprocess.run(['clang', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                            # CFG analysis can conservatively capture a flag no branch consumes.
                            '-Wno-parentheses-equality', '-Wno-unused-variable', '-Wno-unused-label',
                            '-Wno-unused-but-set-variable',
                            '-fsanitize=undefined', '-I'+str(UPSTREAM/'templates/runtime'),
                            str(file), '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True, env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})


if __name__ == '__main__':
    unittest.main()
