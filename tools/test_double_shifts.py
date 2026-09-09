"""CPU-only SHLD/SHRD semantics and original CRT64 helpers, never game/UI execution."""
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
from tools.recomp.disasm import Instruction, Operand
from tools.recomp.lifter import Lifter


class DoubleShifts(unittest.TestCase):
    def test_original_crt_helpers_and_emitted_widths(self):
        xbe = ROOT / 'local/assets/default.xbe'
        if not xbe.exists(): self.skipTest('requires the user-provided extracted XBE')
        config.configure_from_xbe(str(xbe))
        batch = BatchTranslator(xbe_path=str(xbe),
                                func_json_path=str(ROOT/'local/reports/disasm/functions.json'),
                                labels_json_path=str(ROOT/'local/reports/disasm/labels.json'),
                                identified_json_path=str(ROOT/'local/reports/func_id/identified_functions.json'),
                                abi_json_path=str(ROOT/'local/reports/abi/abi_functions.json'))
        functions = [batch.translator.translate_function(address, batch.func_db[address])
                     for address in (0xF4600, 0xF61A0, 0xF64A0)]
        for function in functions:
            self.assertNotIn('if (_flags', function)
            self.assertNotIn('recomp_unsupported_instruction', function)
        for bits, dest, src in ((16, 'ax', 'dx'), (32, 'eax', 'edx')):
            for mnemonic in ('shld', 'shrd'):
                ins = Instruction(0x10000, 3, mnemonic, '', '')
                ins.operands = [Operand('reg', reg=r) for r in (dest, src, 'cl')]
                lifter = Lifter(); lifter.needs_cf = True; lifter.needs_shift_snapshot = True
                functions.append(f'''static uint32_t {mnemonic}{bits}(uint32_t value,uint32_t source,unsigned count,int *cf,int *of) {{
                    g_eax=value;g_edx=source;g_ecx=count;int _cf=*cf,_shift_of=*of;
                    uint32_t _shift_result=0;(void)_shift_result;
                    {' '.join(lifter.lift_instruction(ins))}
                    *cf=_cf;*of=_shift_of;return g_eax;
                }}''')
        source = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#define RECOMP_GENERATED_CODE
#include "recomp_types.h"
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebp,g_seh_ebp;
''' + '\n'.join(functions) + r'''
static uint64_t run(void (*fn)(void),uint64_t value,unsigned count) {
    g_eax=(uint32_t)value;g_edx=(uint32_t)(value>>32);g_ecx=0xAABBCC00u|count;g_esp=0;
    fn();assert(g_esp==4);return ((uint64_t)g_edx<<32)|g_eax;
}
static uint64_t arithmetic(uint64_t value,unsigned count) {
    for(unsigned i=0;i<count;i++)value=(value>>1)|(value&UINT64_C(0x8000000000000000));
    return value;
}
static uint32_t reference(uint32_t value,uint32_t source,unsigned count,unsigned bits,int right,int *cf,int *of) {
    uint32_t mask=bits==32?UINT32_MAX:(1u<<bits)-1u,r=value&mask,s=source&mask;
    unsigned masked=count&31u,original_sign=r>>(bits-1);
    /* The16-bit result/flags are architecturally undefined for counts17..31. */
    assert(masked<=bits);
    for(unsigned i=0;i<masked;i++) {
        *cf=(int)(right?r&1u:r>>(bits-1));
        if(right) { r=(r>>1)|((s&1u)<<(bits-1));s>>=1; }
        else { r=((r<<1)|(s>>(bits-1)))&mask;s=(s<<1)&mask; }
    }
    if(masked==1)*of=right?(int)(original_sign^(r>>(bits-1))):(int)((r>>(bits-1))^(unsigned)*cf);
    return (value&~mask)|r;
}
int main(void) {
    const uint64_t values[]={0,1,2,UINT64_MAX,UINT64_C(0x8000000000000000),UINT64_C(0x7fffffffffffffff),
        UINT64_C(0x123456789abcdef0),UINT64_C(0xfedcba9876543210),UINT64_C(0x0000000080000000)};
    for(unsigned i=0;i<sizeof(values)/sizeof(*values);i++)for(unsigned count=0;count<256;count++) {
        uint64_t value=values[i];
        assert(run(sub_000F4600,value,count)==(count>=64?0:value<<count));
        assert(run(sub_000F61A0,value,count)==(count>=64?0:value>>count));
        assert(run(sub_000F64A0,value,count)==arithmetic(value,count));
    }
    typedef uint32_t (*operation)(uint32_t,uint32_t,unsigned,int*,int*);
    operation shifts[2][2]={{shld16,shrd16},{shld32,shrd32}};
    for(unsigned width=0;width<2;width++)for(unsigned i=0;i<sizeof(values)/sizeof(*values);i++)
    for(unsigned j=0;j<sizeof(values)/sizeof(*values);j++)for(unsigned count=0;count<256;count++)for(int right=0;right<2;right++)for(int initial=0;initial<2;initial++) {
        unsigned bits=16u<<width;if((count&31u)>bits)continue;
        int cf=initial,of=initial,expected_cf=initial,expected_of=initial;
        uint32_t value=(uint32_t)values[i],source=(uint32_t)values[j];
        uint32_t expected=reference(value,source,count,bits,right,&expected_cf,&expected_of);
        assert(shifts[width][right](value,source,count,&cf,&of)==expected && cf==expected_cf);
        if((count&31u)<=1)assert(of==expected_of);
    }
    puts("PASS original CRT64 left/logical-right/arithmetic-right counts0..255 and emitted SHLD/SHRD16/32 defined results/flags");
}
'''
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary); file = directory/'shifts.c'; file.write_text(source)
            binary = directory/'shifts'
            subprocess.run(['clang', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                            '-Wno-parentheses-equality', '-Wno-unused-variable', '-Wno-unused-label',
                            '-fsanitize=undefined', '-I'+str(UPSTREAM/'templates/runtime'),
                            str(file), '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True, env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})


if __name__ == '__main__':
    unittest.main()
