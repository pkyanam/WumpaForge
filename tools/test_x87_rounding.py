"""Execute emitted x87 integer conversions with guest/host rounding separated."""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'third_party/xboxrecomp'))
from tools.recomp.disasm import Instruction, Operand
from tools.recomp.lifter import Lifter


class GuestRounding(unittest.TestCase):
    def test_native_conversions_and_d3dx_white(self):
        functions = []
        for mnemonic in ('fist', 'fistp'):
            for size in (2, 4, 8):
                if mnemonic == 'fist' and size == 8:
                    continue  # x87 has no non-popping qword form.
                instruction = Instruction(0x11EA9B, 3, mnemonic, '', '')
                op = Operand('mem'); op.mem_size = size; op.mem_disp = 16
                instruction.operands = [op]
                code = '\n'.join(Lifter().lift_instruction(instruction))
                functions.append(f'''static int64_t {mnemonic}{size}(double value, unsigned rc) {{
                    g_fp_stack[0]=value; g_fp_top=0; g_fp_control_word=0x37F|(rc<<10);
                    {code}
                    assert(g_fp_top=={int(mnemonic=='fistp')});
                    return SMEM{size*8}(16);
                }}''')
        source = r'''
#include <stdint.h>
#include <math.h>
#include <assert.h>
#include <fenv.h>
#include <stdio.h>
static union {int64_t alignment; unsigned char data[64];} memory;
#define SMEM16(a) (*(int16_t*)(memory.data+(a)))
#define SMEM32(a) (*(int32_t*)(memory.data+(a)))
#define SMEM64(a) (*(int64_t*)(memory.data+(a)))
static double g_fp_stack[8]; static unsigned g_fp_top; static uint16_t g_fp_control_word;
#define fp_top() g_fp_stack[g_fp_top]
#define fp_pop() (g_fp_top=(g_fp_top+1)&7)
''' + '\n'.join(functions) + r'''
int main(void) {
    int64_t (*converters[])(double,unsigned)={fist2,fist4,fistp2,fistp4,fistp8};
    const double values[]={-2.5,-1.5,-0.5,0.5,1.5,2.5,-1.1,1.1,255.5};
    const int expected[4][9]={{-2,-2,0,0,2,2,-1,1,256},
                              {-3,-2,-1,0,1,2,-2,1,255},
                              {-2,-1,0,1,2,3,-1,2,256},
                              {-2,-1,0,0,1,2,-1,1,255}};
    const int host_modes[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};
    for(unsigned h=0;h<4;h++) {
        assert(fesetround(host_modes[h])==0);
        for(unsigned f=0;f<5;f++) for(unsigned rc=0;rc<4;rc++)
            for(unsigned i=0;i<9;i++) assert(converters[f](values[i],rc)==expected[rc][i]);
    }
    assert(fesetround(FE_TONEAREST)==0);
    for(unsigned rc=0;rc<4;rc++) {
        assert(fistp2(NAN,rc)==INT16_MIN); assert(fistp4(INFINITY,rc)==INT32_MIN);
        assert(fistp8(-INFINITY,rc)==INT64_MIN);
        assert(fistp2(0x1p15,rc)==INT16_MIN); assert(fistp4(0x1p31,rc)==INT32_MIN);
        assert(fistp8(0x1p63,rc)==INT64_MIN);
        assert(fistp2(-0x1p15,rc)==INT16_MIN); assert(fistp4(-0x1p31,rc)==INT32_MIN);
        assert(fistp8(-0x1p63,rc)==INT64_MIN);
        assert(fistp8(nextafter(0x1p63,0),rc)==INT64_MAX-1023);
    }
    /* Actual D3DX 0x11EA84 sequence: FISTP(channel*255 + .5), RC=truncate.
       This byte packing previously produced 0x01010100 for opaque white. */
    for(unsigned i=0;i<256;i++) {
        float channel=(float)i/255.0f;
        float scaled=(float)((double)channel*255.0+0.5);
        uint32_t c=(uint32_t)fistp4(scaled,3);
        uint32_t alpha=(uint32_t)fistp4(255.5,3);
        uint32_t packed=((((alpha<<8)|c)<<8)|c)<<8|c;
        assert(packed==(0xFF000000u|i*0x010101u));
    }
    puts("guest x87 rounding: modes, ties, ranges, FIST stack and D3DX bytes pass");
}
'''
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary); file = directory/'rounding.c'; file.write_text(source)
            binary = directory/'rounding'
            subprocess.run(['clang','-std=c11','-O2','-Wall','-Wextra','-Werror',
                            '-fsanitize=undefined,float-cast-overflow',str(file),'-o',str(binary)],check=True)
            subprocess.run([str(binary)],check=True)

if __name__ == '__main__':
    unittest.main()
