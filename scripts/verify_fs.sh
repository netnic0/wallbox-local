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
unzip -o "$FW_ZIP" -d "$TMP_DIR" >/dev/null

BIN=$(find "$TMP_DIR" -name fs.bin | head -n1)
if [ -z "$BIN" ] || [ ! -f "$BIN" ]; then
  echo "ERROR: fs.bin not found in $FW_ZIP" >&2
  exit 3
fi

echo "fs.bin: $BIN"

MISSING=0

echo "- Checking for rpc_auth.htdigest..."
if ! strings "$BIN" | grep -q "rpc_auth.htdigest"; then
  echo "  MISSING rpc_auth.htdigest"
  MISSING=1
else
  echo "  OK"
fi

echo "- Checking for Digest user (admin:wallbox:)..."
if ! strings "$BIN" | grep -q "admin:wallbox:"; then
  echo "  MISSING admin:wallbox:<md5>"
  MISSING=1
else
  echo "  OK"
fi

echo "- Checking for index.html.gz..."
if ! strings "$BIN" | grep -q "index.html"; then
  echo "  MISSING index.html.gz"
  MISSING=1
else
  echo "  OK"
fi

if [ "$MISSING" -ne 0 ]; then
  echo "Verification FAILED"
  exit 4
fi

echo "Verification PASSED"
exit 0
