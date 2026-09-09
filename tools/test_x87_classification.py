"""Actual emitted x87 operations and production TLS definitions, CPU only."""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
UPSTREAM = ROOT/'third_party/xboxrecomp'
sys.path.insert(0, str(UPSTREAM))
from tools.recomp.disasm import Instruction, Operand
from tools.recomp.lifter import Lifter
from tools.recomp.translator import FP_STACK_MACROS


def emitted(mnemonic, *registers):
    instruction = Instruction(0x10000, 2, mnemonic, ', '.join(registers), '')
    instruction.operands = []
    for name in registers:
        op = Operand('reg')
        op.reg = name
        instruction.operands.append(op)
    return '\n'.join(Lifter().lift_instruction(instruction))


class X87Classification(unittest.TestCase):
    def test_unmodeled_save_restore_is_an_explicit_boundary(self):
        for mnemonic, address in (('fnsave', 0xF47D8), ('frstor', 0xF47E4)):
            instruction = Instruction(address, 3, mnemonic, '[ecx+8]', '')
            op = Operand('mem');op.mem_base = 'ecx';op.mem_disp = 8
            instruction.operands = [op]
            lifter = Lifter()
            result = '\n'.join(lifter.lift_instruction(instruction))
            self.assertIn(f'recomp_unsupported_instruction(0x{address:08X}u)', result)
            self.assertEqual(lifter.unimplemented[mnemonic], [address])

    def test_classification_stack_status_and_actual_tls(self):
        functions = '\n'.join(
            f'static void {name}(void) {{ {emitted(mnemonic, *registers)} }}'
            for name, mnemonic, registers in (
                ('initialize', 'fninit', ()), ('clear_exceptions', 'fnclex', ()),
                ('examine', 'fxam', ()), ('duplicate', 'fld', ('st(0)',)),
                ('exchange', 'fxch', ('st(1)',)), ('store2', 'fst', ('st(2)',)),
                ('storepop2', 'fstp', ('st(2)',)), ('comparepop', 'fcomp', ('st(1)',)),
                ('compare_integer', 'fcomi', ('st(1)',)), ('read_status', 'fnstsw', ('ax',)),
                ('clear_tags', 'emms', ()),
            ))
        source = r'''
#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include "recomp_types.h"
#define eax g_eax
''' + '\n'.join(FP_STACK_MACROS) + '\n' + functions + r'''
static void push_value(double value) { fp_push(value); }
static void pop_value(void) { fp_pop(); }
static void expected_status(unsigned conditions) {
    g_eax=0xA5A50000;read_status();
    assert(g_eax==(0xA5A50000u|(g_fp_top<<11)|conditions));
}
static atomic_uint ready;
static void *worker(void *argument) {
    unsigned tag=(unsigned)(uintptr_t)argument;
    assert(g_fp_empty_mask==0xFF && g_fp_status_word==0 && g_fp_control_word==0x37F);
    initialize();
    for(unsigned i=0;i<tag;i++)push_value(-1.0*(i+1));
    g_fp_control_word=(uint16_t)(0x37F|(tag<<10));
    examine();
    atomic_fetch_add(&ready,1);
    while(atomic_load(&ready)!=2)sched_yield();
    for(unsigned i=0;i<1000;i++) {
        assert(g_fp_top==(8-tag) && g_fp_control_word==(0x37F|(tag<<10)));
        assert(g_fp_empty_mask==((1u<<(8-tag))-1u));
        assert(fp_top()==-(double)tag);expected_status(0x0600);sched_yield();
    }
    return NULL;
}
int main(void) {
    assert(g_fp_empty_mask==0xFF && g_fp_control_word==0x37F);
    initialize();expected_status(0);
    const double values[]={0.0,-0.0,1.0,-1.0,INFINITY,-INFINITY,NAN,-NAN,0x1p-1074,-0x1p-1074};
    const unsigned classes[]={0x4000,0x4200,0x0400,0x0600,0x0500,0x0700,0x0100,0x0300,0x0400,0x0600};
    for(unsigned i=0;i<sizeof(values)/sizeof(*values);i++) {
        initialize();push_value(values[i]);examine();expected_status(classes[i]);
        assert(g_fp_top==7 && g_fp_empty_mask==0x7F);
        duplicate();assert(g_fp_top==6 && g_fp_empty_mask==0x3F);
        examine();expected_status(classes[i]);pop_value();pop_value();
        /* FNINIT does not erase underlying register bits. */
        initialize();g_fp_stack[0]=values[i];examine();
        expected_status(0x4100|(signbit(values[i])?0x0200:0));
    }
    initialize();push_value(-3);push_value(2);exchange();
    assert(fp_top()==-3 && fp_st1()==2 && g_fp_empty_mask==0x3F);
    examine();expected_status(0x0600);
    store2();assert(g_fp_empty_mask==0x3E && fp_st(2)==-3);
    storepop2();assert(g_fp_top==7 && g_fp_empty_mask==0x7E);
    /* Compare clears prior C1/class and pops exactly once. */
    examine();expected_status(0x0400);comparepop();expected_status(0);
    assert(g_fp_top==0 && g_fp_empty_mask==0xFE);
    initialize();push_value(1);push_value(NAN);examine();expected_status(0x0100);
    compare_integer();expected_status(0x0100);assert(g_fp_cmp==2);
    comparepop();expected_status(0x4500);
    g_fp_status_word|=0x80FF;clear_exceptions();expected_status(0x4500);
    assert(g_fp_empty_mask==0x7F && g_fp_top==7);
    initialize();for(unsigned i=0;i<8;i++)push_value((double)i);
    assert(g_fp_top==0 && !g_fp_empty_mask);
    clear_tags();assert(g_fp_empty_mask==0xFF && g_fp_top==0);
    initialize();g_fp_stack[0]=-7;g_fp_control_word=0xF7F;
    pthread_t a,b;assert(!pthread_create(&a,NULL,worker,(void*)1));
    assert(!pthread_create(&b,NULL,worker,(void*)2));
    assert(!pthread_join(a,NULL) && !pthread_join(b,NULL));
    assert(g_fp_top==0 && g_fp_empty_mask==0xFF && g_fp_control_word==0xF7F && g_fp_stack[0]==-7);
    puts("PASS emitted x87 class/sign/empty/TOP, stack store/exchange, compare status, FNCLEX/FNINIT and production pthread TLS");
}
'''
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            file = directory/'classification.c';file.write_text(source)
            binary = directory/'classification'
            subprocess.run(['clang', '-std=c11', '-O1', '-ffunction-sections', '-fdata-sections',
                            '-Wl,-dead_strip', '-Wno-deprecated-declarations',
                            '-I'+str(UPSTREAM/'templates/runtime'), '-I'+str(UPSTREAM/'src'),
                            '-I'+str(UPSTREAM/'src/platform'), str(file),
                            str(UPSTREAM/'src/kernel/xbox_memory_layout.c'),
                            str(UPSTREAM/'src/platform/win32_compat.c'), '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    unittest.main()
