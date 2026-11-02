analogBEAST ab3.2 - Songmode Fix (Windows CRLF compatibility)

This fixes the incorrect raw SHA256 matching in ab3.1. Windows Python
writes CRLF line endings, so an otherwise identical ab2.2 source appears
unrecognized. ab3.2 validates the normalized contents, but preserves the
existing line-ending convention. It will still STOP on actual source changes.

PowerShell:
cd C:\FM1\analogBEAST
Expand-Archive "$env:USERPROFILE\Downloads\analogBEAST-ab3.2-Windows-Fix.zip" -DestinationPath .\tools -Force
py tools\apply_analogbeast_ab32_song.py --check
py tools\apply_analogbeast_ab32_song.py --apply
py tools\apply_analogbeast_ab32_song.py --verify

Run host tests after a successful install:
wsl bash -lc 'cd /mnt/c/FM1/analogBEAST && mkdir -p build/host && cc -O1 -o build/host/analogbeast_song_ab3 tests/analogbeast_song_ab3.c && build/host/analogbeast_song_ab3'
wsl bash -lc 'cd /mnt/c/FM1/analogBEAST && cc -O1 -w -o build/host/analogbeast_ab3_storage tests/analogbeast_ab3_storage_mock.c && build/host/analogbeast_ab3_storage'
wsl bash -lc 'cd /mnt/c/FM1/analogBEAST && cc -O1 -w -DGOLANA=1 -Ibuild/gen -Ifirmware/src -o build/host/analogbeast_stage5_ui tests/dz_stage5_ui.c -lm && build/host/analogbeast_stage5_ui'
wsl bash -lc 'cd /mnt/c/FM1/analogBEAST && cc -O1 -w -DGOLANA=1 -Ibuild/gen -Ifirmware/src -o build/host/analogbeast_stage6 tests/dz_stage6_persist.c -lm && build/host/analogbeast_stage6'

Build (only after passing all tests):
wsl bash -lc 'cd /mnt/c/FM1/analogBEAST && PYTHON=~/minibrute-venv/bin/python ./build.sh --release 0.1.0-ab32'

NO FLASH until host tests and hardware storage migration strategy are confirmed.
128 song rows; TEMPO, PRESET and REST control entries; END preserved.
Navigation SELECT/SEQ and BRUTE dev5 should remain unchanged.
