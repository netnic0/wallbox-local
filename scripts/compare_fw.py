"""Compare a freshly-built fw.zip with an optional reference zip.

Usage:
    python3 scripts/compare_fw.py [<built.zip>] [<reference.zip>]

Defaults:
    built.zip = build/fw.zip
    reference = none (skip reference comparison)

The script dumps the contents of each Mongoose OS firmware bundle
(fw.zip) and prints the manifest's name / version / platform / build_id
side by side. Used to verify the L1 Definition of Done.
"""
import json
import os
import sys
import zipfile


def show(label: str, path: str) -> None:
    print(f"--- {label} ---")
    if not os.path.exists(path):
        print(f"  NOT FOUND: {path}")
        print()
        return
    total = os.path.getsize(path)
    with zipfile.ZipFile(path) as z:
        for info in z.infolist():
            print(f"  {info.filename:50s} {info.file_size:>10d} bytes")
        manifest_name = next(
            (n for n in z.namelist() if n.endswith("manifest.json")), None
        )
        if manifest_name is not None:
            with z.open(manifest_name) as f:
                m = json.load(f)
            print(f"  manifest.name      = {m.get('name')}")
            print(f"  manifest.version   = {m.get('version')}")
            print(f"  manifest.platform  = {m.get('platform')}")
            print(f"  manifest.build_id  = {m.get('build_id')}")
        print(f"  total zip size     = {total} bytes")
    print()


def main() -> int:
    built = sys.argv[1] if len(sys.argv) > 1 else "build/fw.zip"
    reference = sys.argv[2] if len(sys.argv) > 2 else None

    show("Built firmware", built)
    if reference is not None:
        show("Reference firmware", reference)
    return 0


if __name__ == "__main__":
    sys.exit(main())
