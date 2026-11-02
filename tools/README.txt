miniBRUTE dev7: BRUTE FACTOR v3 (experimental hardware tuning)
===============================================================

WHY:
dev5 sounded fuller at factor 0..40 but whistled and dropped out around 50.
dev6 no longer dropped out, but sounded almost unchanged across 0..127.
dev7 retains the damped feedback, increases it only to approx. 0.5x max,
and additionally adds a *progressively mixed, softly saturated* prefilter
path that changes the waveform, rather than simply making it louder.

WHAT CHANGES:
Only firmware/src/eng_golana.c, the BF>0 BRUTE-only branch.
The existing feedback smoothing and Steiner LP/BP/HP stay.
MIKROKORG, BF0, sequencer, 32 presets, 192-byte NOR records stay.
Automatic backup before changing source:
  firmware/src/eng_golana.c.dev6-before-fb-v3.bak

INSTALL:
Windows PowerShell, inside C:\FM1\miniBRUTE:
  Expand-Archive "$env:USERPROFILE\Downloads\miniBRUTE-dev7-Feedback-v3.zip" -DestinationPath .\tools -Force

WSL:
  cd /mnt/c/FM1/miniBRUTE
  python3 tools/apply_minibrute_fb_v3.py --check &&
  python3 tools/apply_minibrute_fb_v3.py --apply &&
  PYTHON=~/minibrute-venv/bin/python ./build.sh --release 0.1.0-dev7

NEW HOST TEST (do not flash before testing):
  mkdir -p build/host
  cc -O1 -w -DGOLANA=1 -Ibuild/gen -Ifirmware/src \
     -o build/host/minibrute_feedback_v3 tests/minibrute_feedback_v3.c -lm
  build/host/minibrute_feedback_v3

EXISTING REGRESSION TESTS:
  for name in minibrute_synth_mode minibrute_brute_factor minibrute_steiner minibrute_osc minibrute_feedback_v2; do
    cc -O1 -w -DGOLANA=1 -Ibuild/gen -Ifirmware/src \
       -o "build/host/$name" "tests/$name.c" -lm || break
    "build/host/$name" || break
  done

CAUTION:
- Patch logic checked against uploaded dev5 and local dev6 preview only.
- Actual device compile, audio and CPU performance not yet verified.
- Before any hardware test, lower speaker/headphone volume.
- The v3 test calculates gain-normalized waveform change, sustained-note
  level, and maximum-feedback bounds. It is not proof of a good sound.
