#!/usr/bin/env python3
"""analogBEAST v4 full read-only backup over USB-MIDI (64 x 64 steps and 256 song rows).

All operations used here are protocol GET commands; nothing is written or erased.
Use after v4 Stage3 is fully built/flashed/validated, NOT against ab3.5.
"""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
from analogbeast_import_songs_v4 import FM1


def collect(dev):
    meta = dev.dz(13)
    if len(meta) != 4 or not (1 <= meta[0] + 128*meta[1] <= 256):
        raise RuntimeError('Ungültige 256er-Songmetadaten')
    result = {'format':'analogBEAST-v4-read-only',
              'timestamp_utc':datetime.now(timezone.utc).isoformat(),
              'firmware_info':dev.dz(0),
              'song_meta':meta,
              'patterns':[], 'song':[]}
    for bank in range(4):
        for pattern in range(16):
            header=dev.dz(17,[bank,pattern])
            if len(header)!=4 or header[:2]!=[bank,pattern]:
                raise RuntimeError(f'Pattern-Header falsch: {bank}:{pattern}')
            steps=[]
            for start in range(0,64,16):
                h=[bank,pattern,start,16]
                chunk=dev.dz(15,h)
                if chunk[:4]!=h or len(chunk)!=4+16*5:
                    raise RuntimeError(f'Step-Readback falsch: {bank}:{pattern}:{start}')
                steps.extend([chunk[4+i*5:9+i*5] for i in range(16)])
            if dev.dz(17,[bank,pattern])!=header:
                raise RuntimeError('Pattern während Sicherung verändert')
            result['patterns'].append({'bank':'ABCD'[bank], 'pattern':pattern+1,
                                       'length':header[2], 'resolution':header[3], 'steps':steps})
        print(f'Gelesen: Bank {"ABCD"[bank]} (16 Patterns)')
    for start in range(0,256,24):
        count=min(24,256-start)
        addr=[start&127,start>>7,count]
        chunk=dev.dz(11,addr)
        if chunk[:3]!=addr or len(chunk)!=3+5*count:
            raise RuntimeError(f'Arranger-Readback falsch bei {start+1}')
        for i in range(count):
            bank,pat,steps,lo,hi=chunk[3+i*5:8+i*5]
            result['song'].append([bank,pat,steps,lo|(hi<<7)])
    if dev.dz(13)!=meta:
        raise RuntimeError('Arranger während Sicherung verändert')
    assert len(result['patterns'])==64 and len(result['song'])==256
    return result


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--in-port');ap.add_argument('--out-port');ap.add_argument('--out',type=Path)
    args=ap.parse_args()
    import mido
    dev=FM1(mido,args.in_port,args.out_port)
    try:
        dev.fw_check(require_flash=False)
        print('READ ONLY: Gerät stoppen, REC verlassen, DAW und Web-Editor schließen')
        result=collect(dev)
    finally:dev.close()
    out=args.out or Path('analogBEAST-v4-bank-backup-'+datetime.now().strftime('%Y%m%d-%H%M%S')+'.json')
    if out.exists():raise FileExistsError(f'Zieldatei existiert schon: {out}')
    out.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(f'PASS: {out} | 64 Patterns, 4096 Steps, 256 Arranger-Zeilen | READ ONLY')

if __name__=='__main__':main()
