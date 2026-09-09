import hashlib,importlib.util,json,tempfile,unittest
from pathlib import Path
spec=importlib.util.spec_from_file_location('device',Path(__file__).with_name('device.py'))
device=importlib.util.module_from_spec(spec);spec.loader.exec_module(device)
class AssetImport(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name);self.assets=self.root/'assets';self.assets.mkdir()
        (self.assets/'test.bin').write_bytes(b'fixture')
        self.rows=[{'path':'test.bin','size':7,'disc_sha256':hashlib.sha256(b'fixture').hexdigest(),'matches':True}]
        self.report=self.root/'report.json'
    def tearDown(self):self.temp.cleanup()
    def check(self):
        self.report.write_text(json.dumps(self.rows))
        return device.verified_assets(self.assets,self.report,require_original=False)
    def test_verified_tree(self):self.assertEqual(len(self.check()),1)
    def test_changed_bytes(self):
        (self.assets/'test.bin').write_bytes(b'changed')
        with self.assertRaises(ValueError):self.check()
    def test_extra_file(self):
        (self.assets/'extra').write_bytes(b'')
        with self.assertRaises(ValueError):self.check()
    def test_unsafe_paths_and_unverified_reports(self):
        for change in [{'path':'../outside'},{'path':'/outside'},{'path':'a/../test.bin'},{'path':'a\nb'},{'matches':False},{'disc_sha256':'invalid'}]:
            original=dict(self.rows[0]);self.rows[0].update(change)
            with self.subTest(change=change),self.assertRaises(ValueError):self.check()
            self.rows[0]=original
    def test_duplicate(self):
        self.rows.append(dict(self.rows[0]))
        with self.assertRaises(ValueError):self.check()
    def test_symlink(self):
        (self.assets/'test.bin').unlink();(self.root/'outside').write_bytes(b'fixture');(self.assets/'test.bin').symlink_to(self.root/'outside')
        with self.assertRaises(ValueError):self.check()
    def test_original_disc_required(self):
        self.check()
        with self.assertRaises(ValueError):device.verified_assets(self.assets,self.report)
if __name__=='__main__':unittest.main()
