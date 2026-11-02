#!/usr/bin/env python3
"""Safely install v4 Stage3 source, MIDI importer and read-only backup; no flashing."""
from pathlib import Path
from datetime import datetime
import hashlib, shutil, sys
ROOT=Path(__file__).resolve().parents[1]
BUNDLE=ROOT/'analogBEAST-v4-stage3-MIDI256'
REL='firmware/src/golana_dz_midi.c'
TOOLS=['analogbeast_import_songs_v4.py','analogbeast_backup_v4.py']
TESTS=['test_analogbeast_v4_stage3_source.py','test_analogbeast_v4_midi_protocol.c','test_analogbeast_v4_tools.py']
# Hash matched to the already-installed Stage2 file; normalize only the proven Windows CRLF variant.
STAGE2_SHA='f37c0b1fdf9bc381691435384c4a03b40f8d224e673008e783aa2b271feb9d0c'
STAGE3_SHA='f0e68900ad39310a176c19891cb49c3579a99c70fc544abda0f76c774267b79f'
def digest(data):return hashlib.sha256(data.replace(b'\r\n',b'\n')).hexdigest()
old=ROOT/REL
issues=[]
if not BUNDLE.is_dir():issues.append('Stage3 ZIP nicht vollständig im Projekt entpackt')
if not old.is_file():issues.append('golana_dz_midi.c fehlt')
else:
    current=digest(old.read_bytes())
    if current not in (STAGE2_SHA,STAGE3_SHA):issues.append('golana_dz_midi.c ist nicht der bekannte Stage2-Stand')
for rel,expected in [
('firmware/src/golana_ui.c',b'{B_REC, "SET-PATT", "PATT"'),
('firmware/src/golana_pattern_model.h',b'#define DZ_NSONG AB_SONG_ROWS'),
('firmware/src/analogbeast_snapshot.h',b'#define AB_SONG_ROWS 256u'),
('firmware/src/usb.c',b"'F', 0, 'e', 0, 'l', 0, 'u'"),
]:
    p=ROOT/rel
    if not p.is_file() or expected not in p.read_bytes():issues.append(rel+': erforderlicher Stage2-Stand fehlt')
eng=ROOT/'firmware/src/eng_golana.c';dev5=ROOT/'firmware/src/eng_golana.c.dev5-before-fb-fix.bak'
if not eng.is_file() or not dev5.is_file() or eng.read_bytes()!=dev5.read_bytes():
    issues.append('BRUTE dev5 nicht identisch mit Referenz')
if not list(ROOT.glob('analogBEAST-bank-backup-*.json')):
    issues.append('Kein ab3.5-Bank-Backup vorhanden')
for file in ['payload/'+REL]+['tools/'+x for x in TOOLS]+['tests/'+x for x in TESTS]+['reference-midi/Brahms_Wiegenlied.mid','reference-midi/Musette_BWV_Anh126_Bonus.mid']:
    if not (BUNDLE/file).is_file():issues.append('Stage3-Datei fehlt: '+file)
for file in [f'tools/{t}' for t in TOOLS]+[f'tests/{t}' for t in TESTS]:
    dest=ROOT/file
    if dest.exists() and dest.read_bytes()!=(BUNDLE/file).read_bytes():
        issues.append('Zieldatei existiert mit anderem Inhalt: '+file)
if issues:
    print('STOP – nichts verändert:')
    for x in issues: print(' -',x)
    sys.exit(2)
if current==STAGE3_SHA:
    print('PASS: Stage3 ist schon installiert – keine Änderungen')
    sys.exit(0)
backup=ROOT/('source-backup-before-v4-stage3-'+datetime.now().strftime('%Y%m%d-%H%M%S'))
if backup.exists():sys.exit('STOP: Backup-Ordner existiert bereits')
backup.mkdir()
(backup/REL).parent.mkdir(parents=True,exist_ok=True)
shutil.copy2(old,backup/REL)
# All preflight is complete, only one existing firmware file gets replaced.
shutil.copy2(BUNDLE/'payload'/REL,old)
for file in [f'tools/{t}' for t in TOOLS]+[f'tests/{t}' for t in TESTS]:
    dst=ROOT/file
    if not dst.exists():
        dst.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(BUNDLE/file,dst)
print('PASS: analogBEAST v4 Stage3 MIDI256 Source installiert; Backup:',backup)
print('QUELLCODE – NICHT FLASHEN!')
