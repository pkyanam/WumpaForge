#!/usr/bin/env python3
"""Offline app validation: closure, personal assets and actual SDL3 runtime loading."""
import argparse
import ctypes
import importlib.util
from pathlib import Path
import plistlib
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('wumpaforge_package', Path(__file__).with_name('package.py'))
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)


class ResolutionTests(unittest.TestCase):
    def test_loader_and_inherited_rpath(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            (root / 'lib').mkdir()
            library = root / 'lib/a.dylib'
            library.touch()
            owner = root / 'lib/owner.dylib'
            executable = root / 'game'
            self.assertEqual(package.resolve_library('@loader_path/a.dylib', owner, executable), library)
            with patch.object(package, 'rpaths', return_value=[]):
                self.assertEqual(package.resolve_library('@rpath/a.dylib', owner, executable,
                                                         ['@executable_path/lib']), library)
                with self.assertRaises(RuntimeError):
                    package.resolve_library('@rpath/missing.dylib', owner, executable)

    def test_rpath_parser(self):
        with patch.object(package, 'output', return_value='Load command 1\n cmd LC_RPATH\n cmdsize 40\n path @loader_path/../lib (offset 12)\n'):
            self.assertEqual(package.rpaths(Path('fixture')), ['@loader_path/../lib'])


def sdl_loader_check(app):
    """No SDL window, game code or extracted game data is opened by this probe."""
    framework = app / 'Contents/Frameworks'
    sdl2 = next(framework.glob('libSDL2*.dylib'))
    lib = ctypes.CDLL(str(sdl2))
    lib.SDL_Init.argtypes = [ctypes.c_uint32]
    lib.SDL_Init.restype = ctypes.c_int
    assert lib.SDL_Init(0) == 0, 'Bundled SDL initialization failed'
    dyld = ctypes.CDLL(None)
    dyld._dyld_image_count.restype = ctypes.c_uint32
    dyld._dyld_get_image_name.argtypes = [ctypes.c_uint32]
    dyld._dyld_get_image_name.restype = ctypes.c_char_p
    loaded = [dyld._dyld_get_image_name(i).decode() for i in range(dyld._dyld_image_count())]
    sdl3 = [Path(name).resolve() for name in loaded if 'libSDL3' in name]
    assert sdl3 == [(framework / 'libSDL3.dylib').resolve()], sdl3
    lib.SDL_Quit()
    print('PASS: SDL2-compat dynamically loaded bundled SDL3 without opening a window')


def validate_app(app):
    contents = app / 'Contents'
    executable = contents / 'MacOS/wrath_native'
    framework = contents / 'Frameworks'
    libraries = [executable, *framework.glob('*.dylib')]
    for binary in libraries:
        package.require_arm64(binary)
        assert not package.rpaths(binary), binary
        for name in package.dependencies(binary):
            if package.system_library(name):
                continue
            prefix = '@executable_path/../Frameworks/'
            assert name.startswith(prefix) and (framework / name[len(prefix):]).is_file(), (binary, name)
    minimum = max((package.minimum_macos(path) for path in libraries),
                  key=lambda value: tuple(map(int, value.split('.'))))
    assert plistlib.loads((contents / 'Info.plist').read_bytes())['LSMinimumSystemVersion'] == minimum
    assets = contents / 'Resources/assets'
    assert assets.is_dir() and not assets.is_symlink()
    assert not any(path.is_symlink() for path in assets.rglob('*')), 'External asset dependency'
    for source in (package.ROOT / 'local/assets').rglob('*'):
        if source.is_file():
            copied = assets / source.relative_to(package.ROOT / 'local/assets')
            assert copied.stat().st_size == source.stat().st_size, copied
    subprocess.run(['codesign', '--verify', '--deep', '--strict', str(app)], check=True)
    # A separate process makes the loaded-library check independent of earlier tests.
    subprocess.run([sys.executable, __file__, '--sdl-probe', str(app)], check=True)
    print(f'PASS: {len(libraries)} native Mach-O files; portable closure, assets, deployment target and signature')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--app', type=Path)
    parser.add_argument('--sdl-probe', type=Path, help=argparse.SUPPRESS)
    args = parser.parse_args()
    if args.sdl_probe:
        sdl_loader_check(args.sdl_probe.resolve())
    else:
        result = unittest.TextTestRunner().run(unittest.defaultTestLoader.loadTestsFromTestCase(ResolutionTests))
        if not result.wasSuccessful():
            raise SystemExit(1)
        if args.app:
            validate_app(args.app.resolve())
