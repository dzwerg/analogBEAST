#!/usr/bin/env python3
"""Stage 3b: Move ONLY ab5_model (13648 bytes) from .pool to default .bss.
Source-only: no device access, no flash and no firmware installation.
"""
from pathlib import Path
import hashlib, argparse, shutil
ROOT=Path(__file__).resolve().parents[1]
UI=ROOT/'firmware/src/golana_ui.c'
TEST=ROOT/'tests/test_analogbeast_v5_stage3_source.py'
ORIG_UI='713e42bcb0007b0be85c1c26fb395c15eb3e058ff31b9669ceec02a15fce9fbd'
NEW_UI='03def5b9b63ad041f6a51d42f794744d67fed276ba1be1801d46247681210fbe'
ORIG_TEST='471fd41e0eafa250faed57f347e331690ba7690753dca36541a4957ec7628376'
NEW_TEST='2c992f9279d0ba777b9c99cbc3972c62ab9e8df05e98f6549c48980189d8a822'
FROM_UI='static ab5_extra_t ab5_model __attribute__((section(".pool")));'
TO_UI='static ab5_extra_t ab5_model; /* large v5 model in regular .bss; preserve POOL safety reserve */'
FROM_TEST='assert re.search(r\'static ab5_extra_t ab5_model __attribute__\\(\\(section\\(".pool"\\)\\)\\)\',gu)'

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--check-only',action='store_true')
    args=ap.parse_args()
    for p,old,new in ((UI,ORIG_UI,NEW_UI),(TEST,ORIG_TEST,NEW_TEST)):
        if not p.exists():raise SystemExit('STOP: fehlt: '+str(p))
        if sha(p) not in (old,new):raise SystemExit('STOP: unbekannter Quellstand '+str(p)+' SHA256='+sha(p))
    if args.check_only:
        print('PASS: bekannter Stage3-Quellstand, keine Aenderung')
        return
    # Build replacements and check expected hashes BEFORE changing any original.
    ui=UI.read_bytes().replace(FROM_UI.encode(),TO_UI.encode())
    t=TEST.read_bytes().replace(FROM_TEST.encode(),'assert re.search(r\'static ab5_extra_t ab5_model\\s*;\', gu)\nassert not re.search(r\'static ab5_extra_t ab5_model[^;]*section\\(".pool"\\)\',gu)'.encode())
    t=t.replace(b'PASS: v5 pool model',b'PASS: v5 .bss model')
    if hashlib.sha256(ui).hexdigest()!=NEW_UI or hashlib.sha256(t).hexdigest()!=NEW_TEST:
        raise SystemExit('STOP: Patch berechnet unerwartete Daten')
    for p,result,orig in ((UI,ui,ORIG_UI),(TEST,t,ORIG_TEST)):
        if sha(p)==(NEW_UI if p==UI else NEW_TEST):
            print('BEREITS KORREKT:',p.name)
            continue
        bak=p.with_name(p.name+'.before-v5-stage3b-ram-fix')
        if bak.exists() and sha(bak)!=orig:raise SystemExit('STOP: abweichende Sicherung '+str(bak))
        if not bak.exists():shutil.copy2(p,bak)
        p.write_bytes(result)
        print('PATCH:',p,'| Sicherung:',bak.name)
    print('PASS: v5-Modell jetzt in .bss; NOR/BRUTE/Pattern/Song-Code unveraendert; NICHT FLASHEN')
if __name__=='__main__':main()
