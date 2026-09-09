"""CMake script checks; no compiler, NDK, configure/build tree or device required."""
from pathlib import Path
import shutil
import subprocess
import unittest


@unittest.skipUnless(shutil.which("cmake"), "CMake is not installed")
class CMakeContract(unittest.TestCase):
    def check(self, **overrides):
        values = {"CMAKE_SYSTEM_NAME": "Android", "ANDROID_ABI": "arm64-v8a",
                  "CMAKE_SIZEOF_VOID_P": "8", "CMAKE_SYSTEM_VERSION": "30"}
        values.update(overrides)
        command = ["cmake", *(f"-D{key}={value}" for key, value in values.items()),
                   "-P", str(Path(__file__).parent / "probe/target_contract.cmake")]
        return subprocess.run(command, text=True, capture_output=True, check=False)

    def test_android_arm64_metadata_passes_without_claiming_runtime_support(self):
        result = self.check()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("no runtime/device support is implied", result.stdout)

    def test_wrong_os_abi_pointer_width_and_api_rejected(self):
        for change in ({"CMAKE_SYSTEM_NAME": "Darwin"}, {"CMAKE_SYSTEM_NAME": ""},
                       {"ANDROID_ABI": "armeabi-v7a"}, {"ANDROID_ABI": ""},
                       {"CMAKE_SIZEOF_VOID_P": "4"}, {"CMAKE_SIZEOF_VOID_P": ""},
                       {"CMAKE_SYSTEM_VERSION": "29"}, {"CMAKE_SYSTEM_VERSION": ""},
                       {"CMAKE_SYSTEM_VERSION": "android-30"}):
            with self.subTest(change=change):
                result = self.check(**change)
                self.assertNotEqual(result.returncode, 0, result.stdout)

    def test_ndk_legacy_toolchain_effective_api(self):
        self.assertEqual(self.check(CMAKE_SYSTEM_VERSION="1",
                                    ANDROID_PLATFORM_LEVEL="30").returncode, 0)
        for api in ("29", "", "android-30"):
            self.assertNotEqual(self.check(CMAKE_SYSTEM_VERSION="30",
                                           ANDROID_PLATFORM_LEVEL=api).returncode, 0)


if __name__ == "__main__":
    unittest.main()
