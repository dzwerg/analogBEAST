#!/usr/bin/env python3
"""Stage6b RAM-only migration repair: SHA guarded and transactional.
No flash, no USB operations, no MIDI commands. Local SOURCE only.
"""
from pathlib import Path
from datetime import datetime
import hashlib,json,os,sys
ROOT=Path(__file__).resolve().parent.parent
PAYLOAD=ROOT/'tools'/'analogbeast_v4_stage6b_payload'
def normal(data):return data.replace(b'\r\n',b'\n')
def sha(data):return hashlib.sha256(normal(data)).hexdigest()
def main():
    data=json.loads((PAYLOAD/'manifest.json').read_text(encoding='utf-8'))
    errors=[];todo=[];installed=[]
    for rel,expect in data['guards'].items():
        path=ROOT/rel
        if not path.is_file() or sha(path.read_bytes())!=expect:
            errors.append('GUARD SOURCE DIFFERS: '+rel)
    brute=ROOT/'firmware/src/eng_golana.c'
    dev5=ROOT/'firmware/src/eng_golana.c.dev5-before-fb-fix.bak'
    if not brute.is_file() or not dev5.is_file() or brute.read_bytes()!=dev5.read_bytes():
        errors.append('BRUTE dev5 byte identity differs')
    for row in data['files']:
        rel=row['path'];path=ROOT/rel; payload=PAYLOAD/rel
        if not payload.is_file() or sha(payload.read_bytes())!=row['after_sha256_lf']:
            errors.append('PATCH PAYLOAD CORRUPT: '+rel);continue
        if path.exists():
            have=sha(path.read_bytes())
            if have==row['after_sha256_lf']: installed.append(rel)
            elif row['before_sha256_lf'] is not None and have==row['before_sha256_lf']:todo.append(row)
            else:errors.append('LOCAL FILE UNKNOWN: '+rel)
        elif row['before_sha256_lf'] is None:
            todo.append(row)
        else:errors.append('REQUIRED ORIGINAL FILE MISSING: '+rel)
    if errors or (installed and todo):
        print('STOP: Source unchanged. No files installed.')
        for e in errors:print('  -',e)
        if installed and todo:print('  - MIXED/partial patch installation')
        return 2
    if not todo:
        print('PASS: Stage6b already installed, no changes');return 0
    dest=ROOT/('source-backup-before-v4-stage6b-'+datetime.now().strftime('%Y%m%d-%H%M%S'))
    if dest.exists():
        print('STOP: Backup directory already exists.');return 2
    originals={};written=[]
    try:
        for row in todo:
            rel=row['path'];p=ROOT/rel
            originals[rel]=p.read_bytes() if p.exists() else None
            if p.exists():
                target=dest/rel;target.parent.mkdir(parents=True,exist_ok=True)
                target.write_bytes(originals[rel])
                if target.read_bytes()!=originals[rel]:raise RuntimeError('backup not verified: '+rel)
        for row in todo:
            rel=row['path'];p=ROOT/rel
            raw=normal((PAYLOAD/rel).read_bytes())
            if originals[rel] is not None and b'\r\n' in originals[rel]:raw=raw.replace(b'\n',b'\r\n')
            p.parent.mkdir(parents=True,exist_ok=True)
            tmp=p.with_name(p.name+'.stage6b.tmp')
            tmp.write_bytes(raw)
            if sha(tmp.read_bytes())!=row['after_sha256_lf']:
                raise RuntimeError('SHA after temporary write differs: '+rel)
            os.replace(tmp,p);written.append(rel)
    except Exception as exc:
        for rel,old in originals.items():
            p=ROOT/rel
            if old is None:
                if p.exists():p.unlink()
            else:p.write_bytes(old)
        print('STOP: Stage6b failed; originals restored:',exc)
        return 3
    print('PASS: Stage6b RAM-import-before-NOR-guard source installed')
    print('Original files backup:',dest)
    print('SOURCE ONLY – run host tests and target build; DO NOT FLASH YET')
    return 0
if __name__=='__main__':sys.exit(main())
