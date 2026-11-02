#!/usr/bin/env python3
"""Strict Stage4b install: changes one source, never accesses FM-1 or flash."""
from pathlib import Path
import argparse, hashlib, shutil, sys, os
OLD_SHA = "c300a529eda608049e22df535ba897b7cd7ee82b9fc8a03dc7761a218ce49c3f"
NEW_SHA = "e55e33dcbfd359cc3c0d8b9dfd1991d5dbdaac5404f02cb6012bfdbc3b1afe97"
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--check-only',action='store_true')
    args=ap.parse_args()
    root=Path(__file__).resolve().parent.parent
    dst=root/'firmware/src/golana_ui.c'
    src=root/'tools/stage4b_payload/golana_ui.c'
    backup=dst.with_name('golana_ui.c.before-v5-stage4b')
    if not src.is_file() or sha(src)!=NEW_SHA:
        sys.exit('ERROR: Stage4b payload missing or damaged; STOP')
    if not dst.is_file():sys.exit('ERROR: firmware/src/golana_ui.c not found; STOP')
    current=sha(dst)
    if current==NEW_SHA:
        print('PASS: Stage4b already installed; no changes')
        return
    if current!=OLD_SHA:
        sys.exit('ERROR: current golana_ui.c differs from supplied Stage4a source; STOP (no overwrite). '
                 'Upload the changed file for a rebased patch.')
    if backup.exists() and sha(backup)!=OLD_SHA:
        sys.exit('ERROR: Stage4b backup path contains different data; STOP')
    print('PASS: exact Stage4a source confirmed; patch is compatible')
    print('PASS: no flash, MIDI, NOR, v4 engine, DSP or autosave code changed')
    if args.check_only:
        print('CHECK ONLY: no source files changed')
        return
    if not backup.exists():shutil.copy2(dst,backup)
    tmp=dst.with_name('.golana_ui.c.v5-stage4b.tmp')
    try:
        shutil.copy2(src,tmp)
        os.replace(tmp,dst)
    finally:
        if tmp.exists():tmp.unlink()
    if sha(dst)!=NEW_SHA:sys.exit('ERROR: post-write SHA mismatch')
    print('PASS: Stage4b PLAY bridge installed, backup '+str(backup))
if __name__=='__main__':main()
