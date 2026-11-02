#!/usr/bin/env python3
"""READ-ONLY backup of analogBEAST ab3.5 banks A-D + 128-row song.

This tool never sends command 96 with write opcodes. It does NOT erase or
initialize sample slots. Requires mido and python-rtmidi.

PowerShell:
  py tools\analogbeast_backup_v3.py --in-port "Felucca 3" --out-port "Felucca 4"
"""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import sys
import time

MAGIC=[0x7D,0x46,0x4C]
READ_ONLY_OPS={0,3,5,8}

class FM1:
    def __init__(self,in_name,out_name):
        import mido
        self.mido=mido
        inputs=mido.get_input_names(); outputs=mido.get_output_names()
        if not in_name:
            p=[n for n in inputs if 'felucca' in n.lower()]
            if len(p)!=1:raise RuntimeError(f'Eingang unklar: {inputs}; --in-port angeben')
            in_name=p[0]
        if not out_name:
            p=[n for n in outputs if 'felucca' in n.lower()]
            if len(p)!=1:raise RuntimeError(f'Ausgang unklar: {outputs}; --out-port angeben')
            out_name=p[0]
        if in_name not in inputs or out_name not in outputs:
            raise RuntimeError(f'Port nicht vorhanden: IN {in_name}, OUT {out_name}')
        self.inp=mido.open_input(in_name)
        try:self.out=mido.open_output(out_name)
        except Exception:self.inp.close();raise
        self.token=0
    def close(self):self.inp.close();self.out.close()
    def dz(self,op,values=()):
        if op not in READ_ONLY_OPS:raise RuntimeError(f'WRITE OP {op} forbidden in backup tool')
        values=list(values)
        if any(not 0<=x<=127 for x in values):raise ValueError('SysEx values must be 7bit')
        token=self.token;self.token=(token+1)%128
        for attempt in range(4):
            for _ in self.inp.iter_pending():pass
            self.out.send(self.mido.Message('sysex',data=MAGIC+[96,op,token]+values))
            deadline=time.monotonic()+4.0
            while time.monotonic()<deadline:
                for msg in self.inp.iter_pending():
                    if msg.type!='sysex':continue
                    p=list(msg.data)
                    if len(p)<7 or p[:6]!=MAGIC+[96,op,token]:continue
                    if p[6]!=0:raise RuntimeError(f'FM1 read op {op} error {p[6]}')
                    return p[7:]
                time.sleep(0.006)
        raise TimeoutError(f'FM1 antwortet nicht auf op {op}, Port/STOP prüfen')

def collect(dev):
    info=dev.dz(0)
    if len(info)<7 or info[0]!=3 or info[5]!=64 or info[6]!=0:
        raise RuntimeError(f'Erwartet analogBEAST v3, nicht: {info}')
    meta=dev.dz(8)
    if len(meta)!=3:raise RuntimeError('Song metadata invalid')
    patterns=[]
    for bank in range(4):
        for pat in range(16):
            data=dev.dz(3,[bank,pat])
            if len(data)!=129 or not 1<=data[0]<=64:
                raise RuntimeError(f'Pattern {bank} {pat} response invalid (len={len(data)})')
            steps=[[data[1+2*i],data[2+2*i]] for i in range(64)]
            if any(n>127 or f>31 for n,f in steps):raise RuntimeError('Corrupt step note/flags')
            patterns.append({'bank':'ABCD'[bank],'pattern':pat+1,'length':data[0],'steps':steps})
        print(f'Gelesen: Bank {"ABCD"[bank]} (16 Patterns)')
    rows=[]
    for offset in range(0,128,24):
        count=min(24,128-offset)
        ret=dev.dz(5,[offset,count])
        if len(ret)!=2+5*count or ret[:2]!=[offset,count]:raise RuntimeError(f'Invalid song chunk {offset}')
        for i in range(count):
            kind,pat,length,lo,hi=ret[2+5*i:7+5*i]
            rows.append([kind,pat,length,lo|(hi<<7)])
    if dev.dz(8)!=meta:raise RuntimeError('Song changed while reading: repeat with STOP')
    # Independently reread bank A01 and D16 to detect transport / mid-read changes.
    for bank,pat,index in ((0,0,0),(3,15,63)):
        data=dev.dz(3,[bank,pat])
        old=patterns[index]
        if len(data)!=129 or data[0]!=old['length'] or any(list(data[1+2*i:3+2*i])!=old['steps'][i] for i in range(64)):
            raise RuntimeError('Bank changed during backup; do not trust snapshot')
    return {'format':'analogBEAST-v3-read-only','timestamp_utc':datetime.now(timezone.utc).isoformat(),
            'firmware_info':info,'song_meta':meta,'patterns':patterns,'song_rows':rows,
            'notes':'READ ONLY. No NOR writes. Restore on v4 requires separate tested importer.'}

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--in-port');ap.add_argument('--out-port')
    ap.add_argument('--output',type=Path,help='New JSON filename (must not exist)')
    args=ap.parse_args()
    dest=args.output or Path(f'analogBEAST-bank-backup-{datetime.now().strftime("%Y%m%d-%H%M%S")}.json')
    if dest.exists():raise RuntimeError(f'Refusing to overwrite backup: {dest}')
    print('READ ONLY – 64 patterns and 128 song rows. Bitte PLAY STOP und REC verlassen.')
    d=FM1(args.in_port,args.out_port)
    try:backup=collect(d)
    finally:d.close()
    # One atomic rename; incomplete file is never claimed as a verified backup.
    temp=dest.with_name(dest.name+'.partial')
    if temp.exists():raise RuntimeError(f'Partial backup file already exists: {temp}')
    try:
        temp.write_text(json.dumps(backup,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
        probe=json.loads(temp.read_text(encoding='utf-8'))
        if len(probe['patterns'])!=64 or len(probe['song_rows'])!=128:raise RuntimeError('Backup verify failed')
        temp.rename(dest)
    except Exception:
        temp.unlink(missing_ok=True)
        raise
    print(f'PASS: {dest} | 64 Patterns, 128 Songzeilen, READ-ONLY, verifiziert')

if __name__=='__main__':
    try:main()
    except (RuntimeError,TimeoutError,ValueError,OSError,EOFError) as e:sys.exit('ERROR: '+str(e))
