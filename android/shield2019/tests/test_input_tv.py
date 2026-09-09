#!/usr/bin/env python3
"""Run isolated SDL TV input regression; --baseline proves pre-patch failure."""
from pathlib import Path
import argparse, os, shlex, shutil, subprocess, tempfile
ROOT=Path(__file__).resolve().parents[3]
def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--baseline',action='store_true');args=parser.parse_args()
    sdl_flags=shlex.split(subprocess.check_output(['sdl2-config','--cflags','--libs'],text=True))
    runtime=ROOT/'third_party/xboxrecomp/src'
    with tempfile.TemporaryDirectory(prefix='wumpa-tv-input-') as temp:
        work=Path(temp);source=work/'src/input/xinput_device.c';source.parent.mkdir(parents=True);shutil.copy2(runtime/'input/xinput_device.c',source)
        if not args.baseline:
            patch=ROOT/'android/shield2019/patches/runtime-input-tv.patch'
            subprocess.run(['git','apply','--check',str(patch)],cwd=work,check=True)
            subprocess.run(['git','apply',str(patch)],cwd=work,check=True)
        binary=work/'test-input-tv'
        subprocess.run(['clang','-std=c11','-O1','-g','-fsanitize=undefined','-fno-sanitize-recover=all','-DWRATH_ANDROID_TV','-I'+str(runtime),'-I'+str(runtime/'input'),str(Path(__file__).with_name('input_tv.c')),str(source),*sdl_flags,'-o',str(binary)],check=True)
        completed=subprocess.run([str(binary)],cwd=work)
        raise SystemExit(completed.returncode if completed.returncode>=0 else 128-completed.returncode)
if __name__=='__main__':main()
