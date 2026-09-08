#!/usr/bin/env python3
"""Package the existing ARM64 build for local macOS UI inspection and launching."""
from pathlib import Path
import plistlib
import shutil

ROOT = Path(__file__).resolve().parents[1]

def main():
    binary = ROOT / 'build/native/wrath_native'
    if not binary.is_file():
        raise SystemExit('Build the native executable first.')
    app = ROOT / 'build/Wrath Native.app'
    contents = app / 'Contents'
    executable = contents / 'MacOS/wrath_native'
    resources = contents / 'Resources'
    executable.parent.mkdir(parents=True, exist_ok=True)
    resources.mkdir(parents=True, exist_ok=True)
    shutil.copy2(binary, executable)
    assets = resources / 'assets'
    if not assets.is_symlink() and assets.exists():
        raise SystemExit(f'Preserving unexpected existing asset directory: {assets}')
    if assets.is_symlink():
        if assets.resolve() != (ROOT / 'local/assets').resolve():
            raise SystemExit(f'Preserving unexpected asset link: {assets}')
    else:
        assets.symlink_to(ROOT / 'local/assets', target_is_directory=True)
    info = {
        'CFBundleIdentifier': 'local.wrath.native',
        'CFBundleExecutable': 'wrath_native',
        'CFBundleName': 'Wrath Native',
        'CFBundleDisplayName': 'Crash Bandicoot: The Wrath of Cortex',
        'CFBundlePackageType': 'APPL',
        'CFBundleVersion': '0.1',
        'CFBundleShortVersionString': '0.1',
        'LSMinimumSystemVersion': '14.0',
        'NSHighResolutionCapable': True,
        'NSPrincipalClass': 'NSApplication',
    }
    (contents / 'Info.plist').write_bytes(plistlib.dumps(info))
    print(app)

if __name__ == '__main__':
    main()
