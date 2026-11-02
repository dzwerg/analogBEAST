miniBRUTE Stage 3 - Steiner-Parker-inspired nonlinear digital 2-pole multimode filter
Requirements: Stage 1 SYNTH MODE and Stage 2 BRUTE FACTOR (dev3, 12/12 tests).
All 32 presets preserved. Only BRUTE path uses the new Steiner approximation.
MIKROKORG path retains its original filter and 4-voice behavior.

Run in WSL from /mnt/c/FM1/miniBRUTE:
python3 tools/apply_minibrute_steiner_stage3.py --check
python3 tools/apply_minibrute_steiner_stage3.py --apply
PYTHON=~/minibrute-venv/bin/python ./build.sh --release 0.1.0-dev4
mkdir -p build/host
cc -O1 -w -DGOLANA=1 -Ibuild/gen -Ifirmware/src -o build/host/minibrute_steiner tests/minibrute_steiner.c -lm
build/host/minibrute_steiner
build/host/minibrute_brute_factor
build/host/minibrute_synth_mode

The Stage 3 TYPE setting on FILTER is LP/BP/HP when SYNTH MODE=BRUTE.
The existing 5 filter choices stay assigned in MIKROKORG mode.
The additional Steiner-mode parameter is saved per program in the existing 192-byte record.
Steiner is a *digital-inspired approximation*, not a verified electrical-circuit clone.
Do not flash before firmware build, host tests, and CPU checks pass.
