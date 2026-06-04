"""Compare the freshly-built fw.zip with the legacy fw (7).zip."""
import zipfile, json, os

def show(label, path):
    print(f"--- {label} ---")
    if not os.path.exists(path):
        print(f"  NOT FOUND: {path}")
        return
    total = os.path.getsize(path)
    with zipfile.ZipFile(path) as z:
        for info in z.infolist():
            print(f"  {info.filename:50s} {info.file_size:>10d} bytes")
        manifest_name = [n for n in z.namelist() if n.endswith("manifest.json")][0]
        with z.open(manifest_name) as f:
            m = json.load(f)
        print(f"  manifest.name      = {m.get('name')}")
        print(f"  manifest.version   = {m.get('version')}")
        print(f"  manifest.platform  = {m.get('platform')}")
        print(f"  manifest.build_id  = {m.get('build_id')}")
        print(f"  total zip size     = {total} bytes")
    print()


show("WALLBOX-LOCAL v1.0.0 (L1 final)",
     "build/fw.zip")
show("UPSTREAM 0.3.0 (fw (7).zip reference)",
     "/mnt/c/Users/I058304/Downloads/fw (7).zip")
