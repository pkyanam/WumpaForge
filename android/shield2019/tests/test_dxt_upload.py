#!/usr/bin/env python3
"""Check Android DXT source replay and compressed transfer bounds under UBSan."""
from pathlib import Path
import tempfile,shutil,subprocess
ROOT=Path(__file__).resolve().parents[3]
with tempfile.TemporaryDirectory(prefix='wumpa-dxt-') as temporary:
    work=Path(temporary);shutil.copytree(ROOT/'src',work,dirs_exist_ok=True)
    for patch in sorted((ROOT/'android/shield2019/patches').glob('title-*.patch')):
        subprocess.run(['git','apply',str(patch)],cwd=work,check=True)
    binary=work/'check'
    subprocess.run(['clang','-std=c11','-D_DARWIN_C_SOURCE','-O1','-g','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(work),str(Path(__file__).with_name('dxt_upload.c')),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
