"""Execute synthetic x86 CFG joins through the actual translator and native C.
No game bytes or native game build are needed.
"""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'third_party/xboxrecomp'))
from tools.recomp.disasm import Operand
from tools.recomp.lifter import merge_flag_states
from tools.recomp.translator import FunctionTranslator


def fixture(name, left, right, jcc, backward=False):
    # TEST selector; branch to one of two different CMP/TEST operand sets.
    # Overwrite both source registers with MOV after comparing: the join must
    # consume captured flags, not re-read operands at their new values.
    code = bytearray(); labels = {}; branches = []
    def emit(s): code.extend(bytes.fromhex(s))
    def jump(op, label):
        code.append(op); branches.append((len(code), label)); code.append(0)
    emit('85ff'); jump(0x74, 'right')
    emit(left); emit('b800000000'); jump(0xEB, 'join')
    if not backward:
        labels['right'] = len(code)
        emit(right); emit('bb00000000')
    labels['join'] = len(code)
    jump(jcc, 'yes'); emit('b800000000c3')
    labels['yes'] = len(code); emit('b801000000c3')
    if backward:
        labels['right'] = len(code)
        emit(right); emit('bb00000000'); jump(0xEB, 'join')
    for at, label in branches: code[at] = (labels[label]-at-1) & 255
    raw = bytes(code); start = 0x1000
    info = {'end': start+len(raw), 'name': name}
    translator = FunctionTranslator(raw, {start: info})
    translator._read_func_bytes = lambda a, b: raw[a-start:b-start]
    result = translator.translate_function(start, info)
    assert result
    return result


class FlagMerge(unittest.TestCase):
    def test_native_branch_paths_and_snapshots(self):
        functions = [
            fixture('wave_join', '3d57415645', '39cb', 0x74),
            fixture('wave_memory_join', '817dfc57415645', '394dfc', 0x74),
            fixture('signed_join', '3d00000000', '39cb', 0x7C),
            fixture('test_join', 'a9ff000000', '85cb', 0x74),
            fixture('byte_sign_join', '3c80', '38cb', 0x78),
            fixture('mixed_join', '3d07000000', '85db', 0x74),
            fixture('mixed_back', '3d07000000', '85db', 0x74, True),
            fixture('menu_back', '39442410', '85db', 0x74, True),
            fixture('menu_back_edi', '397c2410', '85db', 0x74, True),
            fixture('mixed_width', '3c80', '6685db', 0x75, True),
        ]
        for function in functions:
            self.assertNotIn('if (_flags', function)
        source = r'''
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
static uint32_t eax,ebx,ecx,edi,esp,g_ebp,g_seh_ebp;
static uint32_t memory[0x9000/4];
#define MEM32(a) memory[(uint32_t)(a)/4]
#define LO8(a) ((uint8_t)(a))
#define LO16(a) ((uint16_t)(a))
#define SET_LO16(a,v) ((a)=((a)&0xffff0000u)|(uint16_t)(v))
#define TEST_Z(a,b) (((a)&(b))==0)
#define CMP_EQ(a,b) ((a)==(b))
#define CMP_L(a,b) ((a)<(b))
''' + '\n'.join(functions) + r'''
static unsigned run(void (*fn)(void), unsigned selector, uint32_t a, uint32_t b) {
    edi=selector; eax=ebx=a; ecx=b; esp=0x8000;
    g_ebp=g_seh_ebp=0x4000; MEM32(0x3FFC)=a; MEM32(esp+0x10)=b;
    fn(); assert(esp==0x8004); return eax;
}
int main(void) {
    const uint32_t values[]={0,1,7,0x80,0x10000,0x45564157,0x45564158,0x80000000,0xFFFFFFFF};
    const unsigned n=sizeof(values)/sizeof(*values);
    for(unsigned p=0;p<2;p++) for(unsigned a=0;a<n;a++) for(unsigned b=0;b<n;b++) {
        uint32_t lhs=values[a], rhs=p?0x45564157:values[b];
        assert(run(wave_join,p,lhs,values[b])==(lhs==rhs));
        assert(run(wave_memory_join,p,lhs,values[b])==(lhs==rhs));
        rhs=p?0:values[b];
        assert(run(signed_join,p,lhs,values[b])==((int32_t)lhs<(int32_t)rhs));
        rhs=p?255:values[b];
        assert(run(test_join,p,lhs,values[b])==((lhs&rhs)==0));
    }
    for(unsigned p=0;p<2;p++) for(unsigned a=0;a<n;a++) for(unsigned b=0;b<n;b++) {
        uint32_t v=values[a];
        assert(run(mixed_join,p,v,values[b])==(p?(v==7):(v==0)));
        assert(run(mixed_back,p,v,values[b])==(p?(v==7):(v==0)));
        assert(run(menu_back,p,v,values[b])==(p?(values[b]==v):(v==0)));
        assert(run(menu_back_edi,p,v,values[b])==(p?(values[b]==p):(v==0)));
        assert(run(mixed_width,p,v,values[b])==(p?((v&255)!=128):((v&65535)!=0)));
    }
    for(unsigned p=0;p<2;p++) for(unsigned a=0;a<256;a++) for(unsigned b=0;b<256;b++) {
        unsigned rhs=p?128:b;
        assert(run(byte_sign_join,p,a,b)==(((a-rhs)&128)!=0));
    }
    puts("PASS: native CMP/TEST CFG merges, backward mixed-ZF menu paths, changed operands/widths, signed32 and exhaustive byte SF");
}
'''
        with tempfile.TemporaryDirectory() as temporary:
            directory=Path(temporary); path=directory/'merge.c'; path.write_text(source)
            binary=directory/'merge'
            subprocess.run(['clang','-std=c11','-O2','-fsanitize=undefined',
                            str(path),'-o',str(binary)],check=True)
            subprocess.run([str(binary)],check=True)

    def test_reject_incompatible_or_unknown_states(self):
        a=Operand('reg',reg='eax'); b=Operand('reg',reg='ebx')
        byte=Operand('reg',reg='al'); imm=Operand('imm',imm=7)
        cmp=('cmp',[a,imm]); other=('cmp',[b,imm])
        self.assertEqual(merge_flag_states([cmp,other]),cmp)
        self.assertEqual(merge_flag_states([cmp,('test',[b,imm])]), ("snapshot_zf", []))
        self.assertEqual(merge_flag_states([cmp,('cmp',[byte,imm])]), ("snapshot_zf", []))
        self.assertIsNone(merge_flag_states([cmp,None]))
        self.assertIsNone(merge_flag_states([]))
        self.assertIsNone(merge_flag_states([('sub',[a,imm]),('sub',[b,imm])]))
        self.assertEqual(merge_flag_states([cmp,cmp]),cmp)
        # The mixed meet provides only ZF. Signed conditions remain unresolved.
        mixed=fixture('mixed_join','3d07000000','85cb',0x7C)
        self.assertIn('if (_flags',mixed)
        unknown=fixture('unknown_join','3d07000000','0f31',0x74,True)
        self.assertIn('if (_flags',unknown)
        call=fixture('call_join','3d07000000','e800000000',0x74,True)
        self.assertIn('if (_flags',call)

if __name__=='__main__': unittest.main()
