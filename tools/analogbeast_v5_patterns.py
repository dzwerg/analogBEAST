#!/usr/bin/env python3
"""analogBEAST v5 E-H 16-step export/import (SysEx 96, op 21/22).

JSON schema: {"patterns":[{"bank":"E", "pattern":1, "length":16,
"resolution":2, "steps":[{"note":60, "flags":0, "velocity":96,
"gate":64, "swing":0}, ... 16 entries]}]}.
This does not claim to parse Donners proprietary RSEQ/BRSEQ yet.
"""
import argparse
import json
import sys
import time
from pathlib import Path

FIVE = ("note", "flags", "velocity", "gate", "swing")
D = {"note":0, "flags":0, "velocity":96, "gate":64, "swing":0}


def valid_pat(p):
    try:
        bank=p["bank"]
        if not isinstance(bank,str) or len(bank)!=1 or bank not in "EFGH": raise ValueError("bank must be E..H")
        pos=int(p["pattern"])
        if not 1<=pos<=16: raise ValueError("pattern must be 1..16")
        length=int(p.get("length",16))
        res=int(p.get("resolution",2))
        if not 1<=length<=16 or not 0<=res<=5: raise ValueError("bad pattern length/resolution")
        steps=p["steps"]
        if len(steps)!=16: raise ValueError(f"{bank}{pos:02d}: exactly 16 step entries required")
        normalized=[]
        for step in steps:
            merged={k:int(step.get(k,D[k])) for k in FIVE}
            if not (0<=merged['note']<=127 and 0<=merged['flags']<=31 and
                    1<=merged['velocity']<=127 and 1<=merged['gate']<=127 and
                    0<=merged['swing']<=100): raise ValueError(f"invalid step in {bank}{pos:02d}")
            normalized.append(merged)
        return {"bank":bank,"pattern":pos,"length":length,"resolution":res,"steps":normalized}
    except (KeyError,TypeError,ValueError) as exc:
        raise ValueError(f"Invalid pattern record {p!r}: {exc}") from exc


def payload(p):
    v=[ord(p['bank'])-ord('A'),p['pattern']-1,p['length'],p['resolution']]
    for step in p['steps']:v.extend(step[k] for k in FIVE)
    assert len(v)==84 and all(0<=x<=127 for x in v)
    return v


def unpack(v):
    if len(v)!=84 or not 4<=v[0]<=7 or v[1]>=16:
        raise ValueError('Unexpected device reply')
    p={"bank":chr(v[0]+65), "pattern":v[1]+1, "length":v[2], "resolution":v[3],
       "steps":[dict(zip(FIVE,v[4+j*5:9+j*5])) for j in range(16)]}
    return valid_pat(p)


def read_input(path):
    data=json.loads(path.read_text(encoding='utf-8'))
    if not isinstance(data,dict) or not isinstance(data.get('patterns'),list):
        raise ValueError('JSON must have a patterns array')
    ps=[valid_pat(p) for p in data['patterns']]
    keys=[(p['bank'],p['pattern']) for p in ps]
    if len(set(keys))!=len(keys):raise ValueError('Duplicate E-H pattern addresses')
    return ps


class Midi:
    def __init__(self, in_name, out_name):
        import mido
        self.mido=mido
        self.ip=mido.open_input(in_name)
        self.op=mido.open_output(out_name)
        self.token=0

    def close(self):
        self.ip.close();self.op.close()

    def request(self, op, args=(), timeout=6):
        self.token=(self.token+1)%128
        for _ in self.ip.iter_pending(): pass  # flush old replies
        self.op.send(self.mido.Message('sysex',data=(0x7D,0x46,0x4C,96,op,self.token,*args)))
        t=time.monotonic()+timeout
        while time.monotonic()<t:
            for msg in self.ip.iter_pending():
                if msg.type!='sysex':continue
                d=tuple(msg.data)
                if len(d)>=8 and d[:5]==(0x7D,0x46,0x4C,96,op) and d[5]==self.token:
                    if d[6]:raise RuntimeError(f"FM-1 op{op} failed, status {d[6]}")
                    return list(d[7:])
            time.sleep(0.012)
        raise TimeoutError(f"No matching reply to op{op}; check MIDI port names/cables")

    def get(self,bank,n):return unpack(self.request(22,[ord(bank)-65,n-1]))

    def put(self,p):
        r=self.request(21,payload(p))
        if r != [ord(p['bank'])-65,p['pattern']-1]:
            raise RuntimeError('Unexpected write acknowledgement')
        result=self.get(p['bank'],p['pattern'])
        if result!=p:raise RuntimeError(f"Readback mismatch at {p['bank']}{p['pattern']:02d}")


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--input',type=Path,help='Canonical JSON with patterns array')
    ap.add_argument('--preview',action='store_true',help='Validate input only; no MIDI')
    ap.add_argument('--export',type=Path,help='Read all 64 E-H patterns to JSON; NO writes')
    ap.add_argument('--write',action='store_true',help='Perform validated upload (requires --backup/--confirm)')
    ap.add_argument('--backup',type=Path,help='Mandatory read-only backup of all existing E-H patterns before writing')
    ap.add_argument('--confirm',default='',help='Exact string "WRITE E-H" required for uploads')
    ap.add_argument('--in-port',default='analogBEAST 3')
    ap.add_argument('--out-port',default='analogBEAST 4')
    opt=ap.parse_args()
    if opt.write:
        if not opt.input or opt.confirm!='WRITE E-H' or not opt.backup:
            ap.error('--write requires --input, --backup and --confirm "WRITE E-H"')
        if opt.backup.exists():ap.error('Backup file already exists: choose a new name')
    if not (opt.input or opt.export):ap.error('provide --input or --export')
    ps=read_input(opt.input) if opt.input else []
    print(f'Validated {len(ps)} source patterns')
    if opt.preview:
        for p in ps:print(f"  {p['bank']}{p['pattern']:02d}: length {p['length']} / {len(p['steps'])} steps")
        return 0
    if not (opt.write or opt.export):
        print('No writes requested. Add --preview, --export, or --write.');return 0
    midi=Midi(opt.in_port,opt.out_port)
    try:
        if opt.write or opt.export:
            before=[midi.get(chr(b+65), n) for b in range(4,8) for n in range(1,17)]
            export_path=opt.backup if opt.write else opt.export
            export_path.write_text(json.dumps({'patterns':before},indent=2)+'\n',encoding='utf-8')
            print(f'Read-only backup saved to {export_path} (64 patterns)')
        if opt.write:
            for k,p in enumerate(ps,1):
                midi.put(p)
                print(f"  VERIFIED {k}/{len(ps)}: {p['bank']}{p['pattern']:02d}")
            print('All RAM write/readback checks passed. Wait for device v5 NOR autosave before power-off.')
        return 0
    finally:
        midi.close()


if __name__=='__main__':
    try:sys.exit(main())
    except (OSError,ValueError,RuntimeError,TimeoutError,ImportError) as exc:
        print(f'ERROR: {exc}',file=sys.stderr)
        sys.exit(2)
