"""RCL/RCR emitted results, carry and defined overflow against a bit-step oracle."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
UPSTREAM = ROOT/'third_party/xboxrecomp'
sys.path.insert(0, str(UPSTREAM))
from tools.recomp.disasm import Instruction, Operand
from tools.recomp.lifter import Lifter
from tools.recomp.translator import FunctionTranslator


class CarryRotates(unittest.TestCase):
    def test_emitted_widths_counts_and_one_bit_flags(self):
        functions = []
        for bits, destination in ((8, 'al'), (16, 'ax'), (32, 'eax')):
            for mnemonic in ('rcl', 'rcr'):
                ins = Instruction(0x10000, 2, mnemonic, '', '')
                ins.operands = [Operand('reg', reg=r) for r in (destination, 'cl')]
                lifter = Lifter();lifter.needs_cf = True;lifter.needs_rotate_snapshot = True
                functions.append(f'''static uint32_t {mnemonic}{bits}(uint32_t value,unsigned count,int *carry) {{
                    g_eax=value;g_ecx=count;int _cf=*carry;uint32_t _rotate_result=0;
                    {' '.join(lifter.lift_instruction(ins))}
                    *carry=_cf;return g_eax;
                }}''')
                opcode = {8: 'd0', 16: '66d1', 32: 'd1'}[bits] + ('d0' if mnemonic == 'rcl' else 'd8')
                for name, branch in (('overflow', '7001'), ('carry', '7201')):
                    # SHR ECX,1 establishes incoming carry; MOV after the carry
                    # rotate must not erase flags before their branch consumes them.
                    raw = bytes.fromhex('d1e9' + opcode + 'b800000000' + branch + 'c3b801000000c3')
                    info = {'end': 0x1000+len(raw), 'name': f'translated_{mnemonic}{bits}_{name}'}
                    translator = FunctionTranslator(raw, {0x1000: info})
                    translator._read_func_bytes = lambda a, b: raw[a-0x1000:b-0x1000]
                    function = translator.translate_function(0x1000, info)
                    self.assertIn('_cf', function)
                    self.assertNotIn('if (_flags', function)
                    self.assertNotIn('recomp_unsupported_instruction', function)
                    functions.append(function)
        source = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#define RECOMP_GENERATED_CODE
#include "recomp_types.h"
RECOMP_TLS uint32_t g_eax,g_ecx,g_esp,g_ebp,g_seh_ebp;
''' + '\n'.join(functions) + r'''
static uint32_t reference(uint32_t value,unsigned count,unsigned bits,int right,int *cf,int *of) {
    uint32_t mask=bits==32?UINT32_MAX:(1u<<bits)-1u,r=value&mask;
    unsigned masked=count&31u;
    for(unsigned i=0;i<masked%(bits+1u);i++) {
        int outgoing=(int)(right?r&1u:r>>(bits-1));
        r=right?(r>>1)|((unsigned)*cf<<(bits-1)):((r<<1)|(unsigned)*cf)&mask;
        *cf=outgoing;
    }
    if(masked==1)*of=(int)(((r>>(bits-1))^(right?r>>(bits-2):(unsigned)*cf))&1u);
    return (value&~mask)|r;
}
int main(void) {
    typedef uint32_t (*operation)(uint32_t,unsigned,int*);
    operation rotate[3][2]={{rcl8,rcr8},{rcl16,rcr16},{rcl32,rcr32}};
    void (*branches[3][2][2])(void)={
        {{translated_rcl8_carry,translated_rcl8_overflow},{translated_rcr8_carry,translated_rcr8_overflow}},
        {{translated_rcl16_carry,translated_rcl16_overflow},{translated_rcr16_carry,translated_rcr16_overflow}},
        {{translated_rcl32_carry,translated_rcl32_overflow},{translated_rcr32_carry,translated_rcr32_overflow}}
    };
    const uint32_t values[]={0,1,2,0x7f,0x80,0xff,0x7fff,0x8000,0xffff,0x7fffffff,0x80000000,0xffffffff,0x55aa55aa};
    for(unsigned width=0;width<3;width++) {
        unsigned bits=8u<<width,samples=width?sizeof(values)/sizeof(*values):256;
        for(unsigned i=0;i<samples;i++)for(unsigned count=0;count<256;count++)for(int right=0;right<2;right++)for(int initial=0;initial<2;initial++) {
            uint32_t value=width?values[i]:0xABCD0000u|i;
            int cf=initial,of=initial,expected_cf=initial,expected_of=initial;
            uint32_t expected=reference(value,count,bits,right,&expected_cf,&expected_of);
            assert(rotate[width][right](value,count,&cf)==expected && cf==expected_cf);
            cf=initial;
            uint32_t actual=RECOMP_CARRY_ROTATE(value,count,bits,right,&cf,&of);
            uint32_t mask=bits==32?UINT32_MAX:(1u<<bits)-1u;
            assert(actual==(expected&mask) && cf==expected_cf);
            if((count&31u)<=1)assert(of==expected_of);
            if((count&31u)==1)for(unsigned flag=0;flag<2;flag++) {
                g_eax=value;g_ecx=initial;g_esp=0;branches[width][right][flag]();
                assert(g_eax==(unsigned)(flag?expected_of:expected_cf) && g_esp==4);
            }
        }
    }
    puts("PASS emitted RCL/RCR8/16/32 all byte values/counts/carry inputs, masked counts and full translated one-bit carry/overflow branches");
}
'''
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary);file = directory/'rotate.c';file.write_text(source)
            binary = directory/'rotate'
            subprocess.run(['clang', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                            '-Wno-parentheses-equality', '-Wno-unused-variable', '-Wno-unused-label',
                            '-fsanitize=undefined', '-I'+str(UPSTREAM/'templates/runtime'),
                            str(file), '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True, env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})


if __name__ == '__main__':
    unittest.main()
