#!/usr/bin/env python3
"""Run the production cache block with an upload observer and exact float bits."""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
HERE = ROOT / 'android/shield2019'


def main():
    with tempfile.TemporaryDirectory() as directory:
        work = Path(directory)
        (work / 'shader_bridge.inc').write_bytes((ROOT / 'src/shader_bridge.inc').read_bytes())
        # Replay only patches touching this file, using the same ordering as setup.
        for patch in sorted((HERE / 'patches').glob('title-*.patch')):
            if '--- a/shader_bridge.inc' in patch.read_text():
                subprocess.run(['git', 'apply', str(patch)], cwd=work, check=True)
        source = (work / 'shader_bridge.inc').read_text()
        block = re.search(r'    if\(!p->vertex_uniform_valid \|\|.*?p->vertex_uniform_valid=1;\n    }', source, re.S)
        assert block
        assert 'slot->vertex_uniform_valid=0;' in source
        fixture = r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
static float s_vertex_constants[192][4];
struct Program { unsigned vertex_uniform_valid; float vertex_uniform_values[192][4]; int u_vconstants_location; };
static unsigned uploads;
static float observed[192][4];
static void glUniform4fv(int location,int count,const float *values) {
    assert(location==7 && count==192);++uploads;memcpy(observed,values,sizeof(observed));
}
static void update(struct Program *p) {
BLOCK
}
int main(void) {
    struct Program a={.u_vconstants_location=7},b={.u_vconstants_location=7};
    update(&a);assert(uploads==1);update(&a);assert(uploads==1);
    uint32_t bits=0x80000000u;memcpy(&s_vertex_constants[0][0],&bits,4);
    update(&a);assert(uploads==2 && !memcmp(observed,s_vertex_constants,sizeof(observed)));
    bits=0x7fc00001u;memcpy(&s_vertex_constants[0][0],&bits,4);
    update(&a);assert(uploads==3);update(&a);assert(uploads==3);
    bits=0x7fc00002u;memcpy(&s_vertex_constants[0][0],&bits,4);
    update(&a);assert(uploads==4);
    update(&b);assert(uploads==5);update(&a);assert(uploads==5);
    a.vertex_uniform_valid=0;update(&a);assert(uploads==6);
    s_vertex_constants[191][3]=1;update(&a);assert(uploads==7);
    assert(!memcmp(observed,s_vertex_constants,sizeof(observed)));
    return 0;
}
'''.replace('BLOCK', block.group(0))
        test = work / 'test.c'
        test.write_text(fixture)
        binary = work / 'test'
        subprocess.run(['cc', '-std=c11', '-O1', '-fsanitize=undefined', str(test), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
    print('PASS: production uniform cache handles repeats, signed zero, NaN payloads, program switches and slot reuse')


if __name__ == '__main__':
    main()
