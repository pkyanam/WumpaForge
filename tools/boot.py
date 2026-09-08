#!/usr/bin/env python3
"""Bounded native debugging run; a zero exit is never proof of game success."""
import argparse
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("name", help="Diagnostic log name, e.g. boot-08")
    parser.add_argument("--seconds", type=int, default=10)
    parser.add_argument("--break-at", action="append", default=[])
    args = parser.parse_args()
    if Path(args.name).name != args.name or not 1 <= args.seconds <= 60:
        parser.error("Use a plain log name and a duration from 1 to 60 seconds")
    reports = ROOT / "local/reports"
    reports.mkdir(parents=True, exist_ok=True)
    log = reports / f"{args.name}.log"
    command = ["lldb", "--batch", "-o", "breakpoint set -n bridge_HalReturnToFirmware"]
    # Stop after the watchdog sleep, before it exits the process. A live loading
    # stall needs all native thread stacks; an exit alone loses that evidence.
    watchdog_source = ROOT / "third_party/xboxrecomp/src/kernel/xbox_memory_layout.c"
    for line_number, line in enumerate(watchdog_source.read_text().splitlines(), 1):
        if 'fprintf(stderr, "[WATCHDOG]' in line:
            command += ["-o", f"breakpoint set -f xbox_memory_layout.c -l {line_number}"]
            break
    for symbol in args.break_at:
        command += ["-o", f"breakpoint set -n {symbol}"]
    # LLDB's -o commands stop after a crash; -k is required for that backtrace.
    command += ["-o", "run", "-o", "thread backtrace all", "-o", "quit",
                "-k", "thread backtrace all", "-k", "quit", "--",
                str(ROOT / "build/native/wrath_native"), str(ROOT / "local/assets")]
    env = dict(os.environ, WRATH_BOOT_TIMEOUT=str(args.seconds + 5),
               RECOMP_WATCHDOG_SECS=str(args.seconds))
    with log.open("w") as output:
        result = subprocess.run(command, cwd=ROOT, env=env, stdout=output,
                                stderr=subprocess.STDOUT)
    print(f"Debugger exit {result.returncode}; inspect {log} for actual game state.")
    return result.returncode

if __name__ == "__main__":
    raise SystemExit(main())
