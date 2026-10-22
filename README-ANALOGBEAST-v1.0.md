# analogBEAST v1.0 – release identity patch

This is a **branding-only patch on top of the successful v5 FINAL CANDIDATE**, not a new firmware feature. It changes USB product descriptor in normal application mode, startup splash, About page, editor INFO, console banner and MIDI utilities, and produces the named `.fwsc` artifact.

The signed/packaged OTA model identity stays **FM-1_901**, USB VID/PID unchanged, original upstream credits and source/license references remain intact. **Update-loader mode still identifies as `Felucca Update`** for backwards-compatible installation.

## Windows PowerShell
```powershell
cd C:\FM1\analogBEAST
Expand-Archive "$env:USERPROFILE\Downloads\analogBEAST-v1.0-RELEASE-IDENTITY.zip" -DestinationPath . -Force
py tools\apply_analogbeast_v1_release.py --check-only
py tools\apply_analogbeast_v1_release.py
py tests\test_analogbeast_v1_release.py
py tests\test_analogbeast_v5_final_source.py
```

Installer verifies exact hashes for 9 source files, refuses altered files and makes `*.before-ab-v1-identity` backups. No hardware action occurs. The patched `tools/build.py` maintains `build/felucca.fwsc` for existing tooling AND creates byte-identical `build/analogBEAST-v1.0.fwsc`.

## WSL: build
```bash
cd /mnt/c/FM1/analogBEAST
source ~/.venvs/analogbeast/bin/activate
set -o pipefail
./build.sh 2>&1 | tee build-analogBEAST-v1.0.log
echo "BUILD EXIT CODE: ${PIPESTATUS[0]}"
sha256sum build/felucca.fwsc build/analogBEAST-v1.0.fwsc
```

**Do NOT use `./build.sh --release 1.0`!** Its upstream semantics change `FM-1_901` to `FM-1_910`. Use normal `./build.sh`.

## Before first flash
1. Back up v4 through current `Felucca` ports while v4 runs, and inspect non-empty JSON.
2. Confirm output shows `BUILD EXIT CODE: 0`, headroom ≥8 KiB RAM and pool, and package `identity FM-1_901`.
3. Use the existing validated installer with the current old `Felucca` MIDI IN port to flash the new `analogBEAST-v1.0.fwsc` only after review. No commands here automatically flash.
4. On restart the device's USB product should identify as `analogBEAST`. Windows MIDI port suffix may differ from `3`/`4`. After reconnect, check detected ports instead of guessing.
5. Perform hardware tests for pattern, song, album, E-H edit, NOR persistence with a power cycle before calling it a final release.

Expected initial USB mode name: `analogBEAST` (while bootloader still `Felucca Update`). Note Windows may cache USB device labels; unplug/replug and refresh MIDI applications if you still see `Felucca`.
