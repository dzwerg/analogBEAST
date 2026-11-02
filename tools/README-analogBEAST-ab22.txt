analogBEAST 0.1.0-ab2.2 - SEQ / SELECT / ARP navigation repair
=================================================================
Input: existing analogBEAST ab2 (NOT ab2.1) sources.

Changes:
- SEQ cycles ARP -> TIMING -> STEPS -> BANK/PATTERN -> SONG -> ARP.
- SELECT code is preserved exactly: global next/previous visible menu navigator.
- Physical ARP button remains a toggle; ALGO on HOME remains BPM control.
- REC / WRITE / SONG / preset / audio / storage not changed.
- dev5 BRUTE DSP is checked byte-identically; no DSP change.
- Stage5 regression tests include SELECT and SEQ cycle, ARP toggle.

PowerShell from C:\FM1\analogBEAST:
Expand-Archive "$env:USERPROFILE\Downloads\analogBEAST-ab2.2-Navigation-Repair.zip" -DestinationPath .\tools -Force
py tools\apply_analogbeast_ab22_navigation.py --check
py tools\apply_analogbeast_ab22_navigation.py --apply

Host test (WSL):
wsl bash -lc 'cd /mnt/c/FM1/analogBEAST && cc -O1 -w -DGOLANA=1 -Ibuild/gen -Ifirmware/src -o build/host/analogbeast_stage5_ui tests/dz_stage5_ui.c -lm && build/host/analogbeast_stage5_ui'

Firmware build AFTER passing host test (WSL):
wsl bash -lc 'cd /mnt/c/FM1/analogBEAST && PYTHON=~/minibrute-venv/bin/python ./build.sh --release 0.1.0-ab22'

To undo code changes (not firmware already flashed):
py tools\apply_analogbeast_ab22_navigation.py --restore

Do not use this package on sources with the ab2.1 patch installed.
