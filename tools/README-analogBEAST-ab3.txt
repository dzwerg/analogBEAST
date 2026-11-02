analogBEAST ab3 SONG (128 rows; END / TEMPO / PRESET / REST)

BEFORE: Firmware ab2.2 navigation fix source. Other version will be REFUSED.
Original BRUTE dev5 checked byte-for-byte. SELECT, SEQ, ALGO unaffected.

POWERSHELL:
cd C:\FM1\analogBEAST
Expand-Archive "$env:USERPROFILE\Downloads\analogBEAST-ab3-Songmode.zip" -DestinationPath .\tools -Force
py tools\apply_analogbeast_ab3_song.py --check
py tools\apply_analogbeast_ab3_song.py --apply
py tools\apply_analogbeast_ab3_song.py --verify

TESTS (PowerShell):
wsl bash -lc 'cd /mnt/c/FM1/analogBEAST && mkdir -p build/host && cc -O1 -Wall -Wextra -Werror -o build/host/analogbeast_song_ab3 tests/analogbeast_song_ab3.c && build/host/analogbeast_song_ab3'
wsl bash -lc 'cd /mnt/c/FM1/analogBEAST && cc -O1 -Wall -Wextra -Wno-misleading-indentation -o build/host/analogbeast_ab3_storage tests/analogbeast_ab3_storage_mock.c && build/host/analogbeast_ab3_storage'
wsl bash -lc 'cd /mnt/c/FM1/analogBEAST && cc -O1 -w -DGOLANA=1 -Ibuild/gen -Ifirmware/src -o build/host/analogbeast_stage5_ui tests/dz_stage5_ui.c -lm && build/host/analogbeast_stage5_ui'
wsl bash -lc 'cd /mnt/c/FM1/analogBEAST && cc -O1 -w -DGOLANA=1 -Ibuild/gen -Ifirmware/src -o build/host/analogbeast_stage6 tests/dz_stage6_persist.c -lm && build/host/analogbeast_stage6'

BUILD AFTER ALL PASS:
wsl bash -lc 'cd /mnt/c/FM1/analogBEAST && PYTHON=~/minibrute-venv/bin/python ./build.sh --release 0.1.0-ab3'

DO NOT FLASH UNTIL BUILD/TESTS PASS.
New page: 3632 bytes, 208 free bytes per 3840-byte flash payload. Old page:3312.
The storage format upgrade to v2 occurs after reboot from an old committed v1 pair.
Do not downgrade firmware after writing in new format without backing up songs.
