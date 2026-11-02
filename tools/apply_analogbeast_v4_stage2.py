#!/usr/bin/env python3
"""Stage 2 v4 SOURCE patch. Accept LF/CRLF-only differences, never flash/erase."""
import hashlib
import json
import shutil
import sys
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUNDLE = ROOT / 'analogBEAST-v4-stage2-REC-source'
MANIFEST = BUNDLE / 'stage2-manifest.json'


def same_source(path: Path, expected_sha256: str) -> tuple[bool, str]:
    """Accept only exact contents or Windows CRLF instead of original LF.

    Does NOT ignore whitespace, different source lines, missing final newline,
    bare CR, or other changes. Use byte reads to avoid platform conversions.
    """
    if not path.is_file():
        return False, 'missing'
    data = path.read_bytes()
    if hashlib.sha256(data).hexdigest() == expected_sha256:
        return True, 'exact'
    if b'\r\n' not in data or b'\r' in data.replace(b'\r\n', b''):
        return False, 'modified (not just CRLF)'
    normalized = data.replace(b'\r\n', b'\n')
    if hashlib.sha256(normalized).hexdigest() == expected_sha256:
        return True, 'same source; Windows CRLF'
    return False, 'modified source (normalized SHA-256 mismatch)'


def main() -> int:
    if not MANIFEST.is_file():
        print('STOP – Stage 2 payload/manifest missing. First expand the original Stage 2 ZIP.')
        return 2
    manifest = json.loads(MANIFEST.read_text(encoding='utf-8'))
    failures = []
    crlf_matches = []
    for kind, reason in [('base_sha256', 'not the expected analogBEAST ab3.5 baseline'),
                         ('stage1_required_sha256', 'Stage 1 module missing or different')]:
        for rel, sha in manifest[kind].items():
            ok, info = same_source(ROOT / rel, sha)
            if not ok:
                failures.append(f'{rel}: {reason} [{info}]')
            elif info == 'same source; Windows CRLF':
                crlf_matches.append(rel)

    changed = manifest['changed_files']
    for rel in changed:
        if not (ROOT / rel).is_file():
            failures.append(f'{rel}: source missing')
        if not (BUNDLE / 'payload' / rel).is_file():
            failures.append(f'{rel}: Stage 2 payload missing')

    eng = ROOT / 'firmware/src/eng_golana.c'
    dev5 = ROOT / 'firmware/src/eng_golana.c.dev5-before-fb-fix.bak'
    if not eng.is_file() or not dev5.is_file() or eng.read_bytes() != dev5.read_bytes():
        failures.append('eng_golana.c differs from BRUTE-dev5 reference; no changes allowed')
    if not list(ROOT.glob('analogBEAST-bank-backup-*.json')):
        failures.append('No local analogBEAST bank backup JSON found')
    if failures:
        print('STOP – nothing changed:')
        for problem in failures:
            print(' -', problem)
        return 2

    if crlf_matches:
        print('PASS – same source; only LF/CRLF differed:', ', '.join(crlf_matches))
    backup = ROOT / ('source-backup-before-v4-stage2-' + datetime.now().strftime('%Y%m%d-%H%M%S'))
    if backup.exists():
        print('STOP – backup directory already exists:', backup)
        return 2

    # Full preflight above; back up each exact original file before touching any.
    # If an individual installation copy fails, restore everything from the backup.
    copied = []
    try:
        backup.mkdir()
        for rel in changed:
            dest = backup / rel
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(ROOT / rel, dest)
        for rel in changed:
            copied.append(rel)  # mark before copy in case the copy fails part-way
            shutil.copy2(BUNDLE / 'payload' / rel, ROOT / rel)
        for rel in changed:
            expected = hashlib.sha256((BUNDLE / 'payload' / rel).read_bytes()).digest()
            if hashlib.sha256((ROOT / rel).read_bytes()).digest() != expected:
                raise IOError(f'Verification after copy failed: {rel}')
    except Exception as exc:
        for rel in copied:
            shutil.copy2(backup / rel, ROOT / rel)
        print('STOP – installation failed, original copied files restored:', exc)
        print('Source backup:', backup)
        return 3

    print('PASS: Stage 2 v4 source files installed; originals backed up at', backup)
    print('IMPORTANT: SOURCE ONLY. DO NOT FLASH!')
    return 0


if __name__ == '__main__':
    sys.exit(main())
