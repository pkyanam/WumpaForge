"""CPU-only SAHF flags and binary64 FPREM status, including original CRT loop."""
from pathlib import Path
import math
import random
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
UPSTREAM = ROOT / 'third_party/xboxrecomp'
sys.path.insert(0, str(UPSTREAM))
from tools.recomp import config
from tools.recomp.disasm import Instruction, Operand
from tools.recomp.lifter import Lifter, _make_condition, advance_flag_state, merge_flag_states
from tools.recomp.translator import FunctionTranslator, BatchTranslator, FP_STACK_MACROS


def translated(name, raw):
    raw = bytes.fromhex(raw); start = 0x1000
    info = {'end': start + len(raw), 'name': name}
    translator = FunctionTranslator(raw, {start: info})
    translator._read_func_bytes = lambda a, b: raw[a-start:b-start]
    result = translator.translate_function(start, info)
    assert 'if (_flags' not in result
    return result


def number(value):
    if math.isinf(value): return '-INFINITY' if value < 0 else 'INFINITY'
    return value.hex()


class SahfRemainder(unittest.TestCase):
    def test_native_status_and_original_loop(self):
        functions = []
        # SAHF then overwriting EAX must not refresh AH at branch execution.
        for index, opcode in enumerate((0x92,0x93,0x96,0x97,0x94,0x95,0x9a,0x9b,0x98,0x99)):
            functions.append(translated(f'ah_{index}', f'9eb800000000{opcode-0x20:02x}01c3b801000000c3'))
        # The original NaN comparison status path, with arbitrary old enum state.
        lifter = Lifter(); code = []
        for mnemonic, registers in (('fcom', ('st(1)',)), ('fnstsw', ('ax',)), ('sahf', ())):
            ins = Instruction(0x1000, 2, mnemonic, '', '')
            ins.operands = [Operand('reg', reg=r) for r in registers]
            code.extend(lifter.lift_instruction(ins))
        predicates = [_make_condition(b, 'sahf', [])[0] for b in ('jb','jae','jbe','ja','je','jne','jp','jnp')]
        functions.append('static unsigned compare_flags(double a,double b) { uint32_t _sahf_flags;'
                         'g_fp_top=0;g_fp_stack[0]=a;g_fp_stack[1]=b;g_fp_empty_mask=0xfc;g_fp_cmp=-1;'
                         + ' '.join(code) + 'eax=0;return '
                         + '|'.join(f'((unsigned)!!({p})<<{i})' for i,p in enumerate(predicates)) + ';}')
        # CMP produces OF at8/16/32 bits; SAHF changes SF but preserves that OF.
        for bits, cmpcode in ((8,'38ca'),(16,'6639ca'),(32,'39ca')):
            for index, opcode in enumerate((0x90,0x91,0x9c,0x9d,0x9e,0x9f)):
                functions.append(translated(f'of_{bits}_{index}', cmpcode + f'89d89eb800000000{opcode-0x20:02x}01c3b801000000c3'))
        functions.append(translated('carry_after_sahf', '9eb80000000083d000c3'))
        functions.append(translated('test_of', '85d289d89eb8000000007001c3b801000000c3'))
        # OF from one-bit shifts/rotates is captured before SAHF overwrites CF.
        shifts=('d1c2','d1ca','d1d2','d1da','d1e2','d1ea','0fa4ca01','0facca01')
        for index, raw in enumerate(shifts):
            functions.append(translated(f'shift_of_{index}', '85d2'+raw+'89d89eb8000000007001c3b801000000c3'))

        # Unknown OF must not be upgraded to a known zero; a join intersects it.
        sahf = Instruction(0x1000,1,'sahf','','')
        self.assertEqual(advance_flag_state(sahf,None), ('sahf',[]))
        self.assertIsNone(_make_condition('jo','sahf',[]))
        self.assertEqual(merge_flag_states([('sahf',[]),('sahf_of',[])]),('sahf',[]))
        xbe = ROOT/'local/assets/default.xbe'; original = xbe.exists()
        if original:
            config.configure_from_xbe(str(xbe))
            batch = BatchTranslator(xbe_path=str(xbe),
                func_json_path=str(ROOT/'local/reports/disasm/functions.json'),
                labels_json_path=str(ROOT/'local/reports/disasm/labels.json'),
                identified_json_path=str(ROOT/'local/reports/func_id/identified_functions.json'),
                abi_json_path=str(ROOT/'local/reports/abi/abi_functions.json'))
            function = batch.translator.translate_function(0xF4344,batch.func_db[0xF4344])
            self.assertIn('recomp_fp_remainder',function)
            self.assertIn('_sahf_flags',function)
            functions.append(function)
        pairs=[(x,y) for x in (0.,1.,3.,5.,7.,17.,29.,2.**63,math.ldexp(1.,-1074),sys.float_info.max)
               for y in (2.,3.,4.,7.,math.ldexp(1.,-1074),sys.float_info.max,math.inf)]
        rng=random.Random(4344)
        for _ in range(400):
            a=math.ldexp(rng.uniform(1.,2.),rng.randrange(-1073,1023))
            b=math.ldexp(rng.uniform(1.,2.),rng.randrange(-1073,1023))
            if a and b: pairs.append((a,b))
        rows=[]
        for a,b in pairs:
            for nearest in (0,1):
                if math.isinf(b): q=0;result=a
                else:
                    an,ad=a.as_integer_ratio();bn,bd=b.as_integer_ratio()
                    numerator=an*bd;denominator=ad*bn
                    q,r=divmod(numerator,denominator)
                    if nearest and (2*r>denominator or (2*r==denominator and q&1)): q+=1
                    result=(math.remainder if nearest else math.fmod)(a,b)
                rows.append('{'+f'{number(a)},{number(b)},{number(result)},{q&7},{nearest}'+'}')
        source=r'''
#include <assert.h>
#include <fenv.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#define RECOMP_GENERATED_CODE
#include "recomp_types.h"
RECOMP_TLS uint32_t g_eax,g_ebx,g_ecx,g_edx,g_esp,g_ebp,g_seh_ebp;
RECOMP_TLS double g_fp_stack[8]; RECOMP_TLS int g_fp_top,g_fp_cmp;
RECOMP_TLS uint16_t g_fp_status_word,g_fp_control_word=0x37f;
RECOMP_TLS uint8_t g_fp_empty_mask;
static jmp_buf unsupported; static unsigned trapped;
void recomp_unsupported_instruction(uint32_t address) { if(address!=0x1234)fprintf(stderr,"unexpected boundary %x top=%d st0=%a st1=%a\n",address,g_fp_top,g_fp_stack[g_fp_top],g_fp_stack[(g_fp_top+1)&7]);assert(address==0x1234);trapped++;longjmp(unsupported,1); }
'''+'\n'.join(FP_STACK_MACROS)+'\n'+'\n'.join(functions)+r'''
static unsigned run(void (*fn)(void),unsigned ah,unsigned left,unsigned right) {
    g_eax=g_ebx=ah<<8;g_edx=left;g_ecx=right;g_esp=0;fn();assert(g_esp==4);return g_eax;
}
static unsigned quotient_bits(void) {
    return ((g_fp_status_word>>6)&4)|((g_fp_status_word>>13)&2)|((g_fp_status_word>>9)&1);
}
struct row { double a,b,r; unsigned q,nearest; };
static const struct row rows[]={ROWS};
int main(void) {
    void (*ah_ops[])(void)={AHOPS};
    void (*of_ops[3][6])(void)={OFOPS};
    void (*shift_ops[])(void)={SHIFTOPS};
    const unsigned values[]={0,1,0x7f,0x80,0xff,0x7fff,0x8000,0xffff,0x7fffffff,0x80000000,0xffffffff};
    for(unsigned ah=0;ah<256;ah++) {
        unsigned cf=ah&1,zf=!!(ah&64),pf=!!(ah&4),sf=!!(ah&128);
        unsigned wanted[]={cf,!cf,cf||zf,!cf&&!zf,zf,!zf,pf,!pf,sf,!sf};
        for(unsigned i=0;i<10;i++)assert(run(ah_ops[i],ah,0,0)==wanted[i]);
        assert(run(carry_after_sahf,ah,0,0)==cf);
        assert(run(test_of,ah,0x80000000,0)==0);
        for(unsigned w=0;w<3;w++)for(unsigned a=0;a<11;a++)for(unsigned b=0;b<11;b++) {
            unsigned bits=8u<<w;uint32_t mask=bits==32?UINT32_MAX:(1u<<bits)-1;
            int64_t lhs=values[a]&mask,rhs=values[b]&mask,sign=INT64_C(1)<<(bits-1);
            if(lhs&sign)lhs-=sign*2;if(rhs&sign)rhs-=sign*2;
            int64_t difference=lhs-rhs;unsigned of=difference < -sign || difference >= sign;
            unsigned slt=sf!=of,expected[]={of,!of,slt,!slt,slt||zf,!slt&&!zf};
            for(unsigned i=0;i<6;i++)assert(run(of_ops[w][i],ah,values[a],values[b])==expected[i]);
        }
    }
    for(unsigned ah=0;ah<256;ah++)for(unsigned a=0;a<11;a++)for(unsigned b=0;b<11;b++) {
        uint32_t x=values[a],y=values[b],sign=x>>31;
        uint32_t rol=(x<<1)|sign,ror=(x>>1)|(x<<31),rcl=x<<1;
        uint32_t shl=x<<1,shld=(x<<1)|(y>>31),shrd=(x>>1)|(y<<31);
        unsigned expected[]={(rol>>31)^(rol&1),((ror>>31)^(ror>>30))&1,
            (rcl>>31)^sign,sign,(shl>>31)^sign,sign,(shld>>31)^sign,(shrd>>31)^sign};
        for(unsigned i=0;i<8;i++)assert(run(shift_ops[i],ah,x,y)==expected[i]);
    }
    const double comparisons[][2]={{-1,0},{0,0},{1,0},{NAN,0},{0,NAN},{INFINITY,INFINITY},{-0.,0.}};
    const unsigned compare_expected[]={0xA5,0x96,0xAA,0x55,0x55,0x96,0x96};
    for(unsigned i=0;i<7;i++)assert(compare_flags(comparisons[i][0],comparisons[i][1])==compare_expected[i]);
    const int rounds[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};
    for(unsigned mode=0;mode<4;mode++) {
        assert(fesetround(rounds[mode])==0);
        for(unsigned i=0;i<sizeof(rows)/sizeof(*rows);i++)for(unsigned signs=0;signs<4;signs++) {
            const struct row *v=&rows[i];double x=copysign(v->a,signs&1?-1.:1.),y=copysign(v->b,signs&2?-1.:1.);
            double expected=(signs&1)?-v->r:v->r,actual=x;unsigned steps=0;
            g_fp_status_word=0xBA55u; /* preserve TOP and non-condition bits */
            do { actual=recomp_fp_remainder(actual,y,v->nearest,0x1234);assert(++steps<80); }
            while(g_fp_status_word&0x400u);
            if(!(actual==expected && (actual!=0. || !!signbit(actual)==!!signbit(expected))))fprintf(stderr,"row%u mode%u signs%u near%u x=%a y=%a got=%a want=%a steps%u\n",i,mode,signs,v->nearest,x,y,actual,expected,steps);
            assert(actual==expected && (actual!=0. || !!signbit(actual)==!!signbit(expected)));
            assert(quotient_bits()==v->q);
            assert((g_fp_status_word&~0x4700u)==(0xBA55u&~0x4700u));
            if(isfinite(y)&&x!=0.&&ilogb(fabs(x))-ilogb(fabs(y))>=64)assert(steps>1);
            ORIGINAL
        }
    }
    const double invalid[][2]={{NAN,2},{2,NAN},{INFINITY,2},{2,0},{0,0}};
    for(unsigned i=0;i<5;i++)if(!setjmp(unsupported)) {
        (void)recomp_fp_remainder(invalid[i][0],invalid[i][1],0,0x1234);assert(!"missing boundary");
    }
    assert(trapped==5);
    printf("SAHF all256 AH patterns,805376 width/shift/OF cases; FPREM/1 %zu signed/rounding cases; original CRT loop: ORIGINAL_LABEL; UBSan passed\n",sizeof(rows)/sizeof(*rows)*16);
}
'''
        source=source.replace('ROWS',',\n'.join(rows)).replace('AHOPS',','.join(f'ah_{i}' for i in range(10)))
        source=source.replace('SHIFTOPS',','.join(f'shift_of_{i}' for i in range(8)))
        source=source.replace('OFOPS',','.join('{'+','.join(f'of_{w}_{i}' for i in range(6))+'}' for w in (8,16,32)))
        original_check='''if(!v->nearest) {
                g_fp_top=0;g_fp_stack[0]=y;g_fp_stack[1]=x;g_fp_empty_mask=0xfc;
                recomp_fp_examine();g_fp_status_word|=0x400u;g_esp=0;
                sub_000F4344();assert(g_esp==4 && g_fp_top==1);
                assert(g_fp_stack[g_fp_top]==expected && (expected!=0. || !!signbit(g_fp_stack[g_fp_top])==!!signbit(expected)));
                assert(!(g_fp_status_word&0x400u) && quotient_bits()==v->q);
            }'''
        source=source.replace('ORIGINAL_LABEL','passed' if original else 'not available (BYO XBE)').replace('ORIGINAL',original_check if original else '')
        with tempfile.TemporaryDirectory(prefix='wumpaforge-sahf-') as tmp:
            path=Path(tmp)/'test.c';path.write_text(source);binary=Path(tmp)/'test'
            subprocess.run(['clang','-std=c11','-O2','-Wall','-Wextra','-Werror','-Wno-unused-variable','-Wno-unused-but-set-variable','-Wno-unused-label',
                            '-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(UPSTREAM/'templates/runtime'),str(path),'-o',str(binary)],check=True)
            subprocess.run([str(binary)],check=True,timeout=20)


if __name__=='__main__': unittest.main()
