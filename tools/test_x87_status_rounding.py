"""CPU-only checks of emitted FRNDINT/FTST against guest x87 control/status."""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'third_party/xboxrecomp'))
from tools.recomp.disasm import Instruction
from tools.recomp.lifter import Lifter


class X87StatusRounding(unittest.TestCase):
    def test_emitted_rounding_and_unordered_status(self):
        emitted = {}
        for mnemonic in ('frndint', 'ftst'):
            instruction = Instruction(0x10000, 2, mnemonic, '', '')
            instruction.operands = []
            emitted[mnemonic] = '\n'.join(Lifter().lift_instruction(instruction))
        source = r'''
#include <assert.h>
#include <fenv.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include "recomp_types.h"
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top, g_fp_cmp;
RECOMP_TLS uint16_t g_fp_control_word;
#define fp_top() g_fp_stack[g_fp_top]
static double guest_round(double value, unsigned rc) {
    g_fp_top=3; g_fp_stack[3]=value; g_fp_control_word=0x37f|(rc<<10);
''' + emitted['frndint'] + r'''
    assert(g_fp_top==3);
    return fp_top();
}
static int guest_test(double value) {
    g_fp_top=5; g_fp_stack[5]=value; g_fp_cmp=-99;
''' + emitted['ftst'] + r'''
    assert(g_fp_top==5);
    return g_fp_cmp;
}
int main(void) {
    unsigned failures=0;
    const int modes[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};
    const double values[]={-2.5,-1.5,-0.5,0.5,1.5,2.5,-1.1,1.1,255.5};
    const double expected[4][9]={{-2,-2,0,0,2,2,-1,1,256},
                                {-3,-2,-1,0,1,2,-2,1,255},
                                {-2,-1,0,1,2,3,-1,2,256},
                                {-2,-1,0,0,1,2,-1,1,255}};
    for(unsigned h=0;h<4;h++) {
        assert(!fesetround(modes[h]));
        for(unsigned rc=0;rc<4;rc++) for(unsigned i=0;i<9;i++) {
            double actual=guest_round(values[i],rc);
            if(actual!=expected[rc][i]) {
                if(failures<8)printf("FRNDINT host%u guest%u value%.2f actual%.2f expected%.2f\n",
                                    h,rc,values[i],actual,expected[rc][i]);
                failures++;
            }
        }
        for(unsigned rc=0;rc<4;rc++) {
            assert(isnan(guest_round(NAN,rc)));
            assert(guest_round(INFINITY,rc)==INFINITY);
            assert(guest_round(-INFINITY,rc)==-INFINITY);
            assert(guest_round(0x1p53,rc)==0x1p53);
            assert(signbit(guest_round(-0.0,rc)));
            if(rc!=1)assert(signbit(guest_round(-0.25,rc)));
        }
    }
    assert(!fesetround(FE_TONEAREST));
    const double comparisons[]={-INFINITY,-1.0,-0.0,0.0,1.0,INFINITY,NAN};
    const int compare_expected[]={-1,-1,0,0,1,1,2};
    for(unsigned i=0;i<7;i++) {
        int actual=guest_test(comparisons[i]);
        if(actual!=compare_expected[i]) {
            printf("FTST case%u actual%d expected%d\n",i,actual,compare_expected[i]);
            failures++;
        }
    }
    printf("x87 emitted FRNDINT/FTST: %u mismatches\n",failures);
    return failures?1:0;
}
'''
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            file = directory/'x87_status.c'
            file.write_text(source)
            binary = directory/'x87_status'
            subprocess.run(['clang', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                            '-frounding-math', '-fsanitize=undefined,float-cast-overflow',
                            '-I'+str(ROOT/'third_party/xboxrecomp/templates/runtime'),
                            str(file), '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    unittest.main()
