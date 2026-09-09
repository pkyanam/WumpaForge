"""Execute actual SHL/SHR emissions against a bit-step oracle under UBSan."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
UPSTREAM = ROOT / 'third_party/xboxrecomp'
sys.path.insert(0, str(UPSTREAM))
from tools.recomp.disasm import Instruction, Operand
from tools.recomp.lifter import Lifter, advance_flag_state
from tools.recomp.translator import FunctionTranslator


def instruction(mnemonic, destination, count='cl'):
    ins = Instruction(0x10000, 2, mnemonic, '', '')
    op = Operand('reg'); op.reg = destination
    amount = Operand('imm' if isinstance(count, int) else 'reg')
    if isinstance(count, int): amount.imm = count
    else: amount.reg = count
    ins.operands = [op, amount]
    return ins


class LogicalShifts(unittest.TestCase):
    def test_emitted_results_and_defined_flags(self):
        functions = []
        for bits, destination in ((8, 'al'), (16, 'ax'), (32, 'eax')):
            for mnemonic in ('shl', 'shr'):
                lifter = Lifter(); lifter.needs_cf = True; lifter.needs_shift_snapshot = True
                ins = instruction(mnemonic, destination)
                body = '\n'.join(lifter.lift_instruction(ins))
                functions.append(f'''static uint32_t {mnemonic}{bits}(uint32_t value,unsigned count,int *cf,int *of) {{
                    g_eax=value;g_ecx=count;int _cf=*cf,_shift_of=*of;
                    uint32_t _shift_result=0;(void)_shift_result;
                    {body}
                    *cf=_cf;*of=_shift_of;return g_eax;
                }}''')
        for mnemonic, opcode in (('shl', 'd0e0'), ('shr', 'd0e8')):
            for condition, opcode_condition in (('overflow', '7001'), ('carry', '7201'),
                                                ('zero', '7401'), ('negative', '7801'), ('parity', '7a01')):
                # A masked-zero shift of another register and MOV must preserve
                # the first shift's flags, even after its operand is overwritten.
                raw = bytes.fromhex(opcode + 'c0eb20b800000000' + opcode_condition + 'c3b801000000c3')
                info = {'end': 0x1000+len(raw), 'name': f'translated_{mnemonic}_{condition}'}
                translator = FunctionTranslator(raw, {0x1000: info})
                translator._read_func_bytes = lambda a, b: raw[a-0x1000:b-0x1000]
                function = translator.translate_function(0x1000, info)
                self.assertIn('_shift_result = RECOMP_SHIFT(', function)
                self.assertNotIn('(_flags', function)
                self.assertNotIn('recomp_unsupported_instruction', function)
                functions.append(function)
        for mnemonic in ('shl', 'shr'):
            self.assertEqual(advance_flag_state(instruction(mnemonic, 'al', 32), ('test', [])), ('test', []))
        source = r''' 
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#define RECOMP_GENERATED_CODE
#include "recomp_types.h"
RECOMP_TLS uint32_t g_eax,g_ecx,g_ebx,g_esp,g_ebp,g_seh_ebp;
''' + '\n'.join(functions) + r'''
static uint32_t reference(uint32_t value,unsigned count,unsigned bits,int right,int *cf,int *of) {
    uint32_t mask=bits==32?UINT32_MAX:(1u<<bits)-1u,r=value&mask;
    unsigned masked=count&31u,sign=r>>(bits-1);
    int carry=*cf;
    for(unsigned i=0;i<masked;i++) {
        carry=(int)(right?r&1u:r>>(bits-1));
        r=right?r>>1:(r<<1)&mask;
    }
    /* SHL/SHR CF is undefined for masked counts >= operand width. */
    if(masked<bits)*cf=carry;
    if(masked==1)*of=right?(int)sign:(int)((r>>(bits-1))^(unsigned)carry);
    return (value&~mask)|r;
}
int main(void) {
    typedef uint32_t (*operation)(uint32_t,unsigned,int*,int*);
    operation shifts[3][2]={{shl8,shr8},{shl16,shr16},{shl32,shr32}};
    volatile unsigned warning_count=0x89;int warning_cf=0,warning_of=0;
    assert(shr8(0x80,warning_count,&warning_cf,&warning_of)==0);
    const uint32_t values[]={0,1,2,0x7f,0x80,0xff,0x7fff,0x8000,0xffff,0x7fffffff,0x80000000,0xffffffff,0x55aa55aa};
    for(unsigned width=0;width<3;width++) {
        unsigned bits=8u<<width,samples=width?sizeof(values)/sizeof(*values):256;
        for(unsigned i=0;i<samples;i++)for(unsigned count=0;count<256;count++)for(int right=0;right<2;right++)for(int initial=0;initial<2;initial++) {
            uint32_t value=width?values[i]:0xABCD0000u|i;
            int cf=initial,of=initial,expected_cf=initial,expected_of=initial;
            uint32_t expected=reference(value,count,bits,right,&expected_cf,&expected_of);
            uint32_t actual=shifts[width][right](value,count,&cf,&of);
            assert(actual==expected);
            if((count&31u)<bits)assert(cf==expected_cf);
            if((count&31u)<=1)assert(of==expected_of);
        }
    }
    void (*conditions[2][5])(void)={
        {translated_shl_overflow,translated_shl_carry,translated_shl_zero,translated_shl_negative,translated_shl_parity},
        {translated_shr_overflow,translated_shr_carry,translated_shr_zero,translated_shr_negative,translated_shr_parity}
    };
    for(unsigned value=0;value<256;value++)for(int right=0;right<2;right++) {
        int cf=0,of=0;unsigned result=reference(value,1,8,right,&cf,&of);
        unsigned parity=1;for(unsigned i=0;i<8;i++)parity^=(result>>i)&1u;
        unsigned expected[]={of,cf,result==0,result>>7,parity};
        for(unsigned condition=0;condition<5;condition++) {
            g_eax=value;g_ebx=0x55;g_esp=0;conditions[right][condition]();
            assert(g_eax==expected[condition] && g_ebx==0x55 && g_esp==4);
        }
    }
    puts("PASS emitted SHL/SHR8/16/32 all byte values and256 counts, masked-zero flags, defined carry/one-bit overflow");
}
'''
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary); file = directory/'shifts.c'; file.write_text(source)
            binary = directory/'shifts'
            subprocess.run(['clang', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                            '-Wno-parentheses-equality', '-Wno-unused-variable', '-Wno-unused-label', '-fsanitize=undefined', '-I'+str(UPSTREAM/'templates/runtime'),
                            str(file), '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True, env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})


if __name__ == '__main__':
    unittest.main()
