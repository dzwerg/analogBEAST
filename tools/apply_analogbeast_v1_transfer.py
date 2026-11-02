#!/usr/bin/env python3
"""Install version-checked analogBEAST v1.0 MIDI Song/Album/Pattern transfers.

Idempotent. Check all source hashes before modifying ANY file. Backups use
*.before-v1-transfer (created once). Never accesses USB, flash or MIDI.
"""
import argparse
import hashlib
from pathlib import Path
import shutil
import sys

ROOT=Path(__file__).resolve().parents[1]
PAYLOAD=ROOT/'tools'/'ab_v1_transfer_payload'
OLD_MIDI_SHA='b59b7a060163e50d10c96e83b6343b17ff3bf52e5aa3b26304a8422871729f36'
FILES=('firmware/src/golana_dz_midi.c','firmware/src/analogbeast_v5_transfer.c')

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--check-only',action='store_true')
    opt=ap.parse_args()
    todo=[]
    for relative in FILES:
        dst=ROOT/relative
        src=PAYLOAD/relative
        if not src.is_file():raise RuntimeError(f'Payload missing: {relative}')
        if dst.exists():
            actual=sha(dst)
            if actual==sha(src):
                print('ALREADY:',relative);continue
            if relative=='firmware/src/golana_dz_midi.c' and actual==OLD_MIDI_SHA:
                todo.append((src,dst));continue
            raise RuntimeError(f'Source version unexpected: {relative}; no files changed.')
        if relative=='firmware/src/analogbeast_v5_transfer.c':
            todo.append((src,dst));continue
        raise RuntimeError(f'Missing required existing source {relative}')
    if opt.check_only:
        print(f'PASS: {len(todo)} source files ready, no modifications.');return 0
    # Reserve backups before making any changes; do not overwrite previous ones.
    for src,dst in todo:
        if dst.exists():
            backup=dst.with_name(dst.name+'.before-v1-transfer')
            if not backup.exists():shutil.copy2(dst,backup)
    for src,dst in todo:
        dst.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(src,dst)
        if sha(dst)!=sha(src):raise RuntimeError(f'Write check failed: {dst}')
        print('PATCHED:',dst.relative_to(ROOT).as_posix())
    print('PASS: v1 MIDI import/export installed; firmware NOT flashed')
    return 0

if __name__=='__main__':
    try:sys.exit(main())
    except (OSError,RuntimeError) as e:print('ERROR:',e,file=sys.stderr);sys.exit(2)
