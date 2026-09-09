#!/usr/bin/env python3
"""Validate Android ARM64 ELF/load alignment and APK inventory without executing code."""
from pathlib import Path
import argparse, hashlib, json, re, struct, zipfile

def inspect_elf(data):
    if len(data)<64 or data[:7]!=b'\x7fELF\x02\x01\x01':raise ValueError('Expected little-endian ELF64')
    kind,machine=struct.unpack_from('<HH',data,16)
    if kind!=3 or machine!=183:raise ValueError('Expected AArch64 ET_DYN (shared object/PIE)')
    phoff=struct.unpack_from('<Q',data,32)[0]
    entry_size,count=struct.unpack_from('<HH',data,54)
    if entry_size!=56 or not count or phoff+entry_size*count>len(data):raise ValueError('Invalid program-header table')
    segments=[];interpreter=None
    for i in range(count):
        typ,flags,offset,address,_,file_size,mem_size,alignment=struct.unpack_from('<IIQQQQQQ',data,phoff+i*entry_size)
        if offset+file_size>len(data):raise ValueError('Program segment exceeds file')
        if typ==1:
            if mem_size<file_size:raise ValueError('LOAD memory size smaller than file')
            if alignment<16384 or alignment&(alignment-1) or (offset-address)%16384:raise ValueError('LOAD is not 16 KiB aligned')
            if flags&3==3:raise ValueError('Writable executable LOAD segment')
            segments.append({'offset':offset,'virtual_address':address,'file_bytes':file_size,'memory_bytes':mem_size,'alignment':alignment,'flags':flags})
        elif typ==3:
            interpreter=data[offset:offset+file_size].rstrip(b'\0').decode('ascii')
            if interpreter!='/system/bin/linker64':raise ValueError('Unexpected executable interpreter')
        elif typ==0x6474e551 and flags&1:raise ValueError('Executable stack requested')
    if not segments or not any(s['flags']&1 for s in segments):raise ValueError('Missing executable LOAD')
    return {'architecture':'AArch64','load_segments':segments,'interpreter':interpreter,'sha256':hashlib.sha256(data).hexdigest()}

def inspect_apk(path):
    libraries={};dex=0
    with zipfile.ZipFile(path) as archive:
        names=archive.namelist()
        if len(names)!=len(set(names)):raise ValueError('Duplicate APK entries')
        for name in names:
            if re.fullmatch(r'lib/arm64-v8a/lib[A-Za-z0-9_]+\.so',name):
                libraries[name]=inspect_elf(archive.read(name))
                if libraries[name]['interpreter'] is not None:raise ValueError('APK library has executable interpreter')
            elif re.fullmatch(r'classes(?:[2-9][0-9]*)?\.dex',name):dex+=1
            elif name in ('AndroidManifest.xml','resources.arsc','res/drawable/banner.xml','res/drawable/icon.png') or re.fullmatch(r'META-INF/[A-Z0-9_.-]+',name):pass
            else:raise ValueError('Unexpected APK payload: '+name)
        required={'lib/arm64-v8a/libmain.so','lib/arm64-v8a/libSDL2.so','lib/arm64-v8a/libshield_probe.so','lib/arm64-v8a/libgraphics_check.so','lib/arm64-v8a/libcontroller_check.so','lib/arm64-v8a/libaudio_check.so'}
        if set(libraries)!=required or dex<1:raise ValueError('Missing/unexpected application libraries or DEX')
    return {'apk':str(path),'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'members':len(names),'libraries':libraries,'no_game_asset_payload':True,'executed':False}

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('artifact',type=Path);args=parser.parse_args()
    report=inspect_apk(args.artifact) if args.artifact.suffix=='.apk' else inspect_elf(args.artifact.read_bytes())
    print(json.dumps(report,indent=2))
if __name__=='__main__':main()
