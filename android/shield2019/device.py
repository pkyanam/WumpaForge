#!/usr/bin/env python3
"""Explicit-serial Shield installation/import tools. No network discovery or auto-connect."""
from pathlib import Path, PurePosixPath
import argparse, hashlib, importlib.util, json, re, shlex, subprocess, sys, time
HERE=Path(__file__).resolve().parent; ROOT=HERE.parents[1]
PACKAGE='org.wumpaforge.shield'
STORAGE=f'/sdcard/Android/data/{PACKAGE}/files'
XBE_SHA='e8d7cbf225d899eb88227c11d1f40434c34e27c1b23ed946fb2c3471168d2f4d'

def verified_assets(assets, report, require_original=True):
    assets=assets.resolve(); records=json.loads(report.read_text()); result=[];seen=set()
    if not isinstance(records,list) or not records:raise ValueError('Empty/invalid verification report')
    for item in records:
        name=item.get('path',''); p=PurePosixPath(name)
        if not name or p.is_absolute() or '..' in p.parts or str(p)!=name or any(c in name for c in '\r\n\\'):
            raise ValueError('Unsafe asset path')
        if name in seen:raise ValueError('Duplicate asset path')
        seen.add(name)
        digest=item.get('disc_sha256',''); size=item.get('size')
        if item.get('matches') is not True or not re.fullmatch('[0-9a-f]{64}',digest) or type(size) is not int or size<0:
            raise ValueError('Verification report contains an unverified asset')
        file=assets/name
        if not file.resolve().is_relative_to(assets) or any(parent.is_symlink() for parent in [file,*file.parents] if parent!=assets and parent.is_relative_to(assets)):
            raise ValueError('Symlink asset is not imported')
        if not file.is_file() or file.stat().st_size!=size:raise ValueError(f'Asset size changed: {name}')
        with file.open('rb') as source:
            actual=hashlib.file_digest(source,'sha256').hexdigest()
        if actual!=digest:raise ValueError(f'Asset bytes changed: {name}')
        result.append((name,size,digest))
    if require_original and not any(name=='default.xbe' and digest==XBE_SHA for name,_,digest in result):
        raise ValueError('Missing supported original Xbox executable')
    actual_files={str(p.relative_to(assets)) for p in assets.rglob('*') if p.is_file()}
    if actual_files!=seen:raise ValueError('Asset tree differs from verified file inventory')
    return sorted(result)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action',choices=['inspect','install','import-assets','logs'])
    parser.add_argument('--serial',help='Exact already-connected ADB serial; required except dry-run')
    parser.add_argument('--adb',type=Path,default=Path.home()/'Library/Android/sdk/platform-tools/adb')
    parser.add_argument('--assets',type=Path,default=ROOT/'local/assets')
    parser.add_argument('--report',type=Path,default=ROOT/'local/reports/asset-sha256-verification.json')
    parser.add_argument('--apk',type=Path,default=ROOT/'build/shield2019/WumpaForge-Shield-Pro-2019-dev.apk')
    parser.add_argument('--dry-run',action='store_true',help='Verify local inputs and print plan; never invoke ADB')
    args=parser.parse_args()
    if not args.dry_run and not args.serial:parser.error('Supply the exact authorized ADB serial')
    files=verified_assets(args.assets,args.report) if args.action=='import-assets' else []
    if args.action=='install' and not args.apk.is_file():parser.error('Build the APK first')
    if args.dry_run:
        print(json.dumps({'action':args.action,'target':'NVIDIA mdarcy / ARM64 / API30+','device_contacted':False,'asset_files':len(files),'asset_bytes':sum(size for _,size,_ in files),'plan':['Inspect exact selected device before writes','Preserve any existing assets and saves','Verify staged SHA-256 checksums before activation']},indent=2));return
    def adb(*command, capture=True):
        return subprocess.run([str(args.adb),'-s',args.serial,*map(str,command)],check=True,text=True,stdout=subprocess.PIPE if capture else None).stdout
    def shell(*command):return adb('shell',shlex.join(map(str,command)))
    spec=importlib.util.spec_from_file_location('shield_preflight',HERE.parent/'preflight.py');preflight=importlib.util.module_from_spec(spec);spec.loader.exec_module(preflight)
    values={key:shell('getprop',key).strip() for key in preflight.KEYS}
    page=int(shell('getconf','PAGE_SIZE').strip());assessment=preflight.assess(values,page)
    reports=ROOT/'local/reports/shield2019';reports.mkdir(parents=True,exist_ok=True)
    (reports/'device-preflight.json').write_text(json.dumps(assessment,indent=2)+'\n')
    if not assessment['metadata_gate_passed']:raise RuntimeError('\n'.join(assessment['blockers']))
    if args.action=='inspect': print(json.dumps(assessment,indent=2));return
    if args.action=='install':
        adb('install','-r',args.apk,capture=False)
        print('Installed. Open WumpaForge on the TV and run diagnostics before starting the game.');return
    if args.action=='logs':
        for name in ('native.log','graphics-check.log','controller-check.log','audio-check.log','diagnostics.txt'):
            result=subprocess.run([str(args.adb),'-s',args.serial,'pull',STORAGE+'/'+name,str(reports/name)],check=False)
            if result.returncode:print('Log not available:',name)
        return
    # The launcher must have created app storage before importing.
    shell('test','-d',STORAGE)
    lock=STORAGE+'/.wumpaforge-import-lock'
    shell('mkdir',lock)
    try:
        destination=STORAGE+'/assets'
        exists=subprocess.run([str(args.adb),'-s',args.serial,'shell',shlex.join(['test','-e',destination])],check=False)
        if exists.returncode==0:raise RuntimeError('Existing assets preserved; this initial importer only installs into an empty asset destination')
        if exists.returncode!=1:raise RuntimeError('Could not establish whether remote assets exist')
        stage=STORAGE+'/assets.incoming-'+str(time.time_ns())
        shell('mkdir',stage)
        manifest=reports/'transfer.sha256'
        manifest.write_text(''.join(f'{digest}  ./{name}\n' for name,_,digest in files))
        # adb push directory/. copies only the validated tree contents into owned staging.
        adb('push',str(args.assets.resolve())+'/.',stage,capture=False)
        adb('push',manifest,stage+'/.wumpaforge-sha256',capture=False)
        script='cd '+shlex.quote(stage)+' && sha256sum -c .wumpaforge-sha256'
        check=adb('shell',script)
        (reports/'remote-asset-verification.txt').write_text(check)
        shell('rm',stage+'/.wumpaforge-sha256')
        # Recheck before activation; never intentionally replace an existing asset tree.
        adb('shell','test ! -e '+shlex.quote(destination)+' && mv '+shlex.quote(stage)+' '+shlex.quote(destination))
    finally:
        # Leave any failed incoming tree for diagnosis; never delete user assets.
        subprocess.run([str(args.adb),'-s',args.serial,'shell',shlex.join(['rmdir',lock])],check=False)
    print(f'Imported and remotely verified {len(files)} assets. Saves were untouched.')
if __name__=='__main__':main()
