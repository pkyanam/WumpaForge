#!/usr/bin/env python3
"""Reproducible, local-only extraction and static recompilation stages."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
UPSTREAM = ROOT / "third_party/xboxrecomp"
REVISION = "051a128df5ec27ef14f1ceaaead11c5457321eef"
REPORTS = ROOT / "local/reports"
ASSETS = ROOT / "local/assets"
PYTHON = ROOT / ".venv/bin/python"


def run(module, arguments, log_name):
    REPORTS.mkdir(parents=True, exist_ok=True)
    command = [str(PYTHON), "-m", module, *map(str, arguments)]
    print(f"{module}: log at {REPORTS / log_name}", flush=True)
    with (REPORTS / log_name).open("w") as log:
        subprocess.run(command, cwd=UPSTREAM, stdout=log,
                       stderr=subprocess.STDOUT, check=True)


def disc_image(argument):
    if argument:
        return Path(argument).resolve(strict=True)
    images = list(ROOT.glob("*.iso"))
    if len(images) != 1:
        raise ValueError("Provide --iso when the workspace does not contain exactly one ISO")
    return images[0]


def prepare(iso_path, all_assets=False):
    # Reuse the pinned upstream reader; never mount or modify the disc image.
    sys.path.insert(0, str(UPSTREAM))
    from tools.xiso.xdvdfs import Xiso
    ASSETS.mkdir(parents=True, exist_ok=True)
    REPORTS.mkdir(parents=True, exist_ok=True)
    entries = []
    with Xiso(iso_path) as iso:
        for directory, entry in iso.walk():
            path = Path(directory) / entry.name
            destination = (ASSETS / path).resolve()
            if not destination.is_relative_to(ASSETS.resolve()):
                raise ValueError(f"Unsafe disc path: {path}")
            if iso.base + entry.sector * 2048 + entry.size > iso_path.stat().st_size:
                raise ValueError(f"Truncated disc extent: {path}")
            entries.append({"path": path.as_posix(), "size": entry.size,
                            "sector": entry.sector})
            if all_assets or path.as_posix().lower() == "default.xbe":
                if not destination.exists():
                    iso.extract(entry, str(destination))
                elif destination.stat().st_size != entry.size:
                    raise ValueError(f"Existing extracted file has wrong size: {destination}")
    (REPORTS / "disc-inventory.json").write_text(json.dumps(entries, indent=2) + "\n")
    fingerprint = REPORTS / "disc-sha256.json"
    if not fingerprint.exists():
        with iso_path.open("rb") as source:
            digest = hashlib.file_digest(source, "sha256").hexdigest()
        fingerprint.write_text(json.dumps({"filename": iso_path.name,
                                           "size": iso_path.stat().st_size,
                                           "sha256": digest}, indent=2) + "\n")
    run("tools.xbe_parser", [ASSETS / "default.xbe", "--json",
                            REPORTS / "default_analysis.json"], "xbe-analysis.log")
    print(f"Inventoried {len(entries)} files; extracted {'all assets' if all_assets else 'only the XBE'}.")


def analyze():
    xbe = ASSETS / "default.xbe"
    disasm = REPORTS / "disasm"
    run("tools.disasm", [xbe, "--analysis-json", REPORTS / "default_analysis.json",
                        "--text-only", "--extra-sections", "D3D,D3DX,XGRPH,DSOUND,XPP,DOLBY",
                        "-o", disasm], "disasm.log")
    run("tools.func_id", [xbe, "--functions", disasm / "functions.json",
                         "--strings", disasm / "strings.json",
                         "--xrefs", disasm / "xrefs.json",
                         "-o", REPORTS / "func_id"], "func_id.log")
    run("tools.abi_analysis", [xbe, "--disasm-dir", disasm,
                              "--func-id-dir", REPORTS / "func_id",
                              "--output-dir", REPORTS / "abi"], "abi.log")


def lift():
    # Keep unchanged generated files' timestamps, so adding a small SDK bridge
    # does not force every large game translation unit to recompile.
    generated = ROOT / "local/generated"
    previous = {path.name: (hashlib.sha256(path.read_bytes()).digest(),
                           path.stat().st_atime_ns, path.stat().st_mtime_ns)
                for path in generated.glob("*") if path.is_file()}
    run("tools.recomp", [ASSETS / "default.xbe", "--all", "--split", "100",
                        "--game-name", "Crash Bandicoot: The Wrath of Cortex",
                        "--disasm-dir", REPORTS / "disasm",
                        "--func-id-dir", REPORTS / "func_id",
                        "--abi-dir", REPORTS / "abi",
                        "--manual-functions", ROOT / "config/manual-functions.json",
                        "--output-dir", REPORTS / "recomp",
                        "--gen-dir", ROOT / "local/generated"], "recomp.log")
    for path in generated.glob("*"):
        old = previous.get(path.name)
        if old and path.is_file() and hashlib.sha256(path.read_bytes()).digest() == old[0]:
            os.utime(path, ns=(old[1], old[2]))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("stage", choices=["prepare", "assets", "analyze", "lift"])
    parser.add_argument("--iso")
    args = parser.parse_args()
    actual = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=UPSTREAM, text=True).strip()
    if actual != REVISION:
        raise ValueError(f"Expected xboxrecomp {REVISION}, found {actual}")
    if args.stage in ("prepare", "assets"):
        prepare(disc_image(args.iso), args.stage == "assets")
    elif args.stage == "analyze":
        analyze()
    else:
        lift()


if __name__ == "__main__":
    main()
