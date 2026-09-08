#!/usr/bin/env python3
"""Fetch the pinned source toolkit and apply the checked-in native port patches."""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
UPSTREAM = ROOT / "third_party/xboxrecomp"
REVISION = "051a128df5ec27ef14f1ceaaead11c5457321eef"
URL = "https://github.com/sp00nznet/xboxrecomp.git"


def git(*arguments, check=True):
    return subprocess.run(["git", *arguments], cwd=UPSTREAM, check=check,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)


def main():
    if not UPSTREAM.exists():
        UPSTREAM.mkdir(parents=True)
        git("init")
        git("remote", "add", "origin", URL)
        git("fetch", "--depth", "1", "origin", REVISION)
        git("checkout", "--detach", "FETCH_HEAD")
    actual = git("rev-parse", "HEAD").stdout.strip()
    if actual != REVISION:
        raise RuntimeError(f"Preserving existing checkout at {actual}; expected {REVISION}")
    for patch in sorted((ROOT / "patches").glob("xboxrecomp-*.patch")):
        if git("apply", "--reverse", "--check", str(patch), check=False).returncode == 0:
            print(f"Already applied: {patch.name}")
            continue
        result = git("apply", "--check", str(patch), check=False)
        if result.returncode:
            raise RuntimeError(f"Cannot apply {patch.name}; preserving local edits.\n{result.stderr}")
        git("apply", str(patch))
        print(f"Applied: {patch.name}")
    python = ROOT / ".venv/bin/python"
    if not python.exists():
        subprocess.run([sys.executable, "-m", "venv", str(ROOT / ".venv")], check=True)
    subprocess.run([str(python), "-m", "pip", "install", "-r", str(ROOT / "requirements.txt")], check=True)


if __name__ == "__main__":
    main()
