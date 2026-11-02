#!/usr/bin/env python3
"""analogBEAST ab3.5: correct GATE over NOTE + consecutive TIE steps.

Usage: py tools\apply_analogbeast_ab35_gate_fix.py --check/--apply/--verify
No flash writes. BRUTE dev5 must be byte-identical.
"""
from pathlib import Path
import argparse, hashlib, sys
ROOT=Path(__file__).resolve().parent.parent
REL=Path('firmware/src/seq.c')
SOURCE=Path(__file__).resolve().with_name('seq_ab35_target.txt')
BASE_SHA='de1a2a08ff7b849eb98c4d27bcf8012b4f9d2f19f0d02c070115ef7425dd1dfa'
TARGET_SHA='d24af3bf5bef0b6876ca1d737f0c805a609c526559764b41a59f0480e15f10d1'

def norm(b): return b.replace(b'\r\n', b'\n').replace(b'\r', b'\n')
def sha(b): return hashlib.sha256(norm(b)).hexdigest()
def main():
    ap=argparse.ArgumentParser(description=__doc__)
    options=ap.add_mutually_exclusive_group(required=True)
    for opt in ('check','apply','verify'): options.add_argument('--'+opt,action='store_true')
    args=ap.parse_args()
    dsp=ROOT/'firmware/src/eng_golana.c'
    dev5=ROOT/'firmware/src/eng_golana.c.dev5-before-fb-fix.bak'
    if not dsp.is_file() or not dev5.is_file() or dsp.read_bytes()!=dev5.read_bytes():
        raise RuntimeError('BRUTE dev5 not byte-identical or missing; STOP')
    print('PASS BRUTE dev5 byte-identical')
    file=ROOT/REL
    if not file.is_file(): raise RuntimeError(f'Not found: {file}')
    data=file.read_bytes(); digest=sha(data)
    target=SOURCE.read_bytes()
    if sha(target)!=TARGET_SHA: raise RuntimeError('Corrupted patch payload')
    if digest==TARGET_SHA:
        print('ALREADY: seq.c ab3.5')
        if args.verify: print('PASS ab3.5 installed')
        return
    if digest != BASE_SHA:
        print('ACTUAL:', digest, 'EXPECTED:',BASE_SHA)
        raise RuntimeError('Unknown seq.c revision; nothing changed')
    if args.verify: raise RuntimeError('ab3.5 not installed')
    print('READY: seq.c ab3.4 => ab3.5')
    if args.check:
        print('CHECK ONLY: no changes')
        return
    backup=file.with_name(file.name+'.before-ab35-gate-fix')
    if backup.exists(): raise RuntimeError(f'Backup already exists: {backup}')
    backup.write_bytes(data)
    lineend=b'\r\n' if b'\r\n' in data else b'\n'
    write=norm(target).replace(b'\n',lineend)
    file.write_bytes(write)
    if sha(file.read_bytes())!=TARGET_SHA: raise RuntimeError('Written data invalid: restore backup')
    print('PATCHED firmware/src/seq.c; original saved:',backup.name)
    print('PASS ab3.5 source installed; TEST AND BUILD BEFORE FLASHING')

if __name__=='__main__':
    try:main()
    except Exception as e:sys.exit('ERROR: '+str(e))
