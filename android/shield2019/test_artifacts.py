import importlib.util,struct,unittest
from pathlib import Path
spec=importlib.util.spec_from_file_location('artifact',Path(__file__).with_name('artifact_check.py'));artifact=importlib.util.module_from_spec(spec);spec.loader.exec_module(artifact)
def fixture():
    data=bytearray(256);data[:7]=b'\x7fELF\x02\x01\x01';struct.pack_into('<HH',data,16,3,183);struct.pack_into('<Q',data,32,64);struct.pack_into('<HH',data,54,56,1)
    struct.pack_into('<IIQQQQQQ',data,64,1,5,0,0,0,len(data),len(data),16384);return data
class ELFContract(unittest.TestCase):
    def test_valid_arm64(self):self.assertEqual(artifact.inspect_elf(fixture())['architecture'],'AArch64')
    def test_other_cpu(self):
        data=fixture();struct.pack_into('<H',data,18,62)
        with self.assertRaises(ValueError):artifact.inspect_elf(data)
    def test_alignment_and_writable_code(self):
        for offset,fmt,value in [(64+48,'Q',4096),(64+4,'I',7),(64+16,'Q',4),(64+32,'Q',1000)]:
            data=fixture();struct.pack_into('<'+fmt,data,offset,value)
            with self.subTest(offset=offset),self.assertRaises(ValueError):artifact.inspect_elf(data)
    def test_truncated(self):
        with self.assertRaises(ValueError):artifact.inspect_elf(fixture()[:90])
if __name__=='__main__':unittest.main()
