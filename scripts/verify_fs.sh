#!/bin/sh
# Verify SPIFFS contents in build/fw.zip for Wallbox-Shelly1PM
# Usage (WSL):
#   R=/mnt/c/Users/I058304/HomeAssistant/shelly-ocpp-wallbox
#   bash "$R/scripts/verify_fs.sh"

set -e

R=${R:-/mnt/c/Users/I058304/HomeAssistant/shelly-ocpp-wallbox}
FW_ZIP="$R/build/fw.zip"
TMP_DIR="/tmp/fwcheck"

if [ ! -f "$FW_ZIP" ]; then
  echo "ERROR: $FW_ZIP not found. Build the firmware first." >&2
  exit 2
fi

rm -rf "$TMP_DIR" && mkdir -p "$TMP_DIR"

# Extract the zip. Prefer unzip; fall back to python3 (unzip is not always
# installed in a minimal WSL/CI environment).
if command -v unzip >/dev/null 2>&1; then
  unzip -o "$FW_ZIP" -d "$TMP_DIR" >/dev/null
elif command -v python3 >/dev/null 2>&1; then
  python3 -c "import zipfile,sys; zipfile.ZipFile(sys.argv[1]).extractall(sys.argv[2])" \
    "$FW_ZIP" "$TMP_DIR"
else
  echo "ERROR: need 'unzip' or 'python3' to inspect $FW_ZIP" >&2
  exit 5
fi

BIN=$(find "$TMP_DIR" -name fs.bin | head -n1)
if [ -z "$BIN" ] || [ ! -f "$BIN" ]; then
  echo "ERROR: fs.bin not found in $FW_ZIP" >&2
  exit 3
fi

echo "fs.bin: $BIN"

MISSING=0

# Search a literal marker inside the binary. Uses `strings` when available,
# otherwise falls back to `grep -a` (treat binary as text) so the check works
# on minimal environments without binutils.
has_marker() {
  if command -v strings >/dev/null 2>&1; then
    strings "$BIN" | grep -q "$1"
  else
    grep -a -q "$1" "$BIN"
  fi
}

check() { # $1 = marker, $2 = human label
  printf -- "- Checking for %s...\n" "$2"
  if has_marker "$1"; then
    echo "  OK"
  else
    echo "  MISSING $2"
    MISSING=1
  fi
}

check "rpc_auth.htdigest" "rpc_auth.htdigest"
check "admin:wallbox:"    "Digest user (admin:wallbox:<md5>)"
check "index.html"        "index.html.gz"

if [ "$MISSING" -ne 0 ]; then
  echo "Verification FAILED"
  exit 4
fi

echo "Verification PASSED"
exit 0
