"""CPU emitted-instruction checks for the original FXAM classifier's byte ops."""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
UPSTREAM = ROOT/'third_party/xboxrecomp'
sys.path.insert(0, str(UPSTREAM))
from tools.recomp.disasm import Instruction, Operand
from tools.recomp.lifter import Lifter, _make_condition, advance_flag_state
from tools.recomp.translator import FunctionTranslator


def instruction(mnemonic, destination, count='cl'):
    ins = Instruction(0xF7AB2, 2, mnemonic, '', '')
    op = Operand('reg');op.reg = destination
    amount = Operand('imm' if isinstance(count, int) else 'reg')
    if isinstance(count, int): amount.imm = count
    else: amount.reg = count
    ins.operands = [op, amount]
    return ins


class ShiftRotateWidths(unittest.TestCase):
    def test_results_carry_defined_overflow_and_classifier(self):
        functions = []
        for bits, destination in ((8, 'al'), (16, 'ax'), (32, 'eax')):
            for mnemonic in ('rol', 'ror', 'sar'):
                lifter = Lifter();lifter.needs_cf = True;lifter.needs_rotate_snapshot = True
                ins = instruction(mnemonic, destination)
                body = '\n'.join(lifter.lift_instruction(ins))
                functions.append(f'''static uint32_t {mnemonic}{bits}(uint32_t value,unsigned count,int *carry) {{
                    g_eax=value;g_ecx=count;int _cf=*carry;
                    uint32_t _rotate_result=0;(void)_rotate_result;
                    {body}
                    *carry=_cf;return g_eax;
                }}''')
                if mnemonic == 'sar':
                    one = instruction(mnemonic, destination, 1)
                    condition = _make_condition('js', mnemonic, one.operands)[0]
                    functions.append(f'''static int sar{bits}_negative(uint32_t value) {{
                        g_eax=value;int _cf=0;
                        {' '.join(lifter.lift_instruction(one))}
                        return {condition};
                    }}''')
                if mnemonic != 'sar':
                    one = instruction(mnemonic, destination, 1)
                    condition = _make_condition('jo', mnemonic, one.operands)[0]
                    functions.append(f'''static int {mnemonic}{bits}_overflow(uint32_t value) {{
                        g_eax=value;int _cf=0;uint32_t _rotate_result=0;
                        {' '.join(lifter.lift_instruction(one))}
                        return {condition};
                    }}''')
                    zero = instruction(mnemonic, destination, 32)
                    state = ('test', one.operands)
                    self.assertEqual(advance_flag_state(zero, state), state)
                    self.assertEqual(_make_condition('jb', mnemonic, ins.operands)[0], '_cf')
        for destination in ('cl', 'ch'):
            body = '\n'.join(line for mnemonic in ('shl', 'sar', 'rol')
                             for line in Lifter().lift_instruction(instruction(mnemonic, destination, 1)))
            functions.append(f'''static uint32_t classify_{destination}(uint32_t value) {{
                g_ecx=value;{body}
                return g_ecx;
            }}''')
        for mnemonic, opcode in (('rol', 'd0c0'), ('ror', 'd0c8')):
            for condition, opcode_condition in (('overflow', '7001'), ('carry', '7201')):
                raw = bytes.fromhex(opcode + 'b800000000' + opcode_condition + 'c3b801000000c3')
                info = {'end': 0x1000+len(raw), 'name': f'translated_{mnemonic}_{condition}'}
                translator = FunctionTranslator(raw, {0x1000: info})
                translator._read_func_bytes = lambda a, b: raw[a-0x1000:b-0x1000]
                function = translator.translate_function(0x1000, info)
                self.assertIn('_rotate_result = RECOMP_ROTATE(', function)
                self.assertNotIn('(_flags', function)
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
    uint32_t mask=bits==32?UINT32_MAX:(1u<<bits)-1u;
    uint32_t r=value&mask;
    unsigned masked=count&31u;
    for(unsigned i=0;i<masked%bits;i++) {
        unsigned outgoing=right?r&1u:r>>(bits-1);
        r=right?(r>>1)|(outgoing<<(bits-1)):((r<<1)|outgoing)&mask;
    }
    if(masked)*cf=right?(int)(r>>(bits-1)):(int)(r&1u);
    if(masked==1)*of=(int)(((r>>(bits-1))^(right?r>>(bits-2):r))&1u);
    return (value&~mask)|r;
}
static uint32_t reference_sar(uint32_t value,unsigned count,unsigned bits,int *cf) {
    uint32_t mask=bits==32?UINT32_MAX:(1u<<bits)-1u,r=value&mask;
    for(unsigned i=0;i<(count&31u);i++) {
        *cf=r&1u;r=(r>>1)|(r&(1u<<(bits-1)));
    }
    return (value&~mask)|r;
}
int main(void) {
    typedef uint32_t (*operation)(uint32_t,unsigned,int*);
    operation rotate[3][2]={{rol8,ror8},{rol16,ror16},{rol32,ror32}};
    operation shifts[]={sar8,sar16,sar32};
    int (*overflow[3][2])(uint32_t)={{rol8_overflow,ror8_overflow},{rol16_overflow,ror16_overflow},{rol32_overflow,ror32_overflow}};
    int (*negative[])(uint32_t)={sar8_negative,sar16_negative,sar32_negative};
    const uint32_t values[]={0,1,2,0x7f,0x80,0xff,0x7fff,0x8000,0xffff,0x7fffffff,0x80000000,0xffffffff,0x55aa55aa};
    for(unsigned width=0;width<3;width++) {
        unsigned bits=8u<<width;
        unsigned samples=width?sizeof(values)/sizeof(*values):256;
        for(unsigned i=0;i<samples;i++) {
            uint32_t value=width?values[i]:0xABCD0000u|i;
            assert(negative[width](value)==(int)((value>>(bits-1))&1u));
            for(unsigned count=0;count<64;count++)for(int initial=0;initial<2;initial++) {
                int expected_cf=initial,actual_cf=initial;
                uint32_t expected=reference_sar(value,count,bits,&expected_cf);
                assert(shifts[width](value,count,&actual_cf)==expected && actual_cf==expected_cf);
                for(int right=0;right<2;right++) {
                    int cf=initial,of=initial,expected_of=initial;
                    expected_cf=initial;actual_cf=initial;
                    expected=reference(value,count,bits,right,&expected_cf,&expected_of);
                    assert(rotate[width][right](value,count,&actual_cf)==expected && actual_cf==expected_cf);
                    uint32_t actual=RECOMP_ROTATE(value,count,bits,right,&cf,&of);
                    uint32_t mask=bits==32?UINT32_MAX:(1u<<bits)-1u;
                    assert(actual==(expected&mask) && cf==expected_cf);
                    if((count&31u)<=1)assert(of==expected_of);
                    if((count&31u)==1)assert(overflow[width][right](value)==expected_of);
                }
            }
        }
    }
    /* Original F7AAE/F7B27/F7B34 sequences: sign-extend shifted C3 then rotate
     * into classifier index bit0. Upper/lower register halves remain intact. */
    for(unsigned ah=0;ah<256;ah++) {
        unsigned expected_index=((ah&7u)<<1)|((ah>>6)&1u);
        uint32_t low=0xA5A50000u|ah,high=0xA5A5005Au|(ah<<8);
        uint32_t result=classify_cl(low);
        assert((result&0xfu)==expected_index && (result&0xffffff00u)==(low&0xffffff00u));
        result=classify_ch(high);
        assert(((result>>8)&0xfu)==expected_index && (result&0xffff00ffu)==(high&0xffff00ffu));
    }
    for(unsigned value=0;value<256;value++) {
        int cf=0,of=0;
        reference(value,1,8,0,&cf,&of);
        g_eax=value;g_esp=0;translated_rol_overflow();assert(g_eax==(unsigned)of && g_esp==4);
        g_eax=value;g_esp=0;translated_rol_carry();assert(g_eax==(unsigned)cf && g_esp==4);
        reference(value,1,8,1,&cf,&of);
        g_eax=value;g_esp=0;translated_ror_overflow();assert(g_eax==(unsigned)of && g_esp==4);
        g_eax=value;g_esp=0;translated_ror_carry();assert(g_eax==(unsigned)cf && g_esp==4);
    }
    puts("PASS emitted SAR/ROL/ROR8/16/32, all byte values/counts, carry/defined overflow, zero flags and original FXAM classifier sequences");
}
'''
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary);file = directory/'rotates.c';file.write_text(source)
            binary = directory/'rotates'
            subprocess.run(['clang', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                            '-Wno-unused-variable', '-Wno-unused-label', '-fsanitize=undefined', '-I'+str(UPSTREAM/'templates/runtime'),
                            str(file), '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    unittest.main()
