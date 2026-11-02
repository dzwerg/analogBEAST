#!/usr/bin/env python3
"""Explicit, single-use NOR migration commit. NEVER runs at startup or on import.

1. Stop playback / REC, inspect imported A01-D16 and keep the ab3.5 JSON backup.
2. Save the current RAM model with analogbeast_backup_v4.py (READ ONLY).
3. Run this tool with --backup /path/to/the/v4-backup.json.
4. Tool independently rereads all 64 patterns and 256 song rows twice;
   commit is sent ONLY after an exact human confirmation and data equality.
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path
from analogbeast_import_songs_v4 import FM1
from analogbeast_backup_v4 import collect

KEY='COMMIT V4'

def verify_backup(backup: dict, current: dict) -> None:
    if backup.get('format') != 'analogBEAST-v4-read-only':
        raise RuntimeError('Kein gültiges schreibgeschütztes v4-Backup')
    pats=backup.get('patterns')
    song=backup.get('song')
    if not isinstance(pats,list) or len(pats)!=64 or not isinstance(song,list) or len(song)!=256:
        raise RuntimeError('Backup unvollständig – benötigt alle 64 Patterns und 256 Songzeilen')
    if any(len(p.get('steps',[]))!=64 for p in pats):
        raise RuntimeError('Backup hat nicht alle 4096 Steps')
    # Do not compare status flags or timestamps: only the contents being committed.
    for section in ('patterns','song','song_meta'):
        if current.get(section) != backup.get(section):
            raise RuntimeError('RAM weicht vom Backup ab: '+section+' – ABGEBROCHEN (kein Commit)')

def check_status(dev) -> None:
    st=dev.dz(18)
    if len(st)!=6 or any(x not in (0,1) for x in st):
        raise RuntimeError('Migration Status-Opcode 18 fehlt oder liefert ungültige Daten')
    locked,pending,enabled,dirty,committed,error=st
    if not (locked==1 and pending==1 and enabled==1 and committed==0 and error==0):
        raise RuntimeError('Kein bestätigungsfähiger ab3.5-RAM-Import: '+str(st))

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--backup',required=True,type=Path,help='Zuvor erstellte READ-ONLY-v4-JSON-Sicherung')
    ap.add_argument('--in-port');ap.add_argument('--out-port')
    args=ap.parse_args()
    if not args.backup.is_file():raise FileNotFoundError(args.backup)
    backup=json.loads(args.backup.read_text(encoding='utf-8'))
    import mido
    dev=FM1(mido,args.in_port,args.out_port)
    try:
        dev.fw_check(require_flash=True)
        check_status(dev)
        print('READ ONLY: Vergleiche 64 Patterns / 4096 Steps / 256 Arranger-Zeilen ...')
        verify_backup(backup,collect(dev))
        check_status(dev)
        print('PASS: RAM ist identisch zum v4-Backup:',args.backup)
        print('Achtung: Der folgende Schritt schreibt erstmalig den v4-Snapshot in die NOR-Sample-Slots.')
        answer=input('Wenn du die importierten Patterns geprüft hast, tippe exakt COMMIT V4: ').strip()
        if answer!=KEY:
            print('ABGEBROCHEN – keinerlei NOR-Schreibauftrag gesendet.')
            return
        # No write in between: read everything one more time before sending.
        verify_backup(backup,collect(dev))
        check_status(dev)
        result=dev.dz(19,list(KEY.encode('ascii')))
        if result!=[1,0]:
            raise RuntimeError('Commitantwort unklar: '+repr(result))
        final=dev.dz(18)
        if final != [0,0,1,0,1,0]:
            raise RuntimeError('Commitantwort OK, aber Status unerwartet: '+repr(final))
        print('PASS: v4-NOR-Snapshot mit A/B-CRC bestätigt. Autosave ist jetzt freigegeben.')
        print('Die Original-Projektsektoren wurden nicht überschrieben. Neustart-Readback separat prüfen.')
    finally:
        dev.close()

if __name__=='__main__':main()
