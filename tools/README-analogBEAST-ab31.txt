analogBEAST ab3.1 - SONGMODE (supports the ACTUAL ab2.2 Navigation Repair)

Fixes ab3 version mismatch: shipped ab2.2-Navigation-Repair.zip installed
UI SHA256 12d54eec... whereas original ab3 expected a different, never-shipped ab2.2 variant.
No re-flash or re-installation of ab2.2 is required.

POWERSHELL:
cd C:\FM1\analogBEAST
Expand-Archive "$env:USERPROFILE\Downloads\analogBEAST-ab3.1-Songmode-Fix.zip" -DestinationPath .\tools -Force
py tools\apply_analogbeast_ab31_song.py --check
py tools\apply_analogbeast_ab31_song.py --apply
py tools\apply_analogbeast_ab31_song.py --verify

TESTS:
wsl bash -lc 'cd /mnt/c/FM1/analogBEAST && mkdir -p build/host && cc -O1 -o build/host/analogbeast_song_ab3 tests/analogbeast_song_ab3.c && build/host/analogbeast_song_ab3'
wsl bash -lc 'cd /mnt/c/FM1/analogBEAST && cc -O1 -w -o build/host/analogbeast_ab3_storage tests/analogbeast_ab3_storage_mock.c && build/host/analogbeast_ab3_storage'
wsl bash -lc 'cd /mnt/c/FM1/analogBEAST && cc -O1 -w -DGOLANA=1 -Ibuild/gen -Ifirmware/src -o build/host/analogbeast_stage5_ui tests/dz_stage5_ui.c -lm && build/host/analogbeast_stage5_ui'
wsl bash -lc 'cd /mnt/c/FM1/analogBEAST && cc -O1 -w -DGOLANA=1 -Ibuild/gen -Ifirmware/src -o build/host/analogbeast_stage6 tests/dz_stage6_persist.c -lm && build/host/analogbeast_stage6'

BUILD AFTER ALL TESTS PASS:
wsl bash -lc 'cd /mnt/c/FM1/analogBEAST && PYTHON=~/minibrute-venv/bin/python ./build.sh --release 0.1.0-ab31'

NO FLASH before all tests have passed and build output checked.
Migration V1->V2 storage: do not downgrade after saving to version 2 without backup.
BRUTE dev5 untouched. SELECT / SEQ from shipped ab2.2 preserved.
