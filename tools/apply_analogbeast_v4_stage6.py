#!/usr/bin/env python3
"""Stage6 source-only, transactional, SHA-guarded safe RAM-import overlay."""
from pathlib import Path
from datetime import datetime
import hashlib,json,os,shutil,sys
ROOT=Path(__file__).resolve().parent.parent
PAYLOAD=ROOT/'tools'/'analogbeast_v4_stage6_payload'
def norm(b):return b.replace(b'\r\n',b'\n')
def digest(b):return hashlib.sha256(norm(b)).hexdigest()
def main():
    manifest=json.loads((PAYLOAD/'manifest.json').read_text(encoding='utf-8'))
    bad=[];todo=[];already=[]
    for rel,dig in manifest['guards'].items():
        f=ROOT/rel
        if not f.is_file() or digest(f.read_bytes())!=dig:bad.append('GUARD DIFFERS: '+rel)
    brute=ROOT/'firmware/src/eng_golana.c'
    ref=ROOT/'firmware/src/eng_golana.c.dev5-before-fb-fix.bak'
    if not brute.is_file() or not ref.is_file() or brute.read_bytes()!=ref.read_bytes():
        bad.append('BRUTE dev5 byte identity differs')
    for row in manifest['files']:
        rel=row['path'];f=ROOT/rel;p=PAYLOAD/rel
        if not p.is_file() or digest(p.read_bytes())!=row['after_sha256_lf']:
            bad.append('PAYLOAD ERROR: '+rel);continue
        if f.exists():
            got=digest(f.read_bytes())
            if got==row['after_sha256_lf']:already.append(rel)
            elif row['before_sha256_lf'] is not None and got==row['before_sha256_lf']:todo.append(row)
            else:bad.append('SOURCE DIFFERS FROM VERIFIED STAGE5: '+rel)
        elif row['before_sha256_lf'] is None:todo.append(row)
        else:bad.append('MISSING STAGE5 SOURCE: '+rel)
    if bad:
        print('STOP – absolutely no files changed:')
        for e in bad:print(' -',e)
        return 2
    if already and todo:
        print('STOP – mixed installation, nothing changed; inspect files first')
        return 2
    if not todo:
        print('PASS: Stage6 safely installed already');return 0
    stamp=datetime.now().strftime('%Y%m%d-%H%M%S')
    backup=ROOT/('source-backup-before-v4-stage6-'+stamp)
    if backup.exists():print('STOP – backup folder exists');return 2
    originals={}
    try:
        for row in todo:
            rel=row['path'];f=ROOT/rel
            originals[rel]=f.read_bytes() if f.exists() else None
            if f.exists():
                b=backup/rel;b.parent.mkdir(parents=True,exist_ok=True)
                b.write_bytes(originals[rel]);assert b.read_bytes()==originals[rel]
        written=[]
        for row in todo:
            rel=row['path'];f=ROOT/rel;p=PAYLOAD/rel
            raw=norm(p.read_bytes())
            if originals[rel] is not None and b'\r\n' in originals[rel]:raw=raw.replace(b'\n',b'\r\n')
            f.parent.mkdir(parents=True,exist_ok=True)
            tmp=f.with_name(f.name+'.stage6.tmp');tmp.write_bytes(raw)
            if digest(tmp.read_bytes())!=row['after_sha256_lf']:raise RuntimeError('write mismatch: '+rel)
            os.replace(tmp,f);written.append(rel)
    except Exception as e:
        for rel,orig in originals.items():
            f=ROOT/rel
            if orig is None:
                if f.exists():f.unlink()
            else:f.write_bytes(orig)
        print('STOP – rolled back all Stage6 source changes:',e)
        return 3
    print('PASS: analogBEAST v4 Stage6 SAFE RAM-ONLY migration installed')
    print('SOURCE BACKUP:',backup)
    print('SOURCE ONLY – DO NOT FLASH. Run Stage2/3/4/6 host tests and target build.')
    return 0
if __name__=='__main__':sys.exit(main())
