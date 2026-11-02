analogBEAST ab2 - ALGO / MENU UI
===============================
Basis: analogBEAST 0.1.0-ab1, BRUTE-DSP exakt dev5.

Änderungen:
- ALGO: nur auf HOME sofort TEMPO (BPM) ändern; keine Navigation mehr.
- ALGO: zentrale Belegungstabelle GU_ALGO_BINDINGS für spätere Menüs.
- ALGO in WRITE: ohne Wirkung, kein ungewollter Abbruch.
- SELECT: navigiert durch alle sichtbaren Seiten, einschließlich BANK/PATTERN und SONG.
- SEQ: öffnet ARP wie zuvor.
- MIKROKORG: BRUTE FACTOR Seite nicht anzeigen.
- BRUTE: wirkungslose ASSIGN, DETUNE und SPREAD Regler ausblenden.
- Gemeinsame DSP-Parameter (LFO, Filter-EG, MOD, FX) bleiben sichtbar,
  weil der BRUTE-dev5-Signalweg sie weiterverwendet.
- Test tests/dz_stage5_ui.c angepasst, Sicherheitskopien erzeugt.
- KEINE Änderung an eng_golana.c / BRUTE DSP / Speicherlayout / Sequencer.

WICHTIG: Dies ist ein UI- und Navigationspaket fuer die beiden bereits
funktionierenden Engines. DONNER B1 und BEAST werden NICHT vorzeitig im
SYNTH MODE freigeschaltet; eigene DSP-Signalwege folgen als getrennte Patches.

INSTALLATION (PowerShell):
  cd C:\FM1\analogBEAST
  Expand-Archive "$env:USERPROFILE\Downloads\analogBEAST-ab2-ALGO-UI.zip" -DestinationPath .\tools -Force
  py tools\apply_analogbeast_ab2_ui.py --check
  py tools\apply_analogbeast_ab2_ui.py --apply
  py tools\verify_analogbeast_ab2.py

HOST-TEST + BUILD (PowerShell, WSL Toolchain erforderlich):
  wsl bash -lc 'cd /mnt/c/FM1/analogBEAST && mkdir -p build/host && cc -O1 -w -DGOLANA=1 -Ibuild/gen -Ifirmware/src -o build/host/analogbeast_stage5_ui tests/dz_stage5_ui.c -lm && build/host/analogbeast_stage5_ui'
  wsl bash -lc 'cd /mnt/c/FM1/analogBEAST && PYTHON=~/minibrute-venv/bin/python ./build.sh --release 0.1.0-ab2'

Keine Firmware flashen, bevor Hosttest und Build geprueft sind.

WIEDERHERSTELLUNG falls notwendig:
  firmware/src/golana_ui.c.before-analogbeast-ab2
  tests/dz_stage5_ui.c.before-analogbeast-ab2
