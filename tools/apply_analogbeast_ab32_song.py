#!/usr/bin/env python3
"""analogBEAST ab3.2: Windows LF/CRLF-safe Songmode for ab2.2; --check --apply --verify."""
import argparse,hashlib,os,sys,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parent.parent
PAYLOAD=Path(__file__).resolve().parent/'analogbeast_ab32_payload.zip'
META={'firmware/src/golana_dz_store.c': {'original': '276dc8325429ba329fe34ca04814e02aafe4192881ad6688e37a3382eb74fc79',
                                    'patched': '7b3bbcbae256f52215f3a219be40b31621e411dc66cac36d0a32a185ee71c8f1'},
 'firmware/src/golana_pattern_model.c': {'original': '9e2ff29c7221ae5cfb818067c080b7b699e5d9d21c93f25b4ddc30118f7dcda2',
                                         'patched': 'f66464331fddb6b2c68927e1b5ac120e3f3630f0f08ebe9248d9a5585f7d8283'},
 'firmware/src/golana_pattern_model.h': {'original': '140063b042678c1ea1567888aad9ffd73b57137ccc8acdd4a89d70b4a6e42246',
                                         'patched': '08d984f8e22168281a9b3084cd2cf1b351416cc71f8263bf5fa1289245cb13aa'},
 'firmware/src/golana_ui.c': {'original': '12d54eec3b847ed1d7b20641f9317f550aebd85d7a4f9149f18453f0e1604c13',
                              'patched': '8e0a9a1773e95bd597f5b2fdf36ba85542effbfab823603a56965a9310968b7c'},
 'tests/analogbeast_ab3_storage_mock.c': {'original': None,
                                          'patched': '55839045133763ba741e358d371642b4bf2a8a7fe7d36a5566a4afe360e78c15'},
 'tests/analogbeast_song_ab3.c': {'original': None,
                                  'patched': '9df029ac64c2b72e08e8d19d0620cc32f47db0aac983d274ea49db15c64b65dc'},
 'tests/dz_stage5_ui.c': {'original': '2f054fe6c76d3219d51a676c4b699fad79f6cffa7c528254f466484306a35bc7',
                          'patched': 'e80e7ec590d461812fb37e298f397e7c193f072334af4ee015df4c198a56e079'},
 'tests/dz_stage6_persist.c': {'original': 'a58df274a621aedb5ee1eb1dd7e2c21e9d3c6cb39829c315197e15e933f836d1',
                               'patched': 'f7fa4ad2d87784db4fbf93ae9f868ac2146a64b4f2cff06cb0a2ba529b99564b'}}
def canonical(data):
    """Recognize the same source regardless of Windows CRLF / Unix LF.
    Reject mixed or legacy CR line endings rather than guessing.
    """
    if b'\r' in data:
        if data.count(b'\r\n') != data.count(b'\r'):
            raise RuntimeError('STOP: unsupported CR or mixed line endings')
    return data.replace(b'\r\n', b'\n')
def digest(data): return hashlib.sha256(canonical(data)).hexdigest()
def target_for_original(payload, before):
    if before is not None and before.count(b'\r\n'):
        if before.count(b'\r\n') != before.count(b'\n'):
            raise RuntimeError('STOP: mixed line endings; will not rewrite')
        return payload.replace(b'\n', b'\r\n')
    return payload
def dev5_check():
    a=ROOT/'firmware/src/eng_golana.c'
    b=ROOT/'firmware/src/eng_golana.c.dev5-before-fb-fix.bak'
    if not a.is_file() or not b.is_file() or a.read_bytes()!=b.read_bytes():
        raise RuntimeError('STOP: BRUTE DSP dev5 changed or backup missing')
    print('PASS: dev5 BRUTE DSP byte-identical')
def main():
    ap=argparse.ArgumentParser()
    g=ap.add_mutually_exclusive_group(required=True)
    g.add_argument('--check',action='store_true')
    g.add_argument('--apply',action='store_true')
    g.add_argument('--verify',action='store_true')
    args=ap.parse_args()
    dev5_check()
    if not PAYLOAD.is_file(): raise RuntimeError('Missing payload: '+str(PAYLOAD))
    todo=[]
    with zipfile.ZipFile(PAYLOAD) as z:
        for rel,hashes in META.items():
            dst=ROOT/rel
            nextbytes=z.read(rel)
            if digest(nextbytes)!=hashes['patched']:
                raise RuntimeError('Corrupted payload: '+rel)
            before=dst.read_bytes() if dst.exists() else None
            if before is not None and digest(before)==hashes['patched']:
                print('ALREADY: '+rel)
                continue
            if before is None and hashes['original'] is None:
                print('CREATE:  '+rel)
            elif before is not None and hashes['original'] and digest(before)==hashes['original']:
                print('READY:   '+rel)
            else:
                raise RuntimeError('STOP: unexpected source version; no changes made: '+rel)
            todo.append((dst,before,target_for_original(nextbytes,before),rel))
    if args.verify:
        if todo: raise RuntimeError(f'ab3.2 incomplete: {len(todo)} files still pending')
        print('PASS: ab3.2 files installed')
        return
    if args.check:
        print('CHECK ONLY: no files written')
        return
    # Check all backup conflicts before touching ANY project file.
    for dst,old,_,rel in todo:
        if old is not None:
            backup=dst.with_name(dst.name+'.before-analogbeast-ab32')
            if backup.is_file() and backup.read_bytes()!=old:
                raise RuntimeError('STOP: prior backup differs, nothing written: '+rel)
    applied=[]
    try:
        for dst,old,nextbytes,rel in todo:
            dst.parent.mkdir(parents=True,exist_ok=True)
            if old is not None:
                backup=dst.with_name(dst.name+'.before-analogbeast-ab32')
                if not backup.exists(): backup.write_bytes(old)
                print('BACKUP: '+str(backup.relative_to(ROOT)))
            tmp=dst.with_name(dst.name+'.analogbeast-ab32.tmp')
            tmp.write_bytes(nextbytes)
            os.replace(tmp,dst)
            applied.append((dst,old))
            print('PATCHED: '+rel)
        dev5_check()
    except Exception:
        for dst,old in reversed(applied):
            if old is None: dst.unlink(missing_ok=True)
            else: dst.write_bytes(old)
        raise
    print('OK: ab3.2 installed; no build or flash')
if __name__=='__main__':
    try: main()
    except Exception as e:
        print('ERROR:',e,file=sys.stderr)
        sys.exit(1)
