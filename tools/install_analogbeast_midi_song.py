#!/usr/bin/env python3
# -*- coding: utf-8 -*-
r"""Offline-only installer for analogBEAST graphical MIDI SONG importer.

In C:\FM1\analogBEAST:
  py tools\install_analogbeast_midi_song.py --check-only
  py tools\install_analogbeast_midi_song.py
  py tools\install_analogbeast_midi_song.py --restore

Only web/analogbeast-seq.js is written (plus backup). No MIDI, USB, NOR or firmware actions.
"""
from __future__ import annotations
import argparse, hashlib, shutil, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parent.parent
TARGET=ROOT/'web'/'analogbeast-seq.js'
EDITOR=ROOT/'web'/'editor.html'
SOURCE=ROOT/'tools'/'ab_midi_song_payload'/'analogbeast-seq.js'
BACKUP=ROOT/'web'/'analogbeast-seq.js.before-midi-song-v1'
ALLOWED={
    'c79a2444c75272bc7463372182d0a1434087c6567fba8dee4011f6ad84c7250c', # Graphical Webeditor V2
    'c540a73e3912923ed675b0be906560137da502f5f834cb5a67a5b374ea295dcc', # V2 + preview gate fix
}
MARK='<script src="analogbeast-seq.js" defer></script>'
def sha(p:Path):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    ap=argparse.ArgumentParser(description=__doc__)
    g=ap.add_mutually_exclusive_group()
    g.add_argument('--check-only',action='store_true')
    g.add_argument('--restore',action='store_true')
    args=ap.parse_args()
    try:
        if not all(p.is_file() for p in (EDITOR,TARGET,SOURCE)):
            raise ValueError('Editor/Quellcode fehlt. ZIP im Projektordner entpacken; grafischen Editor V2 vorher installieren.')
        if EDITOR.read_text(encoding='utf-8').count(MARK)!=1:
            raise ValueError('web/editor.html bindet analogbeast-seq.js nicht genau einmal ein; keine Änderung')
        cur,want=sha(TARGET),sha(SOURCE)
        if args.restore:
            if cur!=want:raise ValueError('Editor wurde nach Installation verändert; kein automatischer Rückbau')
            if not BACKUP.is_file() or sha(BACKUP) not in ALLOWED:raise ValueError('Ursprungs-Backup fehlt/unerwartet')
            shutil.copy2(BACKUP,TARGET)
            print('RESTORED: Webeditor vor MIDI SONG Import wiederhergestellt')
            return 0
        if cur==want:
            print('ALREADY: MIDI SONG Import installiert; nichts geändert');return 0
        if cur not in ALLOWED:
            raise ValueError('Unbekannte Webeditor-Version. Refuse to overwrite; bitte SHA256 und aktuellen Dateistand mitteilen')
        if BACKUP.exists() and sha(BACKUP)!=cur:
            raise ValueError('Backup-Ziel existiert mit anderen Daten; Abbruch ohne Überschreiben')
        if args.check_only:
            print('PASS: graphical editor recognized; full MIDI SONG importer ready; no files changed')
            return 0
        if not BACKUP.exists():shutil.copy2(TARGET,BACKUP)
        shutil.copy2(SOURCE,TARGET)
        if sha(TARGET)!=want:
            shutil.copy2(BACKUP,TARGET)
            raise ValueError('Schreibkontrolle fehlgeschlagen; Backup zurückgespielt')
        print('PATCHED: web\\analogbeast-seq.js')
        print('BACKUP: web\\analogbeast-seq.js.before-midi-song-v1')
        print('PASS: entire MIDI songs: track selection, auto split 16/64, TEMPO, P01, END, offline preview, JSON staging')
        print('NO FIRMWARE, BUILD, MIDI, USB, FLASH, NOR OR DEVICE WRITE OPERATIONS')
        return 0
    except (ValueError,OSError,UnicodeError) as exc:
        print('REFUSED:',exc,file=sys.stderr)
        return 2
if __name__=='__main__':raise SystemExit(main())
