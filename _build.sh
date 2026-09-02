#!/bin/sh
set -e
set -x
git config --global --add safe.directory "*"
mos build --local --platform esp8266
