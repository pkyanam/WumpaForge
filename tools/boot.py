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
    parser.add_argument("--on-stop", action="append", default=[],
                        help="Additional LLDB inspection command at the diagnostic stop")
    parser.add_argument("--probe-story", action="store_true",
                        help="Read-only sparse original backstory boundary snapshots")
    parser.add_argument("--probe-intro", action="store_true",
                        help="Read-only matched intro and first draw snapshots")
    parser.add_argument("--probe-shader", action="store_true",
                        help="Export live shader state at a failure or bounded stop")
    parser.add_argument("--app", action="store_true",
                        help="Debug the packaged Mac app for Computer Use testing (package first)")
    args = parser.parse_args()
    if Path(args.name).name != args.name or not 1 <= args.seconds <= 600:
        parser.error("Use a plain log name and a duration from 1 to 600 seconds")
    reports = ROOT / "local/reports"
    reports.mkdir(parents=True, exist_ok=True)
    log = reports / f"{args.name}.log"
    command = ["lldb", "--batch", "-o", "breakpoint set -n bridge_HalReturnToFirmware"]
    shader_dump = []
    if args.probe_shader:
        command += ["-o", f"command script import {ROOT / 'tools/shader_probe.py'}",
                    "-o", "breakpoint set -n shader_error"]
        shader_dump = [f"script shader_probe.dump(lldb.debugger, {str(reports / (args.name + '-shader.json'))!r})"]
    if args.probe_story:
        command += ["-o", f"command script import {ROOT / 'tools/story_probe.py'}",
                    "-o", "script story_probe.install(lldb.debugger)"]
    if args.probe_intro:
        command += ["-o", f"command script import {ROOT / 'tools/intro_probe.py'}",
                    "-o", "script intro_probe.install(lldb.debugger)"]
    # Stop after the watchdog sleep, before it exits the process. A live loading
    # stall needs all native thread stacks; an exit alone loses that evidence.
    watchdog_source = ROOT / "third_party/xboxrecomp/src/kernel/xbox_memory_layout.c"
    for line_number, line in enumerate(watchdog_source.read_text().splitlines(), 1):
        if 'fprintf(stderr, "[WATCHDOG]' in line and not args.probe_intro:
            command += ["-o", f"breakpoint set -f xbox_memory_layout.c -l {line_number}"]
            break
    for symbol in args.break_at:
        command += ["-o", f"breakpoint set -n {symbol}"]
    # LLDB's -o commands stop after a crash; -k is required for that backtrace.
    command += ["-o", "run"]
    for flag in ("-o", "-k"):
        for action in [*args.on_stop, *shader_dump, "thread backtrace all", "quit"]:
            command += [flag, action]
    binary = ROOT / ("build/Wrath Native.app/Contents/MacOS/wrath_native"
                     if args.app else "build/native/wrath_native")
    if not binary.is_file():
        parser.error("Build the native game and run tools/package.py before using --app")
    command += ["--", str(binary), str(ROOT / "local/assets")]
    env = dict(os.environ, WRATH_BOOT_TIMEOUT=str(args.seconds + 5),
               RECOMP_WATCHDOG_SECS=str(args.seconds))
    with log.open("w") as output:
        result = subprocess.run(command, cwd=ROOT, env=env, stdout=output,
                                stderr=subprocess.STDOUT)
    print(f"Debugger exit {result.returncode}; inspect {log} for actual game state.")
    return result.returncode

if __name__ == "__main__":
    raise SystemExit(main())
