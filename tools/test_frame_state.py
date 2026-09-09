"""Synthetic native guest-EBP frame-chain/call-boundary diagnostics.

Compiles generated C only. Clang's deterministic auto-variable pattern makes
the current uninitialized PUSH EBP visible without depending on host stack junk.
"""
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'third_party/xboxrecomp'))
from tools.recomp.translator import FunctionTranslator


def relative(opcode,at,target):
    return bytes([opcode])+struct.pack('<i',target-at-5)


class FrameState(unittest.TestCase):
    def test_native_frame_boundaries(self):
        raw={0x1000:bytes.fromhex('558bec8b45005dc3'), # Save incoming EBP, return saved DWORD.
             0x2000:bytes.fromhex('558bec5dc3'),       # Framed nested call.
             0x3000:bytes.fromhex('8bc5c3')}           # Frameless EBP reader.
        raw[0x4000]=relative(0xe8,0x4000,0x2000)+relative(0xe8,0x4005,0x3000)+b'\xc3'
        raw[0x5000]=bytes.fromhex('558bec')+relative(0xe8,0x5003,0x2000)+bytes.fromhex('b800300000ffd0c9c3')
        # Explicit EBP register argument, with balanced original save/restore.
        raw[0x6000]=bytes.fromhex('55bd67450000')+relative(0xe8,0x6006,0x3000)+bytes.fromhex('5dc3')
        # Standard epilog followed by a tail helper that inherits the restored frame.
        raw[0x7000]=bytes.fromhex('558bec')+relative(0xe8,0x7003,0x2000)+b'\xc9'+relative(0xe9,0x7009,0x3000)
        raw[0x8000]=bytes.fromhex('558bec')+relative(0xe8,0x8003,0x2000)+bytes.fromhex('c9b800300000ffe0')
        # Taken and untaken conditional tails both use the post-LEAVE frame.
        raw[0x9000]=(bytes.fromhex('558bec')+relative(0xe8,0x9003,0x2000)+bytes.fromhex('c985c9')
                     +bytes.fromhex('0f84')+struct.pack('<i',0x3000-0x9011)+bytes.fromhex('8bc5c3'))
        # A discovery boundary can split an original fallthrough in two.
        raw[0xA000]=bytes.fromhex('558bec')+relative(0xe8,0xA003,0x2000)+b'\xc9'
        raw[0xA009]=bytes.fromhex('8bc5c3')
        raw[0xB000]=bytes.fromhex('558bec')+relative(0xe8,0xB003,0x2000)+b'\xc9'+relative(0xe9,0xB009,0x3000)
        # Recognized SEH helpers deliberately exchange a changed EBP. Ordinary
        # nested calls must not erase that exchange or bypass its read-back.
        raw[0xC000]=(relative(0xe8,0xC000,0xD000)+relative(0xe8,0xC005,0x2000)
                     +relative(0xe8,0xC00A,0x3000)+relative(0xe8,0xC00F,0xD100)+b'\xc3')
        raw[0xD000]=bytes.fromhex('bd67450000c3')
        raw[0xD100]=bytes.fromhex('bd70770000c3')
        db={a:{'end':a+len(b),'name':f'sub_{a:08X}'} for a,b in raw.items()}
        functions=[]
        for address,code in raw.items():
            translator=FunctionTranslator(code,db)
            translator.lifter.SEH_PROLOG=0xD000
            translator.lifter.SEH_EPILOG=0xD100
            if address==0xB000:translator.lifter.manual_functions={0x3000}
            translator._read_func_bytes=lambda a,b,base=address,data=code:data[a-base:b-base]
            functions.append(translator.translate_function(address,db[address]))
        source=r'''
#include <stdint.h>
#include <stdio.h>
static uint32_t eax,ebx,ecx,edx,esi,edi,esp,g_ebp,g_seh_ebp;
#define g_esp esp
#define TEST_Z(a,b) (((a)&(b))==0)
static uint32_t memory[0x10000/4];
#define MEM32(a) memory[(uint32_t)(a)/4]
#define PUSH32(s,v) do{uint32_t value=(v);(s)-=4;MEM32(s)=value;}while(0)
#define POP32(s,v) do{(v)=MEM32(s);(s)+=4;}while(0)
#define RECOMP_ABI_CALL(va,fn) (fn)()
static void indirect(uint32_t va);
#define RECOMP_ICALL_SAFE(va,ignored) indirect(va)
#define RECOMP_ITAIL(va) indirect(va)
'''+''.join(f'void sub_{a:08X}(void);\n' for a in raw)+'\n'.join(functions)+r'''
static void indirect(uint32_t va){if(va==0x3000)sub_00003000();else __builtin_trap();}
static unsigned failures,cases;
static void run(void(*fn)(void),const char *name,uint32_t result) {
    cases++;eax=0;ebx=0x1234;esi=0x5678;edi=0x9876;esp=0x8000;
    MEM32(0x8000)=0x12345678;MEM32(0x8004)=0x87654321;
    g_ebp=g_seh_ebp=0x7770;fn();
    if(eax!=result || esp!=0x8004 || g_ebp!=0x7770 || g_seh_ebp!=0x7770
       || ebx!=0x1234 || esi!=0x5678 || edi!=0x9876
       || MEM32(0x8000)!=0x12345678 || MEM32(0x8004)!=0x87654321) {
        fprintf(stderr,"FRAME %s eax%08X expected%08X esp%08X tls_ebp%08X seh%08X expected00007770\n",name,eax,result,esp,g_ebp,g_seh_ebp);failures++;
    }
}
int main(void) {
    run(sub_00001000,"saved-parent-frame",0x7770);
    run(sub_00004000,"frameless-relay",0x7770);
    run(sub_00005000,"framed-indirect-after-child",0x7ffc);
    run(sub_00006000,"explicit-ebp-register-argument",0x4567);
    run(sub_00007000,"restored-frame-tail",0x7770);
    run(sub_00008000,"restored-indirect-tail",0x7770);
    ecx=0;run(sub_00009000,"conditional-tail-taken",0x7770);
    ecx=1;run(sub_00009000,"conditional-tail-untaken",0x7770);
    run(sub_0000A000,"implicit-fallthrough-tail",0x7770);
    run(sub_0000B000,"manual-tail",0x7770);
    run(sub_0000C000,"SEH-changed-frame-exchange",0x4567);
    printf("Guest frame-state cases:%u, mismatches:%u\n",cases,failures);return failures!=0;
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'frames.c';path.write_text(source);binary=Path(tmp)/'frames'
            subprocess.run(['clang','-std=c11','-O1','-ftrivial-auto-var-init=pattern',
                            '-fsanitize=undefined',str(path),'-o',str(binary)],check=True)
            subprocess.run([str(binary)],check=True)


if __name__=='__main__':unittest.main()
