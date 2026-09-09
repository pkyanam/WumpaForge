#!/usr/bin/env python3
"""Cross-build and package the isolated ARM64 Shield development APK; never install."""
from pathlib import Path
import argparse, hashlib, json, os, shutil, subprocess, sys, zipfile
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
WORK=ROOT/'build/shield2019'

def run(command, **kwargs):
    print('+', ' '.join(map(str,command)), flush=True)
    subprocess.run(list(map(str,command)),check=True,**kwargs)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk',type=Path,default=Path.home()/'Library/Android/sdk')
    parser.add_argument('--ndk',type=Path)
    parser.add_argument('--java-home',type=Path,default=Path('/opt/homebrew/opt/openjdk/libexec/openjdk.jdk/Contents/Home'))
    parser.add_argument('--package-only',action='store_true')
    args=parser.parse_args()
    sdk=args.sdk.resolve(); ndk=(args.ndk or sdk/'ndk/27.1.12297006').resolve()
    tools=sdk/'build-tools/36.0.0'; android_jar=sdk/'platforms/android-36/android.jar'
    java=args.java_home.resolve(); env=dict(os.environ,JAVA_HOME=str(java));env['PATH']=str(java/'bin')+os.pathsep+env.get('PATH','')
    for required in [ndk/'build/cmake/android.toolchain.cmake',tools/'aapt2',tools/'d8',tools/'apksigner',android_jar,java/'bin/javac']:
        if not required.exists(): parser.error(f'Missing installed tool: {required}')
    if not args.package_only:
        run([sys.executable,HERE/'prepare.py'])
        run(['cmake','-Wno-author','-Wno-deprecated','-S',HERE,'-B',WORK/'native',f'-DCMAKE_TOOLCHAIN_FILE={ndk}/build/cmake/android.toolchain.cmake','-DANDROID_ABI=arm64-v8a','-DANDROID_PLATFORM=android-30','-DCMAKE_BUILD_TYPE=RelWithDebInfo'])
        run(['cmake','--build',WORK/'native','--parallel','2'])
    package=WORK/'package'; package.mkdir(parents=True,exist_ok=True)
    classes=package/'classes'; dex=package/'dex'; resources=package/'res'
    # Remove only disposable class/dex trees owned by this packager to avoid stale classes.
    for directory in [classes,dex,resources]:
        if directory.exists(): shutil.rmtree(directory)
        directory.mkdir()
    shutil.copytree(HERE/'res',resources,dirs_exist_ok=True)
    shutil.copy2(ROOT/'assets/branding/wumpaforge-icon.png',resources/'drawable/icon.png')
    run([tools/'aapt2','compile','--dir',resources,'-o',package/'resources.zip'])
    run([tools/'aapt2','link','-I',android_jar,'--manifest',HERE/'AndroidManifest.xml','-o',package/'resources.apk',package/'resources.zip'])
    sdl=WORK/'deps/SDL-5d249570393f7a37e037abf22cd6012a4cc56a71'
    sources=sorted((HERE/'java').rglob('*.java'))+sorted((sdl/'android-project/app/src/main/java').rglob('*.java'))
    run([java/'bin/javac','--release','8','-classpath',android_jar,'-d',classes,*sources],env=env)
    jar=package/'classes.jar'
    with zipfile.ZipFile(jar,'w',zipfile.ZIP_DEFLATED) as archive:
        for file in sorted(classes.rglob('*.class')): archive.write(file,file.relative_to(classes))
    run([tools/'d8','--min-api','30','--lib',android_jar,'--output',dex,jar],env=env)
    unsigned=package/'unsigned.apk'; shutil.copy2(package/'resources.apk',unsigned)
    libraries={'libmain.so':WORK/'native/libmain.so','libSDL2.so':WORK/'native/sdl2/libSDL2.so','libshield_probe.so':WORK/'native/libshield_probe.so','libgraphics_check.so':WORK/'native/libgraphics_check.so','libcontroller_check.so':WORK/'native/libcontroller_check.so','libaudio_check.so':WORK/'native/libaudio_check.so'}
    strip=ndk/'toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-strip'
    readelf=strip.with_name('llvm-readelf')
    library_report={}
    with zipfile.ZipFile(unsigned,'a',zipfile.ZIP_DEFLATED) as archive:
        for file in sorted(dex.glob('*.dex')):archive.write(file,file.name)
        for name,source in libraries.items():
            staged=package/name;shutil.copy2(source,staged)
            run([strip,'--strip-debug',staged])
            metadata=subprocess.check_output([str(readelf),'-h','-l','-d',str(staged)],text=True)
            if 'AArch64' not in metadata:raise RuntimeError(f'Non-ARM64 library: {name}')
            (package/(name+'.elf.txt')).write_text(metadata)
            archive.write(staged,'lib/arm64-v8a/'+name)
            library_report[name]={'sha256':hashlib.sha256(staged.read_bytes()).hexdigest(),'bytes':staged.stat().st_size}
    aligned=package/'aligned.apk';run([tools/'zipalign','-P','16','-f','4',unsigned,aligned])
    keystore=WORK/'signing/development.jks';keystore.parent.mkdir(exist_ok=True)
    if not keystore.exists():
        run([java/'bin/keytool','-genkeypair','-keystore',keystore,'-storepass','android','-keypass','android','-alias','wumpaforge-development','-keyalg','RSA','-keysize','2048','-validity','3650','-dname','CN=WumpaForge Local Development'],env=env)
    apk=WORK/'WumpaForge-Shield-Pro-2019-dev.apk'
    run([tools/'apksigner','sign','--ks',keystore,'--ks-pass','pass:android','--key-pass','pass:android','--out',apk,aligned],env=env)
    run([tools/'apksigner','verify','--verbose',apk],env=env)
    run([tools/'zipalign','-c','-P','16','4',apk])
    from artifact_check import inspect_apk
    audit=inspect_apk(apk)
    (WORK/'apk-elf-audit.json').write_text(json.dumps(audit,indent=2)+'\n')
    report={'artifact':str(apk),'sha256':hashlib.sha256(apk.read_bytes()).hexdigest(),'bytes':apk.stat().st_size,'abi':'arm64-v8a','minimum_api':30,'ndk':(ndk/'source.properties').read_text(),'libraries':library_report,'includes_game_assets':False,'device_tested':False,'playability_verified':False}
    (WORK/'apk-manifest.json').write_text(json.dumps(report,indent=2)+'\n')
    print(apk)
if __name__=='__main__':main()
