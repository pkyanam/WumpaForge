#!/usr/bin/env python3
"""Validate optional program-cache file handling without a GPU or game assets."""
from pathlib import Path
import subprocess, tempfile, shutil
ROOT=Path(__file__).resolve().parents[3]
with tempfile.TemporaryDirectory(prefix='wumpa-shader-cache-') as temporary:
    work=Path(temporary)
    shutil.copy2(ROOT/'src/shader_bridge.inc',work/'shader_bridge.inc')
    for name in ('title-uniform-cache.patch','title-zshader-binary-cache.patch'):
        subprocess.run(['git','apply',str(ROOT/'android/shield2019/patches'/name)],cwd=work,check=True)
    binary=work/'check'
    subprocess.run(['clang','-std=c11','-D_DARWIN_C_SOURCE','-O1','-g','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(work),str(Path(__file__).with_name('shader_binary_cache.c')),'-o',str(binary)],check=True)
    subprocess.run([str(binary),str(work/'cache')],check=True)
