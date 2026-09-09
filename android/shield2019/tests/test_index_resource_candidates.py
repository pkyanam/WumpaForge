#!/usr/bin/env python3
"""UBSan comparison of indexed-resource candidate checks against original scan."""
from pathlib import Path
import subprocess,tempfile,shutil
ROOT=Path(__file__).resolve().parents[3]
with tempfile.TemporaryDirectory(prefix='wumpa-index-candidates-') as temporary:
    work=Path(temporary)
    for name in ('graphics.c','index_bridge.inc'):shutil.copy2(ROOT/'src'/name,work/name)
    patch=ROOT/'android/shield2019/patches/title-index-resource-candidates.patch'
    subprocess.run(['git','apply',str(patch)],cwd=work,check=True)
    binary=work/'check'
    subprocess.run(['clang','-std=c11','-O1','-g','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(work),str(Path(__file__).with_name('index_resource_candidates.c')),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
    subprocess.run(['git','apply','--reverse','--check',str(patch)],cwd=work,check=True)
