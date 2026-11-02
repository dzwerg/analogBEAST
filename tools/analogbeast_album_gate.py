#!/usr/bin/env python3
"""analogBEAST ab3.4: two GATE commands into the current 16-entry Brahms/Musette album.

--preview: USB-MIDI READ ONLY.  --apply: back up all 128 rows, prompt and upload
modified SONG rows only (A01..A08 patterns and 32 sound presets never written).
STOP is mandatory; requires MIDI protocol v3 with GATE capability flag.
"""
import argparse
import datetime as dt
import json
from pathlib import Path
import sys
import mido
from analogbeast_import_songs import FM1, SONG_END, SONG_GATE, SONG_TEMPO, SONG_PRESET, SONG_REST

BLANK = (SONG_END, 16, 1, 0)


def read_complete(dev):
    meta = dev.dz(8)
    if len(meta) != 3 or meta[1] not in (0, 1) or meta[2] not in (0, 1):
        raise RuntimeError('Ungültige Metadaten: ' + repr(meta))
    rows = []
    for start in range(0, 128, 24):
        count = min(24, 128 - start)
        data = dev.dz(5, [start, count])
        if len(data) != 2 + 5 * count or data[:2] != [start, count]:
            raise RuntimeError(f'Auslesen ab {start + 1} fehlgeschlagen')
        for i in range(count):
            bank, pat, steps, lo, hi = data[2+5*i:7+5*i]
            rows.append((bank, pat, steps, lo | hi << 7))
    if dev.dz(8) != meta:
        raise RuntimeError('Album wurde während Auslesen verändert. STOP drücken.')
    return meta, rows


def build_album_gate(meta, rows):
    # DO NOT guess/overwrite the user's edits; only the exact established album.
    if meta != [16, 1, 1] or len(rows) != 128:
        raise ValueError('Nicht das erwartete 16-Step-Album mit SONG ON und LOOP.')
    expected = [
        (SONG_TEMPO, 0, 1, 66),  (SONG_PRESET, 0, 1, 17),
        (SONG_REST, 0, 1, 0), (0, 0, 64, 0), (0, 1, 64, 0),
        (0, 2, 64, 0), (SONG_REST, 0, 64, 0),
        (SONG_PRESET, 0, 1, 18), (SONG_TEMPO, 0, 1, 120),
        (SONG_REST, 0, 1, 0), (0, 3, 64, 0), (0, 4, 64, 0),
        (0, 5, 64, 0), (0, 6, 64, 0), (0, 7, 64, 0), BLANK,
    ]
    # The UI does not display PLAY STEPS on zero-time commands. Those fields
    # can retain 64 from the original pattern row and must be preserved.
    for i, (actual, want) in enumerate(zip(rows[:16], expected), 1):
        kind, pat, steps, value = actual
        if kind in (SONG_TEMPO, SONG_PRESET, SONG_END):
            good = kind == want[0] and pat == want[1] and value == want[3] and 1 <= steps <= 64
        else:
            good = actual == want
        if not good:
            raise ValueError(f'Song-Step {i:03} wurde verändert: Gerät={actual}, erwartet={want}. NICHTS geschrieben.')
    if rows[16:] != [BLANK] * 112:
        raise ValueError('Songplätze hinter 016 enthalten andere Daten; nichts geschrieben.')
    result = rows[:2] + [(SONG_GATE, 0, 1, 122)] + rows[2:9] + [
        (SONG_GATE, 0, 1, 105)] + rows[9:16] + [BLANK] * 110
    if len(result) != 128 or result[17] != BLANK:
        raise AssertionError('interner Fehler bei Album-Einträgen')
    return result


def write_complete(dev, rows, count, old_meta):
    for start in range(0, 128, 24):
        chunk = rows[start:start+24]
        payload = [start, len(chunk)]
        for kind, pattern, steps, val in chunk:
            payload += [kind, pattern, steps, val & 127, val >> 7]
        ack = dev.dz(4, payload)
        if ack != [start, len(chunk)]:
            raise RuntimeError(f'Song-Write-ACK ab {start+1} falsch: {ack}')
        result = dev.dz(5, [start, len(chunk)])
        if result != payload:
            raise RuntimeError(f'Song-Readback ab {start+1} falsch')
        print(f'PASS Song {start+1:03}–{start+len(chunk):03} zurückgelesen')
    meta = [count if count < 128 else 0, old_meta[1], old_meta[2]]
    if dev.dz(7, meta) != meta or dev.dz(8) != meta:
        raise RuntimeError('Song-Metadaten falsch: KEIN Power-Off vor Rettung')
    dev.save()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--preview', action='store_true', help='Live lesen, neue GATE-Positionen anzeigen; kein Schreiben')
    mode.add_argument('--apply', action='store_true', help='Bestehende 16er-Liste sichern und nur SONG auf 18 Einträge ändern')
    parser.add_argument('--in-port');parser.add_argument('--out-port')
    args=parser.parse_args()
    dev = FM1(mido, args.in_port, args.out_port)
    try:
        dev.fw_check(require_gate=True)
        meta, before = read_complete(dev)
        after = build_album_gate(meta, before)
        print('\nAltes Album: 16 Einträge, LOOP; bleibt vollständig erhalten.')
        print('NEU: 003 GATE 122/128 für Brahms; 012 GATE 105/128 für Musette.')
        print('NEU: 18 Song-Einträge, LOOP; A01–A08 und P18/P19 unverändert.')
        if args.preview:
            print('READ ONLY: nichts geändert.')
            return
        name = 'analogBEAST_album_vor_Gate_' + dt.datetime.now().strftime('%Y%m%d_%H%M%S') + '.json'
        backup = Path.cwd() / name
        backup.write_text(json.dumps({'protocol':'analogBEAST v3', 'meta':meta,
                            'rows':[list(x) for x in before]}, indent=2), encoding='utf-8')
        print('BACKUP:',backup)
        print('ACHTUNG: Songliste wird geändert. Pattern und Presets NICHT.')
        if input('Für Einfügen exakt JA ALBUM GATE eingeben: ').strip() != 'JA ALBUM GATE':
            print('ABBRUCH: nichts geschrieben.');return
        # Detect external changes during the confirmation and abort instead of clobbering.
        if read_complete(dev) != (meta, before):
            raise RuntimeError('Song wurde zwischen Backup und Bestätigung geändert; ABBRUCH')
        write_complete(dev, after, 18, meta)
        print('\nFERTIG! 18 Song-Einträge mit GATE 122/128 und 105/128, LOOP erhalten.')
    finally:
        dev.close()

if __name__ == '__main__':
    try: main()
    except (ValueError, RuntimeError, TimeoutError, OSError, EOFError) as exc:
        sys.exit('FEHLER: ' + str(exc))
