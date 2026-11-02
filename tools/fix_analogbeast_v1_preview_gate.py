#!/usr/bin/env python3
"""Safe web-only patch for analogBEAST v1.0 graphical editor Browser-Vorhoeren gate.

  py tools\fix_analogbeast_v1_preview_gate.py --check-only
  py tools\fix_analogbeast_v1_preview_gate.py
  py tools\fix_analogbeast_v1_preview_gate.py --restore
"""
import argparse, hashlib, shutil, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parent.parent
DEST=ROOT/'web'/'analogbeast-seq.js'
PAYLOAD=ROOT/'tools'/'analogbeast_seq_gate_payload.js'
BACKUP=ROOT/'web'/'analogbeast-seq.js.before-gate-fix'
PREVIOUS='c79a2444c75272bc7463372182d0a1434087c6567fba8dee4011f6ad84c7250c'
TARGET='c540a73e3912923ed675b0be906560137da502f5f834cb5a67a5b374ea295dcc'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    mode=p.add_mutually_exclusive_group();mode.add_argument('--check-only',action='store_true');mode.add_argument('--restore',action='store_true')
    a=p.parse_args()
    try:
        if not DEST.is_file() or not PAYLOAD.is_file() or sha(PAYLOAD)!=TARGET:
            raise RuntimeError('Editor oder Patchdatei fehlt bzw. Patchhash falsch – nichts veraendert')
        current=sha(DEST)
        if a.restore:
            if current!=TARGET or not BACKUP.is_file() or sha(BACKUP)!=PREVIOUS:
                raise RuntimeError('Rueckbau nicht moeglich: Editor/Backup anders als erwartet')
            shutil.copy2(BACKUP,DEST);print('RESTORED: Browser-Vorhoeren vor Gate-Fix');return 0
        if current==TARGET:
            print('ALREADY: Browser-Gate-Fix bereits vorhanden');return 0
        if current!=PREVIOUS:
            raise RuntimeError('Editor wurde seit Graphical v2 veraendert; sicherer Abbruch statt Ueberschreiben')
        if BACKUP.is_file() and sha(BACKUP)!=PREVIOUS:
            raise RuntimeError('Vorhandenes Backup hat abweichenden Inhalt; Abbruch')
        if a.check_only:
            print('PASS: grafischer V2-Editor erkannt; Gate-Fix bereit; keine Datei veraendert');return 0
        if not BACKUP.exists():shutil.copy2(DEST,BACKUP)
        shutil.copy2(PAYLOAD,DEST)
        if sha(DEST)!=TARGET:
            shutil.copy2(BACKUP,DEST)
            raise RuntimeError('Hashpruefung nach Kopie fehlgeschlagen; Original wiederhergestellt')
        print('PATCHED: web\\analogbeast-seq.js (Browser-Vorhoeren NOTE-OFF / TIE)')
        print('BACKUP: web\\analogbeast-seq.js.before-gate-fix')
        print('PASS: nur Web-Audio Preview geaendert; kein Firmware/MIDI/NOR/Flash-Schreibzugriff')
        return 0
    except (OSError,RuntimeError) as e:
        print('REFUSED:',e,file=sys.stderr);return 2
if __name__=='__main__':sys.exit(main())
