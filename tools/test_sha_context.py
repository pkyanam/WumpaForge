"""Check actual native Xbox SHA storage and standard vectors, without game assets."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class ShaContext(unittest.TestCase):
    def test_guest_layout_and_incremental_hash_vectors(self):
        with tempfile.TemporaryDirectory() as name:
            binary = Path(name) / 'sha_context'
            subprocess.run([
                'clang', '-std=c11', '-O2', '-fsanitize=undefined',
                '-ffunction-sections', '-fdata-sections',
                '-I'+str(ROOT/'third_party/xboxrecomp/src'),
                '-I'+str(ROOT/'third_party/xboxrecomp/include'),
                str(ROOT/'tools/tests/sha_context.c'),
                str(ROOT/'third_party/xboxrecomp/src/kernel/kernel_crypto.c'),
                '-Wl,-dead_strip', '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    unittest.main()
