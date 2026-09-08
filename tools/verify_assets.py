#!/usr/bin/env python3
"""Read-only comparison of extracted assets with the supplied Xbox disc."""
import argparse
import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]


def digest_range(source, size):
    digest = hashlib.sha256()
    while size:
        data = source.read(min(size, 1024 * 1024))
        if not data:
            raise ValueError("Unexpected end of file")
        digest.update(data)
        size -= len(data)
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--iso", type=Path)
    args = parser.parse_args()
    images = [args.iso] if args.iso else list(ROOT.glob("*.iso"))
    if len(images) != 1:
        parser.error("Specify exactly one supplied image with --iso")
    sys.path.insert(0, str(ROOT / "third_party/xboxrecomp"))
    from tools.xiso.xdvdfs import Xiso
    assets = (ROOT / "local/assets").resolve()
    results = []
    with Xiso(images[0]) as iso, images[0].open("rb") as disc:
        entries = [(Path(directory) / entry.name, entry)
                   for directory, entry in iso.walk()]
        for name, entry in sorted(entries, key=lambda item: item[1].sector):
            target = (assets / name).resolve()
            if not target.is_relative_to(assets):
                raise ValueError("Disc entry escapes asset directory")
            item = {"path": name.as_posix(), "size": entry.size}
            if not target.is_file() or target.stat().st_size != entry.size:
                item["matches"] = False
            else:
                disc.seek(iso.base + entry.sector * 2048)
                item["disc_sha256"] = digest_range(disc, entry.size)
                with target.open("rb") as extracted:
                    item["extracted_sha256"] = digest_range(extracted, entry.size)
                item["matches"] = item["disc_sha256"] == item["extracted_sha256"]
            results.append(item)
    report = ROOT / "local/reports/asset-sha256-verification.json"
    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text(json.dumps(results, indent=2) + "\n")
    failed = [item["path"] for item in results if not item["matches"]]
    print(f"Verified {len(results)} files, {sum(item['size'] for item in results)} bytes; mismatches={len(failed)}")
    for name in failed:
        print("MISMATCH:", name)
    print("Report:", report)
    return bool(failed)


if __name__ == "__main__":
    raise SystemExit(main())
