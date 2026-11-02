#!/usr/bin/env python3
# analogBEAST Stage4: safety-checked overlay for one exact Stage3 baseline.
# SOURCE ONLY, no firmware flashing, no flash erase or MIDI writes.
from pathlib import Path
from datetime import datetime
import hashlib, shutil, sys
ROOT=Path(__file__).resolve().parent.parent
PAYLOAD=ROOT/'tools'/'analogbeast_v4_stage4_payload'
MANIFEST={'firmware/src/seq.c': {'before': 'a9ef43c24f41df44dff19a2ac5518fad5407fb2af3a85b7fccb56366dd3d13eb', 'after': 'a6b2c8e0abe8cc9a8bb1d1fa91e980f117590c23bdab21ad838c35c8869eeed8'}, 'firmware/src/golana_ui.c': {'before': 'ab0db7eadc0735dc84847f00f06837839995a992157fdee10aaa252f128fa13c', 'after': '4b7a139fc966b94615a87a1bda2e429df9dcd78cc2721a5f80cc4fea4366d26f'}, 'firmware/src/analogbeast_timing.h': {'before': None, 'after': '58fbc1a2b7c67cf456b16741edfc4ef3fd32512c68755493508ed7536c14b50d'}}
GUARDS={'firmware/src/usb.c': '4fc17a5267b80dd1191b7d091cad95468a3358da28bb0663c07a1d6b73c6b284', 'firmware/src/eng_golana.c': '565e2754451ed5af4f94272b1eb4eb76081628c42cbc212a93c5fd573a36cb93', 'firmware/src/golana_dz_midi.c': 'f0e68900ad39310a176c19891cb49c3579a99c70fc544abda0f76c774267b79f'}

def sha(path):
    return hashlib.sha256(path.read_bytes().replace(b'\r\n',b'\n')).hexdigest()

def run():
    bad=[]; write=[]
    for rel,dig in GUARDS.items():
        f=ROOT/rel
        if not f.is_file() or sha(f)!=dig:
            bad.append(f'guard mismatch: {rel}')
    for rel,dig in MANIFEST.items():
        f=ROOT/rel; source=PAYLOAD/rel
        if not source.is_file() or sha(source)!=dig['after']:
            bad.append(f'payload damaged: {rel}');continue
        if f.exists():
            hv=sha(f)
            if hv==dig['after']:continue
            if dig['before'] is None or hv!=dig['before']:
                bad.append(f'unknown local edits: {rel}');continue
        else:
            if dig['before'] is not None: bad.append(f'missing existing source: {rel}');continue
        write.append(rel)
    brute=ROOT/'firmware/src/eng_golana.c'
    reference=ROOT/'firmware/src/eng_golana.c.dev5-before-fb-fix.bak'
    if not reference.exists() or brute.read_bytes()!=reference.read_bytes():
        bad.append('BRUTE dev5 differs from verified reference')
    if bad:
        print('STOP – nothing changed:')
        for msg in bad: print(' -',msg)
        return 1
    if not write:
        print('PASS: Stage4 source already installed; BRUTE and USB unchanged')
        return 0
    backup=ROOT/('source-backup-before-v4-stage4-'+datetime.now().strftime('%Y%m%d-%H%M%S'))
    backup.mkdir(parents=True,exist_ok=False)
    try:
        for rel in write:
            f=ROOT/rel
            if f.exists():
                dst=backup/rel;dst.parent.mkdir(parents=True,exist_ok=True)
                shutil.copy2(f,dst)
        for rel in write:
            f=ROOT/rel;s=PAYLOAD/rel
            f.parent.mkdir(parents=True,exist_ok=True)
            temp=f.with_name(f.name+'.stage4-tmp')
            shutil.copyfile(s,temp)
            temp.replace(f)
            if sha(f)!=MANIFEST[rel]['after']: raise RuntimeError(f'write verification failed: {rel}')
    except Exception as e:
        print('ERROR installing:',e)
        # Restore every original, or remove a newly added header.
        for rel in write:
            f=ROOT/rel;b=backup/rel
            if b.exists():shutil.copy2(b,f)
            elif f.exists():f.unlink()
        print('Original source restored after failure.')
        return 2
    print('PASS: analogBEAST Stage4 Playback/ARP source applied; original files:',backup)
    print('SOURCE ONLY – DO NOT FLASH. Run Stage4 tests and a full target build next.')
    return 0
if __name__=='__main__':sys.exit(run())
