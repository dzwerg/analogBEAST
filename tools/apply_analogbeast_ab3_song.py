#!/usr/bin/env python3
"""analogBEAST ab3: 128-step song arranger patch. --check / --apply / --verify."""
import argparse, hashlib, os, shutil, sys, zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parent.parent
PAYLOAD=Path(__file__).resolve().parent/'analogbeast_ab3_payload.zip'
META={'firmware/src/golana_dz_store.c': {'original': '276dc8325429ba329fe34ca04814e02aafe4192881ad6688e37a3382eb74fc79',
                                    'patched': '7b3bbcbae256f52215f3a219be40b31621e411dc66cac36d0a32a185ee71c8f1'},
 'firmware/src/golana_pattern_model.c': {'original': '9e2ff29c7221ae5cfb818067c080b7b699e5d9d21c93f25b4ddc30118f7dcda2',
                                         'patched': 'f66464331fddb6b2c68927e1b5ac120e3f3630f0f08ebe9248d9a5585f7d8283'},
 'firmware/src/golana_pattern_model.h': {'original': '140063b042678c1ea1567888aad9ffd73b57137ccc8acdd4a89d70b4a6e42246',
                                         'patched': '08d984f8e22168281a9b3084cd2cf1b351416cc71f8263bf5fa1289245cb13aa'},
 'firmware/src/golana_ui.c': {'original': '9f4d3038d8316a2ab209da8cfa30f376bdd258f8e88b047133db76860aceea77',
                              'patched': 'd36d28c02d0f74312204bd48cb95413a812cb51302a5a3ef2a47ed1b20427c88'},
 'tests/analogbeast_ab3_storage_mock.c': {'original': None,
                                          'patched': '55839045133763ba741e358d371642b4bf2a8a7fe7d36a5566a4afe360e78c15'},
 'tests/analogbeast_song_ab3.c': {'original': None,
                                  'patched': '9df029ac64c2b72e08e8d19d0620cc32f47db0aac983d274ea49db15c64b65dc'},
 'tests/dz_stage5_ui.c': {'original': 'b4a7f3bd58c7b47e02a0ea6e00dde5039143283c1d6424541668b33e2b037988',
                          'patched': 'de2984d53dc91922afe7eddbec9cb8b005697d7ff220c278585937ffdc9b5d53'},
 'tests/dz_stage6_persist.c': {'original': 'a58df274a621aedb5ee1eb1dd7e2c21e9d3c6cb39829c315197e15e933f836d1',
                               'patched': 'f7fa4ad2d87784db4fbf93ae9f868ac2146a64b4f2cff06cb0a2ba529b99564b'}}

def digest(data): return hashlib.sha256(data).hexdigest()

def audit_dev5():
    dsp=ROOT/'firmware/src/eng_golana.c'
    old=ROOT/'firmware/src/eng_golana.c.dev5-before-fb-fix.bak'
    if not dsp.is_file() or not old.is_file() or dsp.read_bytes()!=old.read_bytes():
        raise RuntimeError('STOP: BRUTE dev5 reference is missing or changed')
    print('PASS: BRUTE DSP exactly dev5')

def main():
    ap=argparse.ArgumentParser()
    g=ap.add_mutually_exclusive_group(required=True)
    g.add_argument('--check',action='store_true')
    g.add_argument('--apply',action='store_true')
    g.add_argument('--verify',action='store_true')
    args=ap.parse_args()
    audit_dev5()
    if not PAYLOAD.is_file(): raise RuntimeError('Payload ZIP missing: '+str(PAYLOAD))
    todo=[]
    with zipfile.ZipFile(PAYLOAD) as z:
        for rel,hm in META.items():
            dst=ROOT/rel
            replacement=z.read(rel)
            if digest(replacement)!=hm['patched']: raise RuntimeError('Corrupt patch payload: '+rel)
            current=dst.read_bytes() if dst.exists() else None
            if current is not None and digest(current)==hm['patched']:
                print('ALREADY: '+rel)
                continue
            if current is None and hm['original'] is None:
                print('CREATE:  '+rel)
            elif current is not None and hm['original'] is not None and digest(current)==hm['original']:
                print('READY:   '+rel)
            else:
                raise RuntimeError('STOP: unexpected file version, no changes made: '+rel)
            todo.append((rel,dst,current,replacement))
        # All preconditions checked before writing anything.
        if args.check:
            print('CHECK ONLY: no files written')
            return
        if args.verify:
            if todo: raise RuntimeError('ab3 not fully installed ('+str(len(todo))+' files pending)')
            print('PASS: all ab3 sources/test files installed')
            return
        for rel,dst,original,replacement in todo:
            dst.parent.mkdir(parents=True,exist_ok=True)
            if original is not None:
                backup=dst.with_name(dst.name+'.before-analogbeast-ab3')
                if backup.exists():
                    if backup.read_bytes()!=original: raise RuntimeError('STOP: backup mismatch: '+str(backup))
                else:
                    backup.write_bytes(original)
                print('BACKUP: '+str(backup.relative_to(ROOT)))
            tmp=dst.with_name(dst.name+'.analogbeast-ab3.tmp')
            tmp.write_bytes(replacement)
            os.replace(tmp,dst)
            print('PATCHED: '+rel)
    audit_dev5()
    print('OK: ab3 installed. No build or flash performed.')

if __name__=='__main__':
    try: main()
    except Exception as e:
        print('ERROR:',e,file=sys.stderr)
        raise SystemExit(1)
