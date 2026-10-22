#!/usr/bin/env python3
r"""Install the graphical analogBEAST sequencer editor, strictly limited to web JS.

Usage:
  py tools\install_analogbeast_graphical_editor.py --check-only
  py tools\install_analogbeast_graphical_editor.py
  py tools\install_analogbeast_graphical_editor.py --restore

Runs offline. Does not use MIDI, NOR, firmware, flash, USB, or networking.
"""
from __future__ import annotations
import argparse
import hashlib
from pathlib import Path
import shutil
import sys

ROOT=Path(__file__).resolve().parent.parent
DEST=ROOT/'web'/'analogbeast-seq.js'
SRC=ROOT/'tools'/'ab_graphical_payload'/'analogbeast-seq.js'
EDITOR=ROOT/'web'/'editor.html'
BACKUP=ROOT/'web'/'analogbeast-seq.js.before-graphical-v2'
OLD_SHA='e1e2c2e24a6596f30df69db6f7490aaa9336dc35dc269ec8125aa9af6928dfab'
MARKER='<script src="analogbeast-seq.js" defer></script>'


def sha(p:Path)->str:return hashlib.sha256(p.read_bytes()).hexdigest()

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    g=ap.add_mutually_exclusive_group()
    g.add_argument('--check-only',action='store_true')
    g.add_argument('--restore',action='store_true')
    args=ap.parse_args()
    try:
        if not SRC.is_file() or not DEST.is_file() or not EDITOR.is_file():
            raise ValueError('Missing source, existing web/analogbeast-seq.js or web/editor.html; nothing modified')
        if EDITOR.read_text(encoding='utf-8').count(MARKER)!=1:
            raise ValueError('web/editor.html must contain exactly one analogbeast-seq.js script tag')
        original=sha(DEST);wanted=sha(SRC)
        if args.restore:
            if original!=wanted:raise ValueError('Current JS differs from this release; refusing restore')
            if not BACKUP.is_file() or sha(BACKUP)!=OLD_SHA:
                raise ValueError('Original backup missing or unrecognized; refusing restore')
            shutil.copy2(BACKUP,DEST)
            print('RESTORED: original analogBEAST SEQ editor JS; firmware and data unchanged')
            return 0
        if original==wanted:
            print('ALREADY: graphical editor is installed; no changes made')
            return 0
        if original!=OLD_SHA:
            raise ValueError('Existing editor differs from the verified v1.0 editor. Refusing to overwrite; inspect manually.')
        if BACKUP.exists() and sha(BACKUP)!=OLD_SHA:
            raise ValueError('Backup path occupied by a different file; refusing overwrite')
        if args.check_only:
            print('PASS: existing editor verified, graphical JS ready; no files modified')
            return 0
        if not BACKUP.exists():shutil.copy2(DEST,BACKUP)
        shutil.copy2(SRC,DEST)
        if sha(DEST)!=wanted:
            shutil.copy2(BACKUP,DEST)
            raise ValueError('Copy verification failed; original restored')
        print('PATCHED: web\\analogbeast-seq.js')
        print('BACKUP: web\\analogbeast-seq.js.before-graphical-v2')
        print('PASS: Piano-Roll, SONG timeline, ALBUM playlist, MIDI diagnostics and browser preview installed')
        print('NO FIRMWARE BUILD, MIDI, USB, FLASH, NOR OR SONG/PATTERN WRITE OPERATIONS')
        return 0
    except (ValueError,OSError,UnicodeError) as e:
        print('REFUSED:',e,file=sys.stderr)
        return 2

if __name__=='__main__':raise SystemExit(main())
