#!/usr/bin/env python3
"""Safety-checked installer: loosen ONLY the blank-song count precondition.
Does not open MIDI, touch firmware, samples, NOR, or any FM-1 port.
"""
from pathlib import Path
import hashlib
import shutil

EXPECT_SHA = "8da67f806aa9cd37807bd5b21021008e1732e50e4c04c095cba762071f1b165c"
OLD = "    if current.get('song_meta',[None])[0] != 1:\n        raise RuntimeError('v4 song count no longer 1; refusing overwrite')\n"
NEW = "    # Selecting a row in the v4 UI can extend song_count while leaving every\n    # row at the untouched A01 placeholder. Protect actual CONTENT, not count 1.\n    meta = current.get('song_meta')\n    if not (isinstance(meta, list) and len(meta) == 4 and\n            all(type(x) is int and 0 <= x <= 127 for x in meta) and\n            meta[2] in (0, 1) and meta[3] in (0, 1)):\n        raise RuntimeError('Invalid v4 song metadata; refusing overwrite: '+repr(meta))\n    count = meta[0] | (meta[1] << 7)\n    if not 1 <= count <= 256:\n        raise RuntimeError('Invalid v4 song count; refusing overwrite: '+str(count))\n    if count != 1:\n        print(f'SAFE: v4 song_count={count}, but ALL 256 song rows are default A01; no song contents to replace')\n"


def main():
    root = Path(__file__).resolve().parent.parent
    target = root / 'tools' / 'analogbeast_restore_arranger_v4.py'
    if not target.is_file():
        raise SystemExit('ABORT: original arranger restore tool not found')
    b = target.read_bytes()
    digest = hashlib.sha256(b).hexdigest()
    changed = b.decode('utf-8').replace(OLD, NEW)
    if digest != EXPECT_SHA:
        if NEW in b.decode('utf-8') and OLD not in b.decode('utf-8'):
            print('ALREADY APPLIED: blank-song-count fix is present')
            return
        raise SystemExit('ABORT: source SHA mismatch; do not modify an unknown version: '+digest)
    if b.decode('utf-8').count(OLD) != 1:
        raise SystemExit('ABORT: cannot locate exact guard block')
    assert changed.count(NEW)==1
    backup = target.with_suffix('.py.before-arranger-count-fix')
    if backup.exists():
        if backup.read_bytes()!=b:
            raise SystemExit('ABORT: existing backup has different content: '+str(backup))
    else:
        with backup.open('xb') as out:
            out.write(b)
    target.write_text(changed,encoding='utf-8',newline='\n')
    print('PASS: updated only verify_blank_song song_count guard')
    print('BACKUP:',backup)
    print('No MIDI opened; NO DEVICE WRITES')

if __name__ == '__main__':
    main()
