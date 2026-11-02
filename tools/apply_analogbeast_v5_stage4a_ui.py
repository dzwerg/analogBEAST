#!/usr/bin/env python3
"""Safe Stage4a source overlay. NO MIDI, flash, reset, or firmware upload."""
from pathlib import Path
import argparse,hashlib,shutil
ROOT=Path(__file__).resolve().parents[1]
FW=ROOT/'firmware'/'src'
SRC=ROOT/'v5_stage4a'/'firmware'/'src'
OLD={'felucca.c': '9b041bb7e74bff94e91e5db719c50dca3f5ac970deea89c624fc0042e7ec6377', 'golana_ui.c': '03def5b9b63ad041f6a51d42f794744d67fed276ba1be1801d46247681210fbe'}
NEW={'felucca.c': 'a14f8456186e004ad1a6c1630a44fbcb57204e9c9c540f391991fcb719ca75a2', 'golana_ui.c': 'c300a529eda608049e22df535ba897b7cd7ee82b9fc8a03dc7761a218ce49c3f'}
CHANGED=('felucca.c','golana_ui.c')
ADDED=('analogbeast_v5_controls.c','analogbeast_v5_controls.h')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run():
    a=argparse.ArgumentParser(description=__doc__)
    a.add_argument('--check-only',action='store_true')
    args=a.parse_args()
    for name in CHANGED:
        target=FW/name
        if not target.exists() or sha(target) not in (OLD[name],NEW[name]):
            raise SystemExit('STOP: unbekannter Stage3b-Stand: '+name)
        if sha(SRC/name)!=NEW[name]:
            raise SystemExit('STOP: Paketinhalt veraendert: '+name)
        bak=target.with_name(name+'.before-v5-stage4a')
        if bak.exists() and sha(bak)!=OLD[name]:
            raise SystemExit('STOP: Backup unerwartet anders: '+name)
    for name in ADDED:
        target=FW/name
        if target.exists() and target.read_bytes()!=(SRC/name).read_bytes():
            raise SystemExit('STOP: geaenderte vorhandene Datei: '+name)
    if args.check_only:
        print('PASS: Stage3b erkannt; Stage4a Quellen unveraendert; keine Aenderungen')
        return
    for name in CHANGED:
        target=FW/name
        if sha(target)==NEW[name]:
            print('BEREITS INSTALLIERT:',name);continue
        bak=target.with_name(name+'.before-v5-stage4a')
        if not bak.exists():shutil.copy2(target,bak)
        shutil.copy2(SRC/name,target)
        print('PATCH:',name,'Backup:',bak.name)
    for name in ADDED:
        shutil.copy2(SRC/name,FW/name)
        print('ADDED:',name)
    print('PASS: V5 UI RAM ONLY; V4 PLAY/REC + NOR unchanged; NICHT FLASHEN')
if __name__=='__main__':run()
