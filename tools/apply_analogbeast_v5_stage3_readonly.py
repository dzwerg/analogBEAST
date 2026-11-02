#!/usr/bin/env python3
"""Stage 3 read-only integration. SOURCE PATCH ONLY; NEVER sends MIDI or flashes."""
from pathlib import Path
import hashlib,shutil,sys
ROOT=Path(__file__).resolve().parents[1]
FW=ROOT/'firmware'/'src'
SHAS={'felucca.c': 'f1a8c663f73c48368d7c4b069c8cef6bf0f9dd3aa6bc42299e518521d2fd5925', 'golana_ui.c': '5eba8104e6ea66a511754251d7ab8ac72ba0ea88b0ee2fd4076525a25e93d4d1', 'golana_dz_midi.c': 'fd7b081bc04b5c41d4bd08d6da2aad6e3a23618d3d484ce88c9ac718350ada3b'}
NEW=('analogbeast_v5_runtime.h','analogbeast_v5_runtime.c')
CHANGED=('felucca.c','golana_ui.c','golana_dz_midi.c')
STAGE=ROOT/'v5_stage3_readonly'

def main():
    import argparse
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--check-only',action='store_true')
    a=p.parse_args()
    for name in CHANGED:
        target=FW/name
        if not target.is_file():raise SystemExit('Fehlende Datei: '+str(target))
        expect=SHAS[name];actual=hashlib.sha256(target.read_bytes()).hexdigest()
        updated=hashlib.sha256((STAGE/'firmware'/'src'/name).read_bytes()).hexdigest()
        if actual not in (expect,updated):
            raise SystemExit('ABBRUCH: Source wurde inzwischen geaendert: '+name+' / '+actual)
    for name in NEW:
        dest=FW/name;src=STAGE/'firmware'/'src'/name
        if dest.exists() and dest.read_bytes()!=src.read_bytes():
            raise SystemExit('ABBRUCH: v5-Datei abweichend: '+name)
    if a.check_only:
        print('PASS: Quellstand kompatibel, keine Dateien veraendert')
        return
    for name in CHANGED:
        target=FW/name;source=STAGE/'firmware'/'src'/name
        if target.read_bytes()==source.read_bytes():continue
        backup=target.with_name(name+'.before-v5-stage3-readonly')
        if backup.exists() and backup.read_bytes()!=target.read_bytes():
            raise SystemExit('ABBRUCH: Backup schon vorhanden und abweichend: '+str(backup))
        shutil.copy2(target,backup)
        shutil.copy2(source,target)
        print('PATCH',name,'(original gesichert)')
    for name in NEW:
        shutil.copy2(STAGE/'firmware'/'src'/name,FW/name)
    print('PASS: v5 Stage3 READ-ONLY eingebaut; v4 DSP/NOR unveraendert; NICHT FLASHEN')
if __name__=='__main__': main()
