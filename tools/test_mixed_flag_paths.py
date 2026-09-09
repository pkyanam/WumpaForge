"""Native emitted-C regression for original mixed flag joins; no game assets.

Runs the real translator on synthetic instruction graphs and compares every
incoming path against independent signed/zero predicates. A failing baseline
is intentional until the lifter captures the required flags at their producers.
"""
from pathlib import Path
import subprocess
import tempfile
import unittest

from test_flag_merge import fixture, FunctionTranslator


def angle_fixture():
    # Same flag/control-flow shape as 62498..62510, with input EAX=delta,
    # EDI=heading and EBX selecting CMP(abs(delta),4000) versus wrapped TEST.
    code = bytearray(); labels = {}; branches = []
    def emit(s): code.extend(bytes.fromhex(s))
    def jump(op, label):
        code.append(op); branches.append((len(code), label)); code.append(0)
    emit('85db'); jump(0x74, 'wrapped')
    emit('9933c22bc281c700c000003d00400000'); jump(0xEB, 'join')
    labels['wrapped'] = len(code)
    emit('3d00800000'); jump(0x7E, 'negative')
    emit('2d00000100'); jump(0xEB, 'test')
    labels['negative'] = len(code)
    emit('3d0080ffff'); jump(0x7D, 'test')
    emit('0500000100')
    labels['test'] = len(code); emit('85c0')
    labels['join'] = len(code); jump(0x7D, 'plus')
    emit('81c700e00000'); jump(0xEB, 'end')
    labels['plus'] = len(code); emit('81c700200000')
    labels['end'] = len(code); emit('8bc7c3')
    for at, label in branches: code[at] = (labels[label]-at-1) & 255
    raw=bytes(code); start=0x1000
    info={'end':start+len(raw), 'name':'angle_path'}
    translator=FunctionTranslator(raw,{start:info})
    translator._read_func_bytes=lambda a,b: raw[a-start:b-start]
    return translator.translate_function(start,info)


class MixedFlagPaths(unittest.TestCase):
    def test_native_mixed_paths(self):
        functions=[]; calls=[]
        for backwards in (False, True):
            for width, cmp_bytes, test_bytes in (
                    (32,'3d00400000','85db'),
                    (16,'663d0040','6685db'),
                    (8,'3c40','84db')):
                for condition, opcode in enumerate((0x7C,0x7D,0x7E,0x7F)):
                    name=f'signed_{width}_{condition}_{int(backwards)}'
                    functions.append(fixture(name,cmp_bytes,test_bytes,opcode,backwards))
                    calls.append(f'check_signed({name},"{name}",{width},{condition});')
        # Arithmetic predecessors require the result of the executed operation,
        # including DEC16 wrapping to zero, not a refreshed copy of EAX.
        for name,left,right in (
                ('cmp_dec16','6683fb00','66ffcb'),
                ('cmp_decmem16','66833d0020000000','66ff0d00200000'),
                ('test_dec32','85db','ffcb'),
                ('cmp_sub32','83fb00','83eb08')):
            functions.append(fixture(name,left,right,0x75,True))
        functions.append(angle_fixture())
        source=r'''
#include <stdint.h>
#include <stdio.h>
#include <limits.h>
static uint32_t eax,ebx,ecx,edx,edi,esp,g_ebp,g_seh_ebp;
static uint16_t memory16[0x3000/2];
#define MEM16(a) memory16[(uint32_t)(a)/2]
#define LO8(a) ((uint8_t)(a))
#define LO16(a) ((uint16_t)(a))
#define SET_LO8(a,v) ((a)=((a)&0xffffff00u)|(uint8_t)(v))
#define SET_LO16(a,v) ((a)=((a)&0xffff0000u)|(uint16_t)(v))
#define TEST_Z(a,b) (((a)&(b))==0)
#define CMP_EQ(a,b) ((a)==(b))
#define CMP_NE(a,b) ((a)!=(b))
#define CMP_L(a,b) ((a)<(b))
#define CMP_LE(a,b) ((a)<=(b))
#define CMP_GE(a,b) ((a)>=(b))
#define CMP_G(a,b) ((a)>(b))
'''+'\n'.join(functions)+r'''
static unsigned checks,errors;
static void expect(const char *name,uint32_t path,uint32_t value,uint32_t got,uint32_t want) {
    checks++; if(got==want)return;
    if(errors++<16) fprintf(stderr,"FAIL %s path%u input%08X got%08X want%08X\n",name,path,value,got,want);
}
static uint32_t run(void(*fn)(void),unsigned p,uint32_t value) {
    edi=p; eax=ebx=value; ecx=0; esp=0x8000;
    MEM16(0x2000)=(uint16_t)value; fn(); return eax;
}
static int32_t signed_width(uint32_t value,unsigned width) {
    return width==8?(int8_t)value:width==16?(int16_t)value:(int32_t)value;
}
static const uint32_t values[]={0,1,7,8,9,0x3f,0x40,0x41,0x7f,0x80,0xff,
    0x3fff,0x4000,0x4001,0x7fff,0x8000,0xffff,0x10000,0x7fffffff,0x80000000,0xffffffff};
static void check_signed(void(*fn)(void),const char *name,unsigned width,unsigned condition) {
    for(unsigned p=0;p<2;p++) for(unsigned i=0;i<sizeof(values)/sizeof(*values);i++) {
        int32_t a=signed_width(values[i],width),b=p?(width==8?0x40:0x4000):0;
        unsigned want=condition==0?a<b:condition==1?a>=b:condition==2?a<=b:a>b;
        expect(name,p,values[i],run(fn,p,values[i]),want);
    }
}
static uint16_t turn_rot(uint16_t current,uint16_t target) {
    int32_t delta=(int32_t)target-current;
    if(delta>0x8000)delta-=0x10000; else if(delta< -0x8000)delta+=0x10000;
    if(delta>0x444)return current+0x444;
    if(delta< -0x444)return current-0x444;
    return target;
}
int main(void) {
'''+ '\n'.join(calls)+r'''
    for(unsigned p=0;p<2;p++) for(unsigned i=0;i<sizeof(values)/sizeof(*values);i++) {
        uint32_t v=values[i];
        expect("cmp_dec16",p,v,run(cmp_dec16,p,v),p?((uint16_t)v!=0):((uint16_t)(v-1)!=0));
        expect("cmp_decmem16",p,v,run(cmp_decmem16,p,v),p?((uint16_t)v!=0):((uint16_t)(v-1)!=0));
        expect("test_dec32",p,v,run(test_dec32,p,v),p?(v!=0):(v-1!=0));
        expect("cmp_sub32",p,v,run(cmp_sub32,p,v),p?(v!=0):(v-8!=0));
    }
    const int32_t deltas[]={-65535,-32769,-32768,-16385,-16384,-1,0,1,16383,16384,16385,32767,32768,32769,65535};
    for(unsigned p=0;p<2;p++)for(unsigned i=0;i<sizeof(deltas)/sizeof(*deltas);i++) {
        int32_t d=deltas[i],a=d; uint32_t heading=0x1234;
        if(p)a=a<0?-a:a;
        else if(a>0x8000)a-=0x10000; else if(a< -0x8000)a+=0x10000;
        uint16_t want=(uint16_t)(heading+(p?0xc000:0)+((p?a>=0x4000:a>=0)?0x2000:0xe000));
        eax=(uint32_t)d; edi=heading; ebx=p; esp=0x8000; angle_path();
        expect("original-angle-target",p,(uint32_t)d,(uint16_t)eax,want);
        expect("original-angle-smoothing",p,(uint32_t)d,turn_rot(heading,(uint16_t)eax),turn_rot(heading,want));
    }
    printf("Mixed signed/ZF and mask-angle paths: %u checks, %u mismatches\n",checks,errors);
    return errors!=0;
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'mixed.c'; path.write_text(source)
            binary=Path(tmp)/'mixed'
            subprocess.run(['clang','-std=c11','-O2','-fsanitize=undefined',str(path),'-o',str(binary)],check=True)
            subprocess.run([str(binary)],check=True)


if __name__=='__main__': unittest.main()
