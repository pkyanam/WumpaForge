#!/usr/bin/env python3
"""Summarize Shield frame-profile windows without inventing per-frame data.

The runtime emits one ``[wrath profile]`` row after 60 presents.  ``interval_ms``
is the mean interval for that window and ``max_ms`` is its maximum interval;
neither field is a sample of every presented frame.
"""
import argparse
import json
import re
from pathlib import Path

PROFILE = re.compile(r"^\[wrath profile\] (?P<body>.*)$")
FIELD = re.compile(r"(?P<name>[a-zA-Z_]+)=(?P<value>[^ ]+)")


def rows(paths):
    found = []
    for path in paths:
        for line in Path(path).read_text(errors="replace").splitlines():
            match = PROFILE.match(line)
            if not match:
                continue
            values = {m.group("name"): m.group("value") for m in FIELD.finditer(match.group("body"))}
            if "interval_ms" in values and "max_ms" in values:
                values["source"] = str(path)
                values["interval_ms"] = float(values["interval_ms"])
                values["max_ms"] = float(values["max_ms"])
                values["frame"] = int(values.get("frame", "0"))
                found.append(values)
    return found


def percentile(values, fraction):
    if not values:
        return None
    ordered = sorted(values)
    if len(ordered) == 1:
        return ordered[0]
    position = fraction * (len(ordered) - 1)
    lower = int(position)
    upper = min(lower + 1, len(ordered) - 1)
    return ordered[lower] + (ordered[upper] - ordered[lower]) * (position - lower)


def _thread_report(found):
    intervals = [row["interval_ms"] for row in found]
    maxima = [row["max_ms"] for row in found]
    return {
        "profile_windows": len(found),
        "represented_present_calls": len(found) * 60,
        "window_interval_ms": {
            "median": percentile(intervals, 0.50),
            "p95": percentile(intervals, 0.95),
            "p99": percentile(intervals, 0.99),
            "maximum": max(intervals) if intervals else None,
        },
        "largest_interval_within_window_ms": max(maxima) if maxima else None,
        "windows_over_16_667_ms": sum(value > 16.667 for value in intervals),
        "windows_over_33_333_ms": sum(value > 33.333 for value in intervals),
        "per_frame_interval_stats": None,
        "audio_drift_ms": None,
    }


def report(found):
    grouped = {}
    for row in found:
        source = grouped.setdefault(row["source"], {})
        source.setdefault(row.get("thread", "unknown"), []).append(row)
    return {
        "by_source": {
            source: {"by_thread": {thread: _thread_report(values)
                                    for thread, values in sorted(threads.items())}}
            for source, threads in sorted(grouped.items())
        },
        "limitations": [
            "Each profile row summarizes 60 presents; interval_ms is a window mean.",
            "Present calls are not independently measured display frames; partial windows are absent.",
            "max_ms identifies a largest interval but does not identify loading phase.",
            "Input files are kept separate; select comparable scene windows before comparing runs.",
            "Threads are reported separately so loading-worker windows are not combined with game-thread windows.",
            "No paired audio production/consumption timestamps are present in these logs.",
        ],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("logs", nargs="+", type=Path)
    args = parser.parse_args()
    print(json.dumps(report(rows(args.logs)), indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
