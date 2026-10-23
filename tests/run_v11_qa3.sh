#!/usr/bin/env bash
# analogBEAST 1.1 QA3 host-only; called from repo root in WSL.
# No device flash, no firmware source change, no git push.
set -uo pipefail
cd "$(dirname "$0")/.." || exit 1
mkdir -p build
errors=0
run(){
  printf '\n== %s ==\n' "$1"
  shift
  if "$@"; then echo 'PASS suite'; else rc=$?; echo "FAIL suite ($rc)"; errors=1; fi
}

run '1.1 QA2 precondition: verified device simulator' bash -c 'test -f tests/test_v11_controls.c && test -f tests/test_v11_runtime.c && test -f tests/test_analogbeast_v5_nor.c && echo "PASS QA2 fixtures present"'

run 'QA3 SONG / ALBUM / NEXT real iterator boundaries and exact 13648-byte wire roundtrip' \
  bash -c 'cc -std=c11 -O2 -w -Ifirmware/src -o build/test_v11_boundaries tests/test_v11_boundaries.c firmware/src/analogbeast_v5_extra.c firmware/src/analogbeast_v5_runtime.c && build/test_v11_boundaries'

run 'QA3 simulated NOR A/B persistence, v1.1 opcodes and legacy-data protection' \
  bash -c 'cc -std=c11 -O2 -w -Ifirmware/src -o build/test_v11_nor tests/test_v11_nor.c firmware/src/analogbeast_v5_extra.c firmware/src/analogbeast_v5_nor.c firmware/src/analogbeast_snapshot.c && build/test_v11_nor'

run 'QA3 actual device callback / pending NEXT / note release / STOP cleanup' \
  bash -c 'cc -D_POSIX_C_SOURCE=200809L -std=c11 -O1 -w -DGOLANA=1 -Ibuild/gen -Ifirmware/src -o build/test_v11_device_boundaries tests/test_v11_device_boundaries.c -lm && build/test_v11_device_boundaries'

run 'Webeditor JavaScript syntax' node --check web/analogbeast-seq.js
run 'Git whitespace consistency' git diff --check

if [ "$errors" -ne 0 ]; then
  printf '\nQA3 NOT ALL PASSED. STOP: NO FLASH.\n'
  exit 1
fi
printf '\nALL QA3 TESTS PASSED: host verification only; old UI suite not included.\n'
