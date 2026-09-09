#!/usr/bin/env python3
"""Create a self-contained, locally signed macOS app from your own game build."""
from pathlib import Path
import argparse
import hashlib
import json
import plistlib
import re
import shutil
import subprocess
import tempfile
import os


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

def minimum_macos(binary):
    """Use the deployment target embedded by the compiler, not a guessed version."""
    metadata = subprocess.check_output(
        ['xcrun', 'vtool', '-show-build', str(binary)], text=True)
    versions = re.findall(r'^\s*minos\s+(\d+(?:\.\d+){0,2})\s*$', metadata, re.MULTILINE)
    platforms = re.findall(r'^\s*platform\s+(\S+)\s*$', metadata, re.MULTILINE)
    if len(versions) != 1 or platforms != ['MACOS']:
        raise SystemExit('Expected one native macOS build-version record; preserving the app.')
    return versions[0]


def output(command):
    return subprocess.check_output(command, text=True).strip()


def system_library(name):
    return name.startswith(('/usr/lib/', '/System/Library/'))


def dependencies(binary):
    return [line.strip().split(' (compatibility version', 1)[0]
            for line in output(['otool', '-L', str(binary)]).splitlines()[1:]]


def rpaths(binary):
    return re.findall(r'cmd LC_RPATH\n\s*cmdsize \d+\n\s*path (.+?) \(offset',
                      output(['otool', '-l', str(binary)]))


def resolve_library(name, owner, executable, inherited=()):
    def expand(value):
        return value.replace('@loader_path', str(owner.parent)).replace(
            '@executable_path', str(executable.parent))
    if name.startswith('@rpath/'):
        candidates = [Path(expand(path)) / name[len('@rpath/'):]
                      for path in (*rpaths(owner), *inherited)]
    else:
        candidates = [Path(expand(name))]
    for candidate in candidates:
        if candidate.is_file():
            return candidate.resolve()
    raise RuntimeError(f'Cannot locate dependency {name} required by {owner}')


def require_arm64(binary):
    if 'arm64' not in output(['lipo', '-archs', str(binary)]).split():
        raise RuntimeError(f'Dependency has no native arm64 code: {binary}')


def bundle_libraries(binary, executable, frameworks):
    """Copy the Mach-O closure, including SDL2-compat's runtime-loaded SDL3."""
    frameworks.mkdir()
    copied, names, edges = {}, {}, {}
    inherited = [path.replace('@loader_path', str(binary.parent)) for path in rpaths(binary)]
    queue = [binary.resolve()]
    destinations = {binary.resolve(): executable}
    while queue:
        source = queue.pop(0)
        require_arm64(source)
        linked = dependencies(source)
        # Dylib IDs appear in otool -L too; they are not dependencies.
        ids = output(['otool', '-D', str(source)]).splitlines()[1:]
        linked = [name for name in linked if name not in ids and not system_library(name)]
        edges[source] = []
        for name in linked:
            resolved = resolve_library(name, source, binary, inherited)
            edges[source].append((name, resolved))
            if resolved not in destinations:
                basename = resolved.name
                if basename in names and names[basename] != resolved:
                    raise RuntimeError(f'Conflicting dependency filename: {basename}')
                names[basename] = resolved
                destinations[resolved] = frameworks / basename
                queue.append(resolved)
        if 'libSDL2' in source.name and 'sdl2-compat' in str(source):
            # SDL2-compat uses dlopen("@loader_path/libSDL3.dylib").
            sdl3 = Path(output(['brew', '--prefix', 'sdl3'])) / 'lib/libSDL3.dylib'
            resolved = sdl3.resolve(strict=True)
            if resolved not in destinations:
                if 'libSDL3.dylib' in names and names['libSDL3.dylib'] != resolved:
                    raise RuntimeError('Conflicting SDL3 library')
                names['libSDL3.dylib'] = resolved
                destinations[resolved] = frameworks / 'libSDL3.dylib'
                queue.append(resolved)
    for source, destination in destinations.items():
        if destination != executable:
            shutil.copy2(source, destination)
            os.chmod(destination, destination.stat().st_mode | 0o200)
        if destination != executable:
            subprocess.run(['install_name_tool', '-id',
                            '@executable_path/../Frameworks/' + destination.name,
                            str(destination)], check=True)
        for old, resolved in edges[source]:
            subprocess.run(['install_name_tool', '-change', old,
                            '@executable_path/../Frameworks/' + destinations[resolved].name,
                            str(destination)], check=True)
        for old in rpaths(destination):
            subprocess.run(['install_name_tool', '-delete_rpath', old, str(destination)], check=True)
        copied[source] = destination
    # Verify the actual copied closure, not just the source dependency graph.
    for destination in copied.values():
        require_arm64(destination)
        for name in dependencies(destination):
            if system_library(name):
                continue
            prefix = '@executable_path/../Frameworks/'
            if not name.startswith(prefix) or not (frameworks / name[len(prefix):]).is_file():
                raise RuntimeError(f'Nonportable dependency in {destination}: {name}')
    return list(copied.values())


def package(binary, asset_source, app, linked_assets=False):
    if not binary.is_file():
        raise RuntimeError('Build the native executable first.')
    if not (asset_source / 'default.xbe').is_file() or not (asset_source / 'Crashdat').is_dir():
        raise RuntimeError('Extract your supported USA Xbox ISO before packaging.')
    if app.is_symlink():
        raise RuntimeError(f'Refusing to replace a linked destination: {app}')
    if app.exists():
        info = app / 'Contents/Info.plist'
        if not info.is_file() or plistlib.loads(info.read_bytes()).get('CFBundleIdentifier') not in (
                'local.wrath.native', 'org.wumpaforge.game'):
            raise RuntimeError(f'Refusing to replace an unrelated existing destination: {app}')
    app.parent.mkdir(parents=True, exist_ok=True)
    # Build beside the output, so failed packaging leaves the previous app intact.
    with tempfile.TemporaryDirectory(prefix='.wumpaforge-package-', dir=app.parent) as temporary:
        staged = Path(temporary) / app.name
        contents = staged / 'Contents'
        executable = contents / 'MacOS/wrath_native'
        resources = contents / 'Resources'
        executable.parent.mkdir(parents=True)
        resources.mkdir(parents=True)
        shutil.copy2(binary, executable)
        libraries = bundle_libraries(binary, executable, contents / 'Frameworks')
        deployment_target = max((minimum_macos(path) for path in libraries),
                                key=lambda value: tuple(map(int, value.split('.'))))
        shutil.copy2(build_icon(), resources / 'WumpaForge.icns')
        if linked_assets:
            (resources / 'assets').symlink_to(asset_source.resolve(), target_is_directory=True)
        else:
            # Follow source links, never leave external dependencies in a personal app.
            shutil.copytree(asset_source, resources / 'assets', symlinks=False)
        shutil.copy2(ROOT / 'THIRD_PARTY_NOTICES.md', resources)
        shutil.copytree(ROOT / 'LICENSES', resources / 'LICENSES')
        if (ROOT / 'LICENSE').is_file():
            shutil.copy2(ROOT / 'LICENSE', resources / 'LICENSE')
        info = {
            'CFBundleIdentifier': 'org.wumpaforge.game',
            'CFBundleExecutable': 'wrath_native',
            'CFBundleName': 'WumpaForge',
            'CFBundleDisplayName': 'WumpaForge — The Wrath of Cortex',
            'CFBundlePackageType': 'APPL',
            'CFBundleIconFile': 'WumpaForge.icns',
            'CFBundleVersion': '0.1',
            'CFBundleShortVersionString': '0.1',
            'LSMinimumSystemVersion': deployment_target,
            'NSHighResolutionCapable': True,
            'NSPrincipalClass': 'NSApplication',
        }
        (contents / 'Info.plist').write_bytes(plistlib.dumps(info))
        for library in sorted(libraries, key=lambda path: path == executable):
            subprocess.run(['codesign', '--force', '--sign', '-', str(library)], check=True)
        manifest = {
            'architecture': 'arm64', 'minimum_macos': deployment_target,
            'game_executable_sha256': hashlib.sha256((asset_source / 'default.xbe').read_bytes()).hexdigest(),
            'libraries': [{'path': str(path.relative_to(contents)),
                           'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
                          for path in sorted(libraries)],
            'personal_build': True,
        }
        (resources / 'build-info.json').write_text(json.dumps(manifest, indent=2) + '\n')
        subprocess.run(['codesign', '--force', '--sign', '-', str(staged)], check=True)
        subprocess.run(['codesign', '--verify', '--deep', '--strict', str(staged)], check=True)
        backup = Path(temporary) / 'previous.app'
        if app.exists():
            app.rename(backup)
        try:
            staged.rename(app)
        except BaseException:
            if backup.exists():
                backup.rename(app)
            raise
    return deployment_target


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'build/WumpaForge.app')
    parser.add_argument('--linked-assets', action='store_true',
                        help='developer mode: keep an external link instead of copying game data')
    args = parser.parse_args()
    try:
        target = package(ROOT / 'build/native/wrath_native', ROOT / 'local/assets',
                         args.output.expanduser().absolute(), args.linked_assets)
    except (RuntimeError, subprocess.CalledProcessError) as error:
        raise SystemExit(str(error)) from error
    print(f'{args.output.expanduser().absolute()} (Apple Silicon, macOS {target}+)')


if __name__ == '__main__':
    main()
