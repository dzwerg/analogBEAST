#!/usr/bin/env python3
"""Patch only the v4 SONG STEP browser's false dirty/autosave trigger.

Before applying verifies the exact uploaded Stage6/6b golana_ui.c fingerprint.
Writes an untouched backup of the original; does not touch firmware or hardware.
"""
from __future__ import annotations
import hashlib
from pathlib import Path
from datetime import datetime, timezone

BEFORE = '81db14fb6dbbc125a14163c0e108b6047581cd8bbfe35fa104671073449affd6'
OLD = b'case GV_DZ_SONG_STEP: dz_seq_select_song_step(&dz_model, (uint16_t)(v - 1)); gu_dz_mark_dirty(); return;'
NEW = (
    b'case GV_DZ_SONG_STEP:\n'
    b'        /* Browsing SONG STEP changes only the editor cursor.\n'
    b'         * The complete 21,648-byte A/B snapshot must NOT be rewritten.\n'
    b'         * Editing a SONG row still marks dirty via the other cases. */\n'
    b'        dz_seq_select_song_step(&dz_model, (uint16_t)(v - 1));\n'
    b'        return;'
)

def main() -> None:
    path = Path('firmware/src/golana_ui.c')
    if not path.is_file():
        raise SystemExit('ERROR: aus C:\\FM1\\analogBEAST bzw. /mnt/c/FM1/analogBEAST starten.')
    data = path.read_bytes()
    if hashlib.sha256(data).hexdigest() != BEFORE or data.count(OLD) != 1:
        if NEW in data:
            raise SystemExit('Bereits gepatcht? Datei unveraendert; Test starten.')
        raise SystemExit('ABBRUCH: Quellstand stimmt nicht mit dem getesteten Stage6/6b-Stand ueberein. Nichts geaendert.')
    now = datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S')
    backup = path.with_name(f'golana_ui.c.before-song-nav-fix-{now}')
    with backup.open('xb') as fh:
        fh.write(data)
    updated = data.replace(OLD, NEW)
    path.write_bytes(updated)
    print('PASS: SONG STEP navigation no longer triggers NOR autosave')
    print('Sicherung:', backup)
    print('Geaendert: nur firmware/src/golana_ui.c, ein gu_set() case')
    print('KEIN FLASH / KEIN MIDI / KEIN NOR-SCHREIBZUGRIFF')

if __name__ == '__main__':
    main()
