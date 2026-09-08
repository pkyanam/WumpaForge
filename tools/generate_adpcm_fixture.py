#!/usr/bin/env python3
"""Generate synthetic Xbox ADPCM golden vectors using pinned vgmstream code.
Downloads one source file, compiles only its Xbox decoder and tiny memory I/O
adapter, and records numeric fixtures. No game assets or large dependency build.
"""
from pathlib import Path
import hashlib
import struct
import subprocess
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
REVISION = '09c9f40caae4747e44b6a993b3d5b654cef4d1f7'
URL = f'https://raw.githubusercontent.com/vgmstream/vgmstream/{REVISION}/src/coding/ima_decoder.c'
WORK = ROOT / 'local/reports/adpcm-oracle'
WORK.mkdir(parents=True, exist_ok=True)
source_file = WORK / 'ima_decoder.c'
if not source_file.exists():
    source_file.write_bytes(urllib.request.urlopen(URL).read())
source = source_file.read_text()

def table(name):
    start = source.index('static const ', source.index(name) - 30)
    return source[start:source.index('};', start) + 2]

def function(name):
    line = source.rfind('\n', 0, source.index(name + '(')) + 1
    return source[line:source.index('\n}', line) + 2]

prelude = r'''
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
typedef int16_t sample_t;
typedef struct { const uint8_t *streamfile; off_t offset;
 int32_t adpcm_history1_32; int adpcm_step_index; } VGMSTREAMCHANNEL;
static int clamp16(int x) { return x < -32768 ? -32768 : x > 32767 ? 32767 : x; }
static uint8_t read_u8(off_t p, const uint8_t *s) { return s[p]; }
static int8_t read_s8(off_t p, const uint8_t *s) { return (int8_t)s[p]; }
static int16_t read_s16le(off_t p, const uint8_t *s) { return (int16_t)(s[p] | s[p+1] << 8); }
'''
main = r'''
int main(int argc, char **argv) {
 if (argc != 4) return 2;
 int channels=atoi(argv[1]);
 FILE *f=fopen(argv[2],"rb"); if (!f) return 2;
 fseek(f,0,SEEK_END); long bytes=ftell(f); rewind(f);
 uint8_t *input=malloc((size_t)bytes);
 if (!input || fread(input,1,(size_t)bytes,f)!=(size_t)bytes) return 2;
 fclose(f); size_t blocks=(size_t)bytes/(36*channels);
 int16_t *output=calloc(blocks*64*channels,sizeof(*output)); if (!output) return 2;
 for (size_t b=0;b<blocks;++b) for(int ch=0;ch<channels;++ch) {
  VGMSTREAMCHANNEL stream={0}; stream.streamfile=input;
  decode_xbox_ima(&stream, output+b*64*channels+ch, channels, (int)b*64,64,ch,channels==2);
 }
 f=fopen(argv[3],"wb"); if(!f) return 2;
 fwrite(output,sizeof(*output),blocks*64*channels,f);fclose(f);
 free(output);free(input);return 0;
}
'''
oracle = prelude + '\n' + '\n'.join([
    table('ima_step_size_table'), table('ima_index_table'),
    function('std_ima_expand_nibble_data'), function('std_ima_expand_nibble'),
    function('decode_xbox_ima')]) + '\n' + main
(WORK/'oracle.c').write_text(oracle)
subprocess.run(['clang','-std=c11','-O1',str(WORK/'oracle.c'),'-o',str(WORK/'oracle')],check=True)

def block(predictors, indices, seed):
    channels=len(predictors)
    data=b''.join(struct.pack('<hBB',p,i,0) for p,i in zip(predictors,indices))
    for group in range(8):
        for ch in range(channels):
            data+=bytes(((seed + group*67 + ch*103 + i*37)&255) for i in range(4))
    return data

fixtures={
    'mono':(1,block([-1234],[32],0x10)+block([32700],[88],0x77)+block([-32700],[88],0xff)),
    'stereo':(2,block([1234,-4321],[7,43],0x73)+block([-30000,30000],[80,88],0xef)),
}
header=['/* Synthetic data; reference: '+URL+' */',
        '/* Reference source SHA256: '+hashlib.sha256(source_file.read_bytes()).hexdigest()+' */',
        '#include <stdint.h>']
for name,(channels,encoded) in fixtures.items():
    (WORK/(name+'.adpcm')).write_bytes(encoded)
    subprocess.run([str(WORK/'oracle'),str(channels),str(WORK/(name+'.adpcm')),str(WORK/(name+'.pcm'))],check=True)
    pcm=(WORK/(name+'.pcm')).read_bytes()
    for suffix,ctype,values in [('adpcm','uint8_t',encoded),('pcm','int16_t',struct.unpack('<'+'h'*(len(pcm)//2),pcm))]:
        header.append(f'static const {ctype} golden_{name}_{suffix}[] = {{')
        for i in range(0,len(values),16):header.append('    '+','.join(str(v) for v in values[i:i+16])+',')
        header.append('};')
path=ROOT/'tools/fixtures/xbox_adpcm_golden.h'
path.write_text('\n'.join(header)+'\n')
print(path)
