#!/usr/bin/env bash
# Host-only analogBEAST 1.1 QA, run from repository root on WSL.
# Does not modify firmware sources, does not write device flash, does not push.
set -uo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
fail=0
run(){
  printf '\n== %s ==\n' "$1"
  shift
  if "$@"; then echo "PASS command"; else rc=$?; echo "FAIL command (exit $rc)"; fail=1; fi
}
run "1.1 SONG iterator" bash -c 'cc -std=c11 -O2 -Ifirmware/src tests/test_v11_runtime.c firmware/src/analogbeast_v5_extra.c firmware/src/analogbeast_v5_controls.c firmware/src/analogbeast_v5_runtime.c -o build/test_v11_runtime && build/test_v11_runtime'
run "Browser editor syntax" node --check web/analogbeast-seq.js
run "Firmware/UI simulator compile and behavioral checks" bash -c 'cc -D_POSIX_C_SOURCE=200809L -std=c11 -O1 -w -DGOLANA=1 -Ibuild/gen -Ifirmware/src -o build/test_v11_controls tests/test_v11_controls.c -lm && build/test_v11_controls'
run "Golana audio sound-test link and first factory program" bash -c 'mkdir -p build/v11-golana-sound && cc -O2 -w -DGOLANA=1 -Ibuild/gen -Ifirmware/src -o build/test_v11_sound tests/golana_sound.c -lm && build/test_v11_sound build/v11-golana-sound 0'
run "Git whitespace consistency" git diff --check
if [ "$fail" = 0 ]; then
  echo -e '\nALL 1.1 QA CHECKS PASSED (old Golana UI suite not included)'
else
  echo -e '\n1.1 QA CHECKS FAILED (see errors above)'
  exit 1
fi
