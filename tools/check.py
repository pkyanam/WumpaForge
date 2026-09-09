#!/usr/bin/env python3
"""Run existing CPU-only source regressions sequentially, without launching the game."""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
SYNTHETIC = (
    'test_lifter.py', 'test_incremental_lift.py', 'test_flag_merge.py',
    'test_mixed_flag_paths.py', 'test_logical_shift_widths.py',
    'test_shift_rotate_widths.py', 'test_carry_rotates.py',
    'test_x87_rounding.py', 'test_x87_status_rounding.py',
    'test_x87_classification.py', 'test_audio_probe.py',
)
ORIGINAL = ('test_double_shifts.py', 'test_crt_division.py')
ORIGINAL_INPUTS = (
    'local/assets/default.xbe', 'local/reports/disasm/functions.json',
    'local/reports/disasm/labels.json',
    'local/reports/func_id/identified_functions.json',
    'local/reports/abi/abi_functions.json',
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--suite', choices=('synthetic', 'original', 'all'),
                        default='synthetic', help='default: synthetic; original needs completed local analysis')
    parser.add_argument('--list', action='store_true', help='print selected commands without running them')
    args = parser.parse_args()
    tests = (() if args.suite == 'original' else SYNTHETIC) + (() if args.suite == 'synthetic' else ORIGINAL)
    if args.list:
        for test in tests:
            print(shlex.join([sys.executable, str(ROOT / 'tools' / test)]))
        return 0
    if not (ROOT / 'third_party/xboxrecomp/tools/recomp/lifter.py').is_file() or not shutil.which('clang'):
        parser.error('pinned toolkit and Clang are required; complete the README setup first')
    if args.suite != 'synthetic':
        missing = [name for name in ORIGINAL_INPUTS if not (ROOT / name).is_file()]
        if missing:
            parser.error('original checks need setup/analysis inputs: ' + ', '.join(missing))
    stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ')
    output = ROOT / 'local/reports/checks' / stamp
    output.mkdir(parents=True)
    results = []
    for test in tests:
        log = output / (Path(test).stem + '.log')
        start = time.monotonic()
        print(f'Running {test} ...', flush=True)
        with log.open('w') as stream:
            run = subprocess.run([sys.executable, str(ROOT / 'tools' / test)], cwd=ROOT,
                                 stdout=stream, stderr=subprocess.STDOUT)
        result = {'test': test, 'returncode': run.returncode,
                  'seconds': round(time.monotonic() - start, 3), 'log': str(log)}
        results.append(result)
        print(f"{'PASS' if run.returncode == 0 else 'FAIL'} {test} ({result['seconds']}s)", flush=True)
        if run.returncode:
            print(f'Read failure details: {log}', file=sys.stderr)
            break
    report = {'suite': args.suite, 'selected': len(tests), 'results': results,
              'passed': len(results) == len(tests) and all(r['returncode'] == 0 for r in results)}
    (output / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
    print(f'Report: {output / "results.json"}')
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
