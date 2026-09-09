#!/usr/bin/env python3
"""Package the existing ARM64 build for local macOS UI inspection and launching."""
from pathlib import Path
import argparse
import plistlib
import shutil
import subprocess
import tempfile


def build_icon():
    """Build standard Retina icon sizes from the project-owned generated artwork."""
    source = ROOT / "assets/branding/wumpaforge-icon.png"
    output = ROOT / "build/branding/WumpaForge.icns"
    if output.is_file() and output.stat().st_mtime_ns >= source.stat().st_mtime_ns:
        return output
    iconset = output.parent / "WumpaForge.iconset"
    iconset.mkdir(parents=True, exist_ok=True)
    sizes = ((16, 1), (16, 2), (32, 1), (32, 2), (128, 1), (128, 2),
             (256, 1), (256, 2), (512, 1), (512, 2))
    rendered = {}
    for points, scale in sizes:
        pixels = points * scale
        suffix = "@2x" if scale == 2 else ""
        destination = iconset / f"icon_{points}x{points}{suffix}.png"
        if pixels in rendered:
            shutil.copy2(rendered[pixels], destination)
        else:
            subprocess.run(["sips", "-z", str(pixels), str(pixels), str(source),
                            "--out", str(destination)], check=True, stdout=subprocess.DEVNULL)
            rendered[pixels] = destination
    subprocess.run(["iconutil", "-c", "icns", str(iconset), "-o", str(output)], check=True)
    return output

ROOT = Path(__file__).resolve().parents[1]

def replace_executable(source, destination):
    """Leave an already mapped executable inode intact when packaging a rerun."""
    with tempfile.NamedTemporaryFile(prefix='.wrath-', dir=destination.parent,
                                     delete=False) as temporary:
        staged = Path(temporary.name)
    try:
        shutil.copy2(source, staged)
        staged.replace(destination)
    finally:
        staged.unlink(missing_ok=True)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'build/Wrath Native.app',
                        help='app destination; use a separate path to stage beside a running build')
    args = parser.parse_args()
    binary = ROOT / 'build/native/wrath_native'
    if not binary.is_file():
        raise SystemExit('Build the native executable first.')
    app = args.output.expanduser().resolve()
    contents = app / 'Contents'
    executable = contents / 'MacOS/wrath_native'
    resources = contents / 'Resources'
    executable.parent.mkdir(parents=True, exist_ok=True)
    resources.mkdir(parents=True, exist_ok=True)
    replace_executable(binary, executable)
    shutil.copy2(build_icon(), resources / "WumpaForge.icns")
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
        'CFBundleIconFile': 'WumpaForge.icns',
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
