"""Native overrides should only rebuild affected callers and source chunks."""
from pathlib import Path
import sys
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "third_party/xboxrecomp"))
from tools.recomp.lifter import Lifter
from tools.recomp.translator import BatchTranslator

class IncrementalLift(unittest.TestCase):
    def test_manual_override_keeps_header_and_other_chunks_identical(self):
        class Translator:
            owned_function_starts = set()
            lifter = Lifter()
            def translate_function(self, address, info):
                return f"void {info['name']}(void) {{}}"
        batch = BatchTranslator.__new__(BatchTranslator)
        batch.translator = Translator()
        functions = [(0x10000 + i*16, {"name": f"sub_{0x10000+i*16:08X}"}) for i in range(6)]
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary)
            batch.translate_batch_split(functions, path, chunk_size=2)
            before = {p.name:p.read_bytes() for p in path.iterdir()}
            # Remove every body in the middle chunk, testing empty-chunk handling.
            batch.translate_batch_split(functions, path, chunk_size=2, manual={0x10020,0x10030})
            for name in ('recomp_funcs.h', 'recomp_0000.c', 'recomp_0002.c'):
                self.assertEqual(before[name], (path/name).read_bytes(), name)
            self.assertNotIn('void sub_00010020', (path/'recomp_0001.c').read_text())
            self.assertIn('sub_00010020', (path/'recomp_dispatch.c').read_text())

if __name__ == '__main__':
    unittest.main()
