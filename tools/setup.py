#!/usr/bin/env python3
"""Build and package WumpaForge locally from your original USA Xbox ISO."""
import argparse
import hashlib
import os
from pathlib import Path
import platform
import shlex
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
PYTHON = ROOT / ".venv/bin/python"
USA_XBE_SHA256 = "e8d7cbf225d899eb88227c11d1f40434c34e27c1b23ed946fb2c3471168d2f4d"


def run(*command, dry_run=False):
    command = list(map(str, command))
    print("+ " + shlex.join(command), flush=True)
    if not dry_run:
        subprocess.run(command, cwd=ROOT, check=True)


def preflight():
    if sys.version_info < (3, 11):
        raise ValueError("Python 3.11+ is required; install Homebrew Python.")
    if platform.system() != "Darwin" or platform.machine() != "arm64":
        raise ValueError("Use native ARM64 Python on Apple Silicon macOS, without Rosetta.")
    if int(platform.mac_ver()[0].split(".")[0]) < 14:
        raise ValueError("macOS 14 or newer is required.")
    missing = [name for name in ("git", "cmake", "pkg-config", "brew", "xcrun")
               if not shutil.which(name)]
    if missing:
        raise ValueError("Missing tools: " + ", ".join(missing) + ". See README.md prerequisites.")
    prefix = subprocess.check_output(["brew", "--prefix"], text=True).strip()
    if prefix != "/opt/homebrew":
        raise ValueError("This build expects native Homebrew at /opt/homebrew.")
    subprocess.run(["xcrun", "--find", "clang"], check=True, stdout=subprocess.DEVNULL)
    # openssl@3 is keg-only; pass its discovery paths through to CMake/pkg-config.
    openssl = subprocess.check_output(["brew", "--prefix", "openssl@3"], text=True).strip()
    os.environ["PKG_CONFIG_PATH"] = os.pathsep.join(filter(None, (
        str(Path(openssl) / "lib/pkgconfig"), os.environ.get("PKG_CONFIG_PATH"))))
    subprocess.run(["pkg-config", "--print-errors", "--exists", "sdl2", "epoxy", "openssl"],
                   check=True)
    return openssl


def verify_original(iso_path):
    # The pinned reader supports both game partitions and full-disc images.
    # Verify bytes from the supplied disc before reusing any extracted files.
    sys.path.insert(0, str(ROOT / "third_party/xboxrecomp"))
    from tools.xiso.xdvdfs import Xiso

    with Xiso(iso_path) as disc:
        entry = disc.find("default.xbe")
        if entry is None or entry.is_dir:
            raise ValueError("The disc has no original default.xbe executable.")
        digest = hashlib.sha256(disc.read(entry)).hexdigest()
    if digest != USA_XBE_SHA256:
        raise ValueError(f"Unsupported game executable: SHA-256 {digest}. "
                         "Use the original USA Xbox revision described in README.md.")
    print("Verified original USA Xbox executable.", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("iso", type=Path, help="path to your original USA Xbox ISO (read-only)")
    parser.add_argument("--jobs", type=int, choices=(1, 2), default=2,
                        help="maximum compiler jobs (default: 2)")
    parser.add_argument("--dry-run", action="store_true",
                        help="check host prerequisites and print steps; do not download or build")
    args = parser.parse_args()
    iso = args.iso.expanduser().resolve(strict=True)
    if not iso.is_file():
        parser.error("ISO path must be a file")
    openssl = preflight()
    run(sys.executable, ROOT / "tools/bootstrap.py", dry_run=args.dry_run)
    if args.dry_run:
        print(f"Verify original default.xbe in {iso}: SHA-256 {USA_XBE_SHA256}")
    else:
        verify_original(iso)
    for stage in ("prepare", "assets"):
        run(PYTHON, ROOT / "tools/pipeline.py", stage, "--iso", iso, dry_run=args.dry_run)
    run(PYTHON, ROOT / "tools/verify_assets.py", "--iso", iso, dry_run=args.dry_run)
    for stage in ("analyze", "lift"):
        run(PYTHON, ROOT / "tools/pipeline.py", stage, dry_run=args.dry_run)
    run("cmake", "-S", ROOT, "-B", ROOT / "build/native",
        "-DCMAKE_BUILD_TYPE=RelWithDebInfo", "-DCMAKE_OSX_ARCHITECTURES=arm64",
        f"-DOPENSSL_ROOT_DIR={openssl}", dry_run=args.dry_run)
    run("cmake", "--build", ROOT / "build/native", "--parallel", args.jobs, dry_run=args.dry_run)
    if not args.dry_run:
        architectures = subprocess.check_output(
            ["xcrun", "lipo", "-archs", str(ROOT / "build/native/wrath_native")], text=True).split()
        if architectures != ["arm64"]:
            raise ValueError(f"Expected native arm64 executable, found {architectures}")
    else:
        print("Verify the compiled executable is arm64.")
    run(PYTHON, ROOT / "tools/package.py", dry_run=args.dry_run)
    if args.dry_run:
        print("Dry run complete; ISO contents and build have not been validated.")
    else:
        print("Setup complete. Keep this checkout and its local/assets directory in place.")
        print("Launch with: " + shlex.join(["open", str(ROOT / "build/Wrath Native.app")]))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"Setup stopped: {error}\nSee README.md prerequisites and local/reports/ diagnostics.",
              file=sys.stderr)
        raise SystemExit(1)
