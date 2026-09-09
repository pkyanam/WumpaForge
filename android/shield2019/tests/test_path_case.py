#!/usr/bin/env python3
"""UBSan host integration check for the isolated Xbox path-case patch."""
from pathlib import Path
import argparse, subprocess, tempfile, shutil
ROOT=Path(__file__).resolve().parents[3]
def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--baseline',action='store_true');args=parser.parse_args()
    runtime=ROOT/'third_party/xboxrecomp/src'
    with tempfile.TemporaryDirectory(prefix='wumpa-path-case-') as temporary:
        work=Path(temporary);source=work/'src/kernel/kernel_path.c';source.parent.mkdir(parents=True)
        shutil.copy2(runtime/'kernel/kernel_path.c',source)
        patch=ROOT/'android/shield2019/patches/runtime-path-case.patch'
        if not args.baseline:
            subprocess.run(['git','apply','--check',str(patch)],cwd=work,check=True)
            subprocess.run(['git','apply',str(patch)],cwd=work,check=True)
        binary=work/'test-path-case'
        subprocess.run(['clang','-std=c11','-D_DARWIN_C_SOURCE','-D_GNU_SOURCE',*(['-DWRATH_ANDROID_TV'] if not args.baseline else []),'-O1','-g','-fsanitize=undefined','-fno-sanitize-recover=all','-Werror=implicit-function-declaration','-I'+str(source.parent),'-I'+str(runtime),'-I'+str(runtime/'kernel'),str(Path(__file__).with_name('path_case.c')),'-o',str(binary)],check=True)
        subprocess.run([str(binary)],cwd=work,check=True)
        if not args.baseline:
            subprocess.run(['git','apply','--reverse','--check',str(patch)],cwd=work,check=True)
if __name__=='__main__':main()
