#!/usr/bin/env python3
"""Read saved Android metadata; never build, connect to a device, or claim a port.

Input is the output of `adb shell getprop`. Only the whitelisted properties below
are retained. Keep raw device reports in ignored local/reports, not source control.
"""
import argparse
import json
from pathlib import Path
import re

MIN_API = 30  # Initial probe baseline, including bionic's memfd_create wrapper.
TARGET = "NVIDIA SHIELD TV Pro (2019)"
KEYS = (
    "ro.product.manufacturer", "ro.product.model", "ro.product.device",
    "ro.product.cpu.abilist", "ro.product.cpu.abilist64",
    "ro.build.version.sdk", "ro.build.version.release",
)


def read_properties(text):
    values = {}
    for line in text.splitlines():
        match = re.fullmatch(r"\[([^\]]+)\]: \[(.*)\]", line.strip())
        if match and match[1] in KEYS:
            if match[1] in values:
                raise ValueError(f"Duplicate property: {match[1]}")
            values[match[1]] = match[2]
    return values


def assess(values, page_size):
    blockers = []
    if (values.get("ro.product.manufacturer", "").casefold() != "nvidia" or
            values.get("ro.product.device") != "mdarcy"):
        blockers.append("Metadata does not identify the selected NVIDIA mdarcy (SHIELD TV Pro 2019) target")
    for key in ("ro.product.cpu.abilist", "ro.product.cpu.abilist64"):
        abis = {part.strip() for part in values.get(key, "").split(",")}
        if "arm64-v8a" not in abis:
            blockers.append(f"{key} does not confirm arm64-v8a applications")
    sdk = values.get("ro.build.version.sdk", "")
    if not re.fullmatch(r"[0-9]+", sdk) or int(sdk) < MIN_API:
        blockers.append(f"Device API does not meet initial probe baseline {MIN_API}")
    if page_size not in (4096, 16384):
        blockers.append("Host page size must be captured as 4096 or 16384; other sizes need review")
    return {
        "schema_version": 1,
        "target_device": TARGET,
        "scope": "saved device metadata only; not a runtime or graphics probe",
        "properties": {key: values[key] for key in KEYS if key in values},
        "host_page_size": page_size,
        "planned_abi": "arm64-v8a",
        "planned_min_api": MIN_API,
        "metadata_gate_passed": not blockers,
        "blockers": blockers,
        "android_game_supported": False,
        "requires_device_validation": [
            "ARM64 executable and app sandbox execution",
            "4 GiB sparse reservation, shared aliases and cleanup",
            "EGL desktop OpenGL 4.1+ context, entry points and shader fixtures",
            "SDL audio, controllers and application lifecycle",
            "original game behavior and measured performance",
        ],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--properties", required=True, type=Path,
                        help="Saved adb shell getprop output (read only)")
    parser.add_argument("--page-size", required=True, type=int,
                        help="Value observed with adb shell getconf PAGE_SIZE")
    args = parser.parse_args()
    try:
        report = assess(read_properties(args.properties.read_text()), args.page_size)
    except (OSError, UnicodeError, ValueError) as error:
        parser.error(str(error))
    print(json.dumps(report, indent=2))
    return 0 if report["metadata_gate_passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
