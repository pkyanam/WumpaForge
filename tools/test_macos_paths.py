#!/usr/bin/env python3
"""Compile the actual user-state path helper without game assets or a window."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    source = work / 'paths.c'
    source.write_text(r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "macos_paths.h"
int main(void) {
    char save[4096],run[4096]; struct stat info;
    assert(!wrath_macos_state_paths(save,sizeof(save),run,sizeof(run)));
    assert(strstr(save,"/Library/Application Support/WumpaForge/saves"));
    assert(!stat(save,&info) && S_ISDIR(info.st_mode));
    assert((info.st_mode&0777)==0700);
    assert(!stat(run,&info) && S_ISDIR(info.st_mode));
    assert(!wrath_macos_state_paths(save,sizeof(save),run,sizeof(run)));
    assert(wrath_macos_state_paths(save,2,run,sizeof(run))==-1 && errno==ENAMETOOLONG);
    char path[4096]; snprintf(path,sizeof(path),"%s/blocker",getenv("HOME"));
    FILE *file=fopen(path,"w"); assert(file); fclose(file);
    assert(!setenv("WRATH_STATE_ROOT",path,1));
    assert(wrath_macos_state_paths(save,sizeof(save),run,sizeof(run))==-1);
    return 0;
}
''')
    executable = work / 'paths'
    subprocess.run(['clang', '-O2', '-g', '-fsanitize=address,undefined', '-I', str(ROOT/'src'),
                    str(source), '-o', str(executable)], check=True)
    env = dict(os.environ, HOME=str(work))
    env.pop('WRATH_STATE_ROOT', None)
    subprocess.run([str(executable)], env=env, check=True)
print('PASS: actual state paths, private directories, rerun, bounds and file collision')
