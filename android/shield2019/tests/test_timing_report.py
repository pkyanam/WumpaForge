#!/usr/bin/env python3
import importlib.util
import json
import subprocess
import sys
import tempfile
from pathlib import Path
import unittest

HERE = Path(__file__).resolve().parent
SCRIPT = HERE.parent / "tools" / "timing_report.py"
spec = importlib.util.spec_from_file_location("timing_report", SCRIPT)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class TimingReportTest(unittest.TestCase):
    def test_window_stats_are_labeled_and_audio_is_unknown(self):
        with tempfile.TemporaryDirectory() as directory:
            log = Path(directory) / "fixture.log"
            log.write_text(
                "[wrath profile] thread=main frame=60 fps=60 interval_ms=16.000 max_ms=20.000\n"
                "[wrath profile] thread=main frame=120 fps=30 interval_ms=33.000 max_ms=80.000\n"
                "[wrath present profile] thread=main driver_ms=1.0\n"
            )
            result = module.report(module.rows([log]))
            main = result["by_source"][str(log)]["by_thread"]["main"]
            self.assertEqual(main["profile_windows"], 2)
            self.assertEqual(main["represented_present_calls"], 120)
            self.assertEqual(main["window_interval_ms"]["median"], 24.5)
            self.assertEqual(main["largest_interval_within_window_ms"], 80.0)
            self.assertIsNone(main["per_frame_interval_stats"])
            self.assertIsNone(main["audio_drift_ms"])

    def test_cli_emits_json(self):
        with tempfile.TemporaryDirectory() as directory:
            log = Path(directory) / "fixture.log"
            log.write_text("[wrath profile] thread=main frame=60 fps=60 interval_ms=16.667 max_ms=17.000\n")
            output = subprocess.run([sys.executable, str(SCRIPT), str(log)], check=True, capture_output=True, text=True).stdout
            self.assertEqual(json.loads(output)["by_source"][str(log)]["by_thread"]["main"]["profile_windows"], 1)

    def test_different_runs_and_workers_are_not_pooled(self):
        with tempfile.TemporaryDirectory() as directory:
            first, second = (Path(directory) / name for name in ("o1.log", "o2.log"))
            first.write_text(
                "[wrath profile] thread=main frame=60 interval_ms=30 max_ms=45\n"
                "[wrath profile] thread=worker frame=120 interval_ms=500 max_ms=900\n"
            )
            second.write_text("[wrath profile] thread=main frame=60 interval_ms=20 max_ms=25\n")
            result = module.report(module.rows([first, second]))["by_source"]
            self.assertEqual(result[str(first)]["by_thread"]["main"]["window_interval_ms"]["median"], 30)
            self.assertEqual(result[str(second)]["by_thread"]["main"]["window_interval_ms"]["median"], 20)
            self.assertEqual(result[str(first)]["by_thread"]["worker"]["largest_interval_within_window_ms"], 900)


if __name__ == "__main__":
    unittest.main()
