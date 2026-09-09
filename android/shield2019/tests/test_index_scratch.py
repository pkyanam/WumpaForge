#!/usr/bin/env python3
"""UBSan checks for bounded indexed draw scratch reuse and failure handling."""
from pathlib import Path
import subprocess,tempfile,shutil
ROOT=Path(__file__).resolve().parents[3]
with tempfile.TemporaryDirectory(prefix='wumpa-index-scratch-') as temporary:
    work=Path(temporary)
    for name in ('graphics.c','index_bridge.inc'):shutil.copy2(ROOT/'src'/name,work/name)
    for name in ('title-index-resource-candidates.patch','title-index-scratch.patch'):
        subprocess.run(['git','apply',str(ROOT/'android/shield2019/patches'/name)],cwd=work,check=True)
    binary=work/'check'
    subprocess.run(['clang','-std=c11','-O1','-g','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(work),str(Path(__file__).with_name('index_scratch.c')),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
