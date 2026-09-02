#!/bin/sh
set -e
apk --version >/dev/null 2>&1 || true
mos build --local --platform esp8266
