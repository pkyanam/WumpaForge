#!/usr/bin/env python3
"""Asset-free regressions for setup caching and save preservation."""
import importlib.util
from pathlib import Path
import tempfile
import hashlib
import sys
import types
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('wumpa_setup', Path(__file__).with_name('setup.py'))
setup = importlib.util.module_from_spec(spec)
spec.loader.exec_module(setup)

class SetupTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.root_patch = patch.object(setup, 'ROOT', self.root)
        self.root_patch.start()
        for folder in ('tools', 'config', 'patches', 'local/generated', 'local/reports/disasm'):
            (self.root / folder).mkdir(parents=True)
        for file in ('tools/pipeline.py', 'tools/bootstrap.py', 'requirements.txt', 'config/seeds.json',
                     'patches/runtime.patch', 'local/generated/game.c', 'local/reports/disasm/functions.json'):
            (self.root / file).write_text('original')
    def tearDown(self):
        self.root_patch.stop()
        self.temp.cleanup()
    def test_cache_checks_inputs_and_every_output(self):
        self.assertFalse(setup.translation_is_current())
        setup.record_translation()
        self.assertTrue(setup.translation_is_current())
        for file in ('config/seeds.json', 'patches/runtime.patch', 'local/generated/game.c'):
            path = self.root / file
            path.write_text('changed')
            self.assertFalse(setup.translation_is_current())
            path.write_text('original')
            self.assertTrue(setup.translation_is_current())
        extra = self.root / 'local/generated/injected.c'
        extra.write_text('extra')
        self.assertFalse(setup.translation_is_current())
        extra.unlink()
        (self.root / 'local/reports/disasm/functions.json').unlink()
        self.assertFalse(setup.translation_is_current())
    def test_corrupt_manifest_rebuilds(self):
        (self.root / 'local/reports/setup-translation.json').write_text('{broken')
        self.assertFalse(setup.translation_is_current())
    def test_original_disc_identity(self):
        class Reader:
            payload = b"supported executable"
            missing = False
            def __init__(self, path): pass
            def __enter__(self): return self
            def __exit__(self, *args): pass
            def find(self, name):
                return None if self.missing else types.SimpleNamespace(is_dir=False)
            def read(self, entry): return self.payload
        module = types.ModuleType('tools.xiso.xdvdfs')
        module.Xiso = Reader
        with patch.dict(sys.modules, {'tools.xiso.xdvdfs': module}), patch.object(
                setup, 'USA_XBE_SHA256', hashlib.sha256(Reader.payload).hexdigest()):
            setup.verify_original(Path('USA.iso'))
            Reader.payload = b'wrong console or region'
            with self.assertRaisesRegex(ValueError, 'Unsupported game executable'):
                setup.verify_original(Path('other.iso'))
            Reader.missing = True
            with self.assertRaisesRegex(ValueError, 'no original default.xbe'):
                setup.verify_original(Path('not Xbox.iso'))

    def test_save_migration_never_overwrites(self):
        source = self.root / 'local/saves'
        source.mkdir()
        (source / 'save.dat').write_bytes(b'old progress')
        home = self.root / 'home'
        target = home / 'Library/Application Support/WumpaForge/saves/save.dat'
        with patch.object(Path, 'home', return_value=home):
            setup.migrate_legacy_saves()
            self.assertEqual(target.read_bytes(), b'old progress')
            target.write_bytes(b'new progress')
            setup.migrate_legacy_saves()
        self.assertEqual(target.read_bytes(), b'new progress')
        self.assertEqual((source / 'save.dat').read_bytes(), b'old progress')

if __name__ == '__main__':
    unittest.main()
