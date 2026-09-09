"""Offline prerequisite regressions; these fixtures are not physical-device data."""
import unittest

from preflight import assess, read_properties


class Prerequisites(unittest.TestCase):
    def properties(self, abis="arm64-v8a,armeabi-v7a", abis64="arm64-v8a", sdk="30"):
        return read_properties(
            "[ro.product.manufacturer]: [NVIDIA]\n"
            "[ro.product.device]: [mdarcy]\n"
            "[ro.product.model]: [SHIELD Android TV]\n"
            f"[ro.product.cpu.abilist]: [{abis}]\n"
            f"[ro.product.cpu.abilist64]: [{abis64}]\n"
            f"[ro.build.version.sdk]: [{sdk}]\n"
            "[ro.serialno]: [must-not-appear-in-report]\n"
        )

    def test_valid_metadata_does_not_claim_support(self):
        for page in (4096, 16384):
            report = assess(self.properties(), page)
            self.assertTrue(report["metadata_gate_passed"])
            self.assertFalse(report["android_game_supported"])
            self.assertTrue(report["requires_device_validation"])
            self.assertNotIn("ro.serialno", report["properties"])

    def test_arm_capable_cpu_does_not_override_32_bit_android(self):
        values = self.properties("armeabi-v7a,armeabi", "")
        values["kernel_arch"] = "aarch64"
        report = assess(values, 4096)
        self.assertFalse(report["metadata_gate_passed"])
        self.assertEqual(len(report["blockers"]), 2)

    def test_inconsistent_abi_lists_fail(self):
        for abis, abis64 in (("arm64-v8a", ""), ("armeabi-v7a", "arm64-v8a"),
                            ("x86_64", "x86_64")):
            self.assertFalse(assess(self.properties(abis, abis64), 4096)["metadata_gate_passed"])

    def test_missing_or_malformed_api_and_page_size_fail(self):
        for sdk in ("", "29", "Android 11", "30.0"):
            self.assertFalse(assess(self.properties(sdk=sdk), 4096)["metadata_gate_passed"])
        for page in (None, 0, 8192, 65536):
            self.assertFalse(assess(self.properties(), page)["metadata_gate_passed"])

    def test_empty_adb_error_is_not_valid_metadata(self):
        report = assess(read_properties("adb: no devices/emulators found"), 4096)
        self.assertFalse(report["metadata_gate_passed"])

    def test_duplicate_relevant_property_rejected(self):
        with self.assertRaises(ValueError):
            read_properties("[ro.build.version.sdk]: [30]\n[ro.build.version.sdk]: [35]")

    def test_unselected_or_unidentified_device_fails(self):
        for key, value in (("ro.product.device", "unknown"),
                           ("ro.product.device", ""),
                           ("ro.product.manufacturer", "another vendor")):
            values = self.properties()
            values[key] = value
            self.assertFalse(assess(values, 4096)["metadata_gate_passed"])


if __name__ == "__main__":
    unittest.main()
