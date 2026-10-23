#!/usr/bin/env bash
# Host only. Does not open MIDI ports or flash a real FM-1.
set -uo pipefail
cd "$(dirname "$0")/.." || exit 1
mkdir -p build
errors=0
run(){ printf '\n== %s ==\n' "$1";shift; if "$@";then echo 'PASS suite';return 0;else rc=$?;echo "FAIL suite ($rc)";errors=1;return "$rc";fi; }
run 'QA4 1.1 SONG+ALBUM SysEx 96 payload parser (C)' bash -c \
 'cc -std=c11 -O2 -w -Ifirmware/src -o build/test_v11_xfer tests/test_v11_xfer.c firmware/src/analogbeast_v5_extra.c && build/test_v11_xfer'
rm -f build/qa4_device_bridge
if run 'QA4 firmware device SysEx dispatcher compilation (native emulator)' bash -c \
 'cc -D_POSIX_C_SOURCE=200809L -std=c11 -O1 -w -DGOLANA=1 -Ibuild/gen -Ifirmware/src -o build/qa4_device_bridge tests/qa4_device_bridge.c -lm'; then
 run 'QA4 real Python transfer CLI <-> actual firmware simulator over virtual SysEx frames' \
   python3 tests/test_v11_cli_bridge.py .
fi
run 'QA4 browser editor syntax' node --check web/analogbeast-seq.js
run 'QA4 repository whitespace' git diff --check
if [ "$errors" != 0 ];then printf '\nQA4 NOT ALL PASSED: NO FLASH\n';exit 1;fi
printf '\nALL QA4 HOST TESTS PASSED. REAL USB-MIDI/HARDWARE STILL UNTESTED.\n'
