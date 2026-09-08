"""Check that unsupported x86 executes a diagnostic instead of silently passing."""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "third_party/xboxrecomp"))
from tools.recomp.disasm import Instruction, Operand
from tools.recomp.lifter import Lifter


class LifterDiagnostics(unittest.TestCase):
    def test_privileged_and_unhandled_instructions_stop_native_execution(self):
        instructions = [Instruction(0x12340, 1, "hlt", "", "f4"),
                        Instruction(0x12343, 3, "mov", "dr2, eax", "0f23d0")]
        operands = []
        for name in ("dr2", "eax"):
            op = Operand("reg")
            op.reg = name
            op.size = 4
            operands.append(op)
        instructions[1].operands = operands
        for instruction in instructions:
            with self.subTest(instruction=instruction.mnemonic), tempfile.TemporaryDirectory() as directory:
                directory = Path(directory)
                code = "\n".join(Lifter().lift_instruction(instruction))
                source = directory / "check.c"
                source.write_text("#include <stdint.h>\n#include <stdlib.h>\n"
                                  "void recomp_unsupported_instruction(uint32_t va) {"
                                  f" exit(va == {instruction.address}u ? 42 : 43); }}\n"
                                  "int main(void) {\n" + code + "\nreturn 0; }\n")
                binary = directory / "check"
                subprocess.run(["clang", "-std=c11", "-Werror", str(source), "-o", str(binary)], check=True)
                self.assertEqual(subprocess.run([str(binary)]).returncode, 42)


if __name__ == "__main__":
    unittest.main()
