#!/usr/bin/env python3
"""Build the source-only ADB key-hold helper; never connects to or modifies a device."""
from pathlib import Path
import argparse,os,subprocess,zipfile
ROOT=Path(__file__).resolve().parents[3]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--sdk',type=Path,default=Path.home()/'Library/Android/sdk')
p.add_argument('--java-home',type=Path,default=Path('/opt/homebrew/opt/openjdk/libexec/openjdk.jdk/Contents/Home'))
a=p.parse_args();out=ROOT/'build/shield2019/keyhold';classes=out/'classes';dex=out/'dex'
classes.mkdir(parents=True,exist_ok=True);dex.mkdir(exist_ok=True)
env=dict(os.environ,JAVA_HOME=str(a.java_home));env['PATH']=str(a.java_home/'bin')+os.pathsep+env.get('PATH','')
jar=a.sdk/'platforms/android-36/android.jar'
subprocess.run([str(a.java_home/'bin/javac'),'--release','8','-classpath',str(jar),'-d',str(classes),str(Path(__file__).with_name('WumpaKeyHold.java'))],env=env,check=True)
subprocess.run([str(a.sdk/'build-tools/36.0.0/d8'),'--min-api','30','--lib',str(jar),'--output',str(dex),*[str(x) for x in sorted(classes.glob('*.class'))]],env=env,check=True)
output=out/'wumpa-keyhold.jar'
with zipfile.ZipFile(output,'w',compression=zipfile.ZIP_STORED) as z:z.write(dex/'classes.dex','classes.dex')
print(output)
