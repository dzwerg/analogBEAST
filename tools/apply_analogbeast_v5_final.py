#!/usr/bin/env python3
"""Apply v5 FINAL candidate on the exact user's Stage4b sources.

No hardware I/O or flashing. Conserves backups and refuses unknown sources.
"""
from pathlib import Path
import hashlib, argparse, shutil
BASE={
    'analogbeast_v5_extra.h': 'e1d6e2f4cbdefd850e0edf8717b320d58ea13e26487d8757707e986c20fd7131',
    'analogbeast_v5_extra.c': '5d41c5560187842fb87200e77ba4d80d689ad828b5d6ff7884b0b762669ac0c7',
    'analogbeast_v5_nor.h': 'e069d443399637c2fba0fb7dd2581da9d6ae3f905f8640014c9d6698ffdf4854',
    'analogbeast_v5_nor.c': 'bb140fc953360f7d686eb8b65c99def6aaace8374ef791dc78684baefa0de81b',
    'analogbeast_v5_controls.h': '46ed6c59a0e65285e871819bf92b0715ee4fcf5e799cdd091f09b143ffafbecb',
    'analogbeast_v5_controls.c': '1c1dbe6995cf82fb24b312bb44da3c623b4bc6e5a5c13dbdc169a4ca0f7c2e9e',
    'golana_ui.c': 'e55e33dcbfd359cc3c0d8b9dfd1991d5dbdaac5404f02cb6012bfdbc3b1afe97',
    'golana_dz_midi.c': '730af21392d6f7f4d18f75e538115ec90287297e91bec931cafd1b898e5dbd69',
    'editor.c': '7785821b00cd2892bac971bd60472759f41a48255bd148911d67b8285a46aa9d',
}
TARGET={
    'analogbeast_v5_extra.h': 'ad18c38269f12f7e2e2d92f0ddd3dc3e5b53fc5a22846ed7e0c185b3618155c3',
    'analogbeast_v5_extra.c': '5376c89c3b179ba04fa38b2d2f48cd13267bb7aff96aaae8223d9d4709fa866e',
    'analogbeast_v5_nor.h': '6d3cabdf0e30bdb3c098bcf3b2a9088b887ad61e40ac07cf202519f98da6e45d',
    'analogbeast_v5_nor.c': '760d3df00be5d68b2c530ea78c824f49d44569177a6706bfd0006a314ab69b49',
    'analogbeast_v5_controls.h': '609305c3af77871931231422feaff0914c0b8747be6f8850c84625343ddddd7e',
    'analogbeast_v5_controls.c': 'ed45702ca9ccefaeff528d691fcc9db44ad9e4c01a6756452dd1659abbd81aed',
    'golana_ui.c': '460a0f02f25b25a9479649c570e282a77d7cd27f05193aeae1c957783d9a5547',
    'golana_dz_midi.c': 'b59b7a060163e50d10c96e83b6343b17ff3bf52e5aa3b26304a8422871729f36',
    'editor.c': 'c8efb79ecdf2527c6af755e4c052169ffb788522e48074789f0d3511398803ab',
}

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--check-only',action='store_true')
    args=ap.parse_args()
    root=Path(__file__).resolve().parents[1]
    fw=root/'firmware'/'src'
    payload=root/'tools'/'v5_final_payload'/'firmware'/'src'
    # Validate *every* source before changing even one. Accept already patched
    # files for idempotency and interrupted manual retries.
    for name in BASE:
        dest=fw/name
        source=payload/name
        if not dest.is_file() or not source.is_file() or digest(source)!=TARGET[name]:
            raise SystemExit(f'MISSING/INVALID SOURCE OR PAYLOAD: {name}')
        have=digest(dest)
        if have not in (BASE[name],TARGET[name]):
            raise SystemExit(f'STOP: unexpected local source {name} SHA256={have}. No files changed.')
        backup=dest.with_name(name+'.before-v5-final')
        if backup.exists() and digest(backup)!=BASE[name]:
            raise SystemExit(f'STOP: old backup differs, preserve manually: {backup}')
    if args.check_only:
        print('PASS: all 9 source version checks + payload hashes match, NO files changed')
        return
    for name in BASE:
        dest=fw/name
        if digest(dest)==TARGET[name]: continue
        backup=dest.with_name(name+'.before-v5-final')
        if not backup.exists():shutil.copy2(dest,backup)
        # Atomic per file; all base versions were checked in advance.
        tmp=dest.with_name(name+'.v5final-tmp')
        shutil.copy2(payload/name,tmp)
        tmp.replace(dest)
        if digest(dest)!=TARGET[name]:raise SystemExit(f'PATCH VERIFICATION FAILED: {name}')
        print('PATCHED:',name)
    print('PASS: v5 final candidate installed. No firmware flashed; run host + firmware build.')

if __name__=='__main__':main()
