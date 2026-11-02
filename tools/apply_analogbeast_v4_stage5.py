#!/usr/bin/env python3
"""Source-only Stage 5: repair FUN10 overflow without replacing unknown sources."""
import hashlib
import json
import os
import shutil
import sys
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PAYLOAD = ROOT / 'tools' / 'analogbeast_v4_stage5_payload'
def normalized(data):
    return data.replace(b'\r\n', b'\n')
def checksum(data):
    return hashlib.sha256(normalized(data)).hexdigest()

def main():
    manifest = json.loads((PAYLOAD / 'manifest.json').read_text(encoding='utf-8'))
    checks = []
    failures = []
    for row in manifest['files']:
        dest, patch = ROOT / row['path'], PAYLOAD / row['path']
        if not dest.is_file() or not patch.is_file():
            failures.append(f"Missing {row['path']}")
            continue
        original, replacement = dest.read_bytes(), patch.read_bytes()
        got = checksum(original)
        if checksum(replacement) != row['after_sha256_lf']:
            failures.append(f"PATCH DATA CHECKSUM FAILED: {row['path']}")
        elif got == row['before_sha256_lf']:
            checks.append((row, dest, original, replacement, False))
        elif got == row['after_sha256_lf']:
            checks.append((row, dest, original, replacement, True))
        else:
            failures.append(f"SOURCE DIFFERS FROM TESTED v4 STAGE4: {row['path']}")
    if failures or len(checks) != len(manifest['files']):
        print('STOP – nothing changed:')
        for reason in failures: print(' -', reason)
        return 2
    if all(x[4] for x in checks):
        print('PASS: Stage5 already applied; no changes necessary')
        return 0
    if any(x[4] for x in checks):
        print('STOP – mixed source revisions, nothing changed')
        return 2
    stamp=datetime.now().strftime('%Y%m%d-%H%M%S')
    backup=ROOT/f'source-backup-before-v4-stage5-{stamp}'
    if backup.exists():
        print('STOP – backup folder already exists, nothing changed')
        return 2
    # Copy originals completely before writing even one replacement.
    for row,dest,original,replacement,_ in checks:
        save=backup/row['path'];save.parent.mkdir(parents=True,exist_ok=True)
        save.write_bytes(original)
        if save.read_bytes() != original:
            print('STOP – backup verification failed, nothing changed');return 2
    written=[]
    try:
        for row,dest,original,replacement,_ in checks:
            crlf = b'\r\n' in original
            data = normalized(replacement).replace(b'\n',b'\r\n') if crlf else normalized(replacement)
            temp=dest.with_name(dest.name+'.analogbeast-stage5.tmp')
            temp.write_bytes(data)
            if checksum(temp.read_bytes()) != row['after_sha256_lf']:
                raise RuntimeError('write verification failed: '+row['path'])
            os.replace(temp,dest)
            written.append((dest, original))
    except Exception as exc:
        for dest,original in written: dest.write_bytes(original)
        print('STOP – error; originals restored:',exc)
        return 3
    print('PASS: Stage5 FUN10 full-record fix installed')
    print('SOURCE BACKUP:',backup)
    print('SOURCE ONLY – DO NOT FLASH UNTIL TARGET BUILD AND TESTS PASS!')
    return 0
if __name__ == '__main__':sys.exit(main())
