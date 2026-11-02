#!/usr/bin/env python3
"""analogBEAST v1.0 MIDI SysEx 96: import/export Song, Album and A-H Patterns.

Read-only export: export --type song --id 1 --output song1.json
Safe write: import --input import.json --backup before.json --confirm 'WRITE IMPORT'
Device-side copy: copy-song --source 1 --source-start 8 --destination 2
                         --destination-start 1 --count 9 --backup before.json
                         --confirm 'COPY SONG'

Always STOP transport, exit REC and close DAW/MIDI editor before transfer.
No flash erase command; firmware's existing A/B autosave handles persistence.
"""
import argparse
import json
from pathlib import Path
import sys
import time

MAGIC=(0x7d,0x46,0x4c,96)
FIELDS=('note','flags','velocity','gate','swing')
EMPTY={'note':0,'flags':0,'velocity':96,'gate':64,'swing':0}
DEFAULT_INPUT='analogBEAST'


def number(v, lo, hi, name):
    if type(v) is not int or not lo<=v<=hi:
        raise ValueError(f'{name} must be an integer {lo}..{hi}, got {v!r}')
    return v


def row_valid(r):
    if not isinstance(r,list) or len(r)!=4:raise ValueError('song row must be [bank,pattern,play_steps,value]')
    b,p,steps,val=r
    for x,a,z,t in ((b,0,12,'bank'),(p,0,127,'pattern'),(steps,1,64,'play_steps'),(val,0,255,'value')):
        number(x,a,z,t)
    if b<=7:
        if p>=16 or val!=0 or (b>=4 and steps>16):raise ValueError('Invalid pattern row')
    elif b==8:
        if p!=16 or val!=0:raise ValueError('Invalid END row')
    elif p!=0:raise ValueError('Non-pattern command has nonzero pattern')
    elif b==9 and not 40<=val<=240:raise ValueError('Invalid TEMPO')
    elif b==10 and val>=32:raise ValueError('Invalid PRESET')
    elif b==11 and val!=0:raise ValueError('Invalid REST value')
    elif b==12 and val>1:raise ValueError('Invalid ARP value')
    return r


def check_record(rec,kind):
    if not isinstance(rec,dict):raise ValueError(f'{kind}: record must be object')
    if kind=='songs':
        number(rec.get('song'),1,32,'song')
        number(rec.get('count'),1,64,'song count')
        number(rec.get('loop'),0,1,'song loop')
        if not isinstance(rec.get('rows'),list) or len(rec['rows'])!=64:raise ValueError('Song needs exactly 64 rows')
        for r in rec['rows']:row_valid(r)
        return ('song',rec['song'])
    if kind=='albums':
        number(rec.get('album'),1,4,'album')
        count=number(rec.get('count'),0,32,'album count')
        number(rec.get('loop'),0,1,'album loop')
        songs=rec.get('song_ids')
        if not isinstance(songs,list) or len(songs)!=32:raise ValueError('Album needs exactly 32 song_ids')
        for i,id in enumerate(songs):
            if i<count:number(id,1,32,'album song')
            elif id is not None:raise ValueError('Album slots after END must be null')
        return ('album',rec['album'])
    if kind=='patterns':
        b=rec.get('bank');p=rec.get('pattern')
        if b not in tuple('ABCDEFGH'):raise ValueError('Pattern bank A..H required')
        number(p,1,16,'pattern')
        size=64 if b<='D' else 16
        number(rec.get('length'),1,size,'pattern length')
        number(rec.get('resolution'),0,9 if b<='D' else 5,'resolution')
        steps=rec.get('steps')
        if not isinstance(steps,list) or len(steps)!=size:raise ValueError(f'{b}{p:02d} needs {size} steps')
        for st in steps:
            if not isinstance(st,dict) or set(st)!=set(FIELDS):raise ValueError('Five explicit step fields required')
            for fld,lo,hi in [('note',0,127),('flags',0,31),('velocity',1,127),('gate',1,127),('swing',0,100)]:
                number(st[fld],lo,hi,fld)
        return ('pattern',b,p)
    raise ValueError('Unknown resource type')


def parse_document(obj):
    if not isinstance(obj,dict) or obj.get('format')!='analogBEAST-v1.0':
        raise ValueError('Expected format analogBEAST-v1.0')
    out=[]
    for kind in ('patterns','songs','albums'):
        records=obj.get(kind,[])
        if not isinstance(records,list):raise ValueError(kind+' must be an array')
        for rec in records:out.append((check_record(rec,kind),rec))
    ids=[a for a,_ in out]
    if len(ids)!=len(set(ids)):raise ValueError('Duplicate addresses')
    if not ids:raise ValueError('No objects to import')
    return out


def write_new(path,obj):
    path=Path(path)
    with path.open('x',encoding='utf-8') as f:
        json.dump(obj,f,ensure_ascii=False,indent=2);f.write('\n')
    print(f'BACKUP/EXPORT: {path}')


def select_ports(mido,ino,out):
    def pick(names,given,direction):
        if given:
            if given not in names:raise ValueError(f'{direction} port {given!r} not found: {names}')
            return given
        found=[n for n in names if DEFAULT_INPUT.lower() in n.lower()]
        if len(found)!=1:found=[n for n in names if 'felucca' in n.lower()]
        if len(found)!=1:raise ValueError(f'Cannot identify unique {direction} port: {names}')
        return found[0]
    return pick(mido.get_input_names(),ino,'input'),pick(mido.get_output_names(),out,'output')


class Midi:
    def __init__(self,inp=None,out=None):
        try:import mido
        except ImportError as e:raise RuntimeError('Install mido and python-rtmidi first') from e
        self.mido=mido
        namein,nameout=select_ports(mido,inp,out)
        print(f'MIDI: IN={namein} OUT={nameout}')
        self.ip=mido.open_input(namein)
        try:self.op=mido.open_output(nameout)
        except BaseException:self.ip.close();raise
        self.token=0
    def close(self):self.ip.close();self.op.close()
    def request(self,op,data=()):
        if any(type(v)!=int or not 0<=v<=127 for v in data):raise ValueError('Non-7bit MIDI byte')
        self.token=(self.token+1)%128
        for _ in self.ip.iter_pending():pass
        self.op.send(self.mido.Message('sysex',data=(*MAGIC,op,self.token,*data)))
        limit=time.monotonic()+6
        while time.monotonic()<limit:
            for msg in self.ip.iter_pending():
                if msg.type!='sysex':continue
                r=list(msg.data)
                if len(r)<7 or tuple(r[:4])!=MAGIC or r[4]!=op or r[5]!=self.token:continue
                if r[6]:raise RuntimeError(f'MIDI op {op}: device status {r[6]}')
                return r[7:]
            time.sleep(.012)
        raise TimeoutError(f'MIDI timeout on op {op}; check device/ports/STOP')

    def read_song(self,id):
        meta=self.request(28,[id-1]);assert meta[0]==id-1 and len(meta)==3
        rows=[]
        for start in range(0,64,16):
            r=self.request(24,[id-1,start,16])
            if r[:3]!=[id-1,start,16] or len(r)!=83:raise RuntimeError('Bad song read reply')
            for i in range(16):
                b,p,steps,vlo,vhi=r[3+i*5:8+i*5]
                rows.append([b,p,steps,vlo|(vhi<<7)])
        result={'song':id,'count':meta[1],'loop':meta[2],'rows':rows}
        check_record(result,'songs')
        return result

    def write_song(self,rec):
        check_record(rec,'songs')
        song=rec['song']-1
        for start in range(0,64,16):
            data=[song,start,16]
            for b,p,steps,v in rec['rows'][start:start+16]:data.extend([b,p,steps,v&127,v>>7])
            if self.request(23,data)!=[song,start,16]:raise RuntimeError('Song write ACK mismatch')
        if self.request(27,[song,rec['count'],rec['loop']])!=[song,rec['count'],rec['loop']]:
            raise RuntimeError('Song meta ACK mismatch')

    def read_album(self,id):
        r=self.request(26,[id-1])
        if len(r)!=35 or r[0]!=id-1:raise RuntimeError('Invalid album reply')
        rec={'album':id,'count':r[1],'loop':r[2],
             'song_ids':[v+1 if v!=127 else None for v in r[3:]]}
        check_record(rec,'albums')
        return rec

    def write_album(self,rec):
        check_record(rec,'albums')
        r=[rec['album']-1,rec['count'],rec['loop']]+[v-1 if v is not None else 127 for v in rec['song_ids']]
        if self.request(25,r)!=r[:3]:raise RuntimeError('Album write ACK mismatch')

    def read_pattern(self,bank,pat):
        b=ord(bank)-65;p=pat-1
        if b>=4:
            r=self.request(22,[b,p])
            if len(r)!=84 or r[:2]!=[b,p]:raise RuntimeError('Bad E-H read')
            length,res=r[2:4]
            data=r[4:]
        else:
            meta=self.request(17,[b,p]);
            if len(meta)!=4 or meta[:2]!=[b,p]:raise RuntimeError('Bad A-D metadata')
            length,res=meta[2:4];data=[]
            for start in range(0,64,16):
                r=self.request(15,[b,p,start,16])
                if len(r)!=84 or r[:4]!=[b,p,start,16]:raise RuntimeError('Bad A-D steps')
                data.extend(r[4:])
        rec={'bank':bank,'pattern':pat,'length':length,'resolution':res,
             'steps':[dict(zip(FIELDS,data[i:i+5])) for i in range(0,len(data),5)]}
        check_record(rec,'patterns')
        return rec

    def write_pattern(self,rec):
        check_record(rec,'patterns')
        b=ord(rec['bank'])-65;p=rec['pattern']-1
        stepdata=[v for st in rec['steps'] for v in (st[k] for k in FIELDS)]
        if b>=4:
            if self.request(21,[b,p,rec['length'],rec['resolution'],*stepdata])!=[b,p]:
                raise RuntimeError('E-H write ACK mismatch')
        else:
            for start in range(0,64,16):
                r=[b,p,start,16,*stepdata[start*5:(start+16)*5]]
                if self.request(14,r)!=[b,p,start,16]:raise RuntimeError('A-D step ACK mismatch')
            if self.request(16,[b,p,rec['length'],rec['resolution']])!=[b,p,rec['length'],rec['resolution']]:
                raise RuntimeError('A-D meta ACK mismatch')

    def read_key(self,key):
        if key[0]=='song':return self.read_song(key[1])
        if key[0]=='album':return self.read_album(key[1])
        return self.read_pattern(key[1],key[2])
    def write_key(self,key,rec):
        if key[0]=='song':return self.write_song(rec)
        if key[0]=='album':return self.write_album(rec)
        return self.write_pattern(rec)

    def wait_saved(self,v4,v5,timeout=45):
        end=time.monotonic()+timeout
        while time.monotonic()<end:
            if v4:
                state=self.request(6)
                if len(state)!=2 or state[0]>1 or state[1]>1:raise RuntimeError('Invalid v4 save reply')
                if state[0]:time.sleep(.3);continue
            if v5:
                state=self.request(30)
                if len(state)!=5:raise RuntimeError('No v5 persistence status')
                dirty,error,enabled,valid,boot=state
                if error or not enabled:raise RuntimeError(f'v5 NOR save failed: {state}')
                if dirty or not valid:time.sleep(.5);continue
            print('PASS: device reports autosave complete')
            return
        raise TimeoutError('Device did not confirm persistence; do not power off; recheck/backup')


def make_doc(entries):
    result={'format':'analogBEAST-v1.0','patterns':[],'songs':[],'albums':[]}
    for key,rec in entries:
        kind={'pattern':'patterns','song':'songs','album':'albums'}[key[0]]
        result[kind].append(rec)
    return result


def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__,formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--in-port');p.add_argument('--out-port')
    sub=p.add_subparsers(dest='cmd',required=True)
    exp=sub.add_parser('export',help='read-only export; --type all exports everything')
    exp.add_argument('--type',choices=['all','song','album','pattern'],required=True)
    exp.add_argument('--id',type=int)
    exp.add_argument('--bank',choices=list('ABCDEFGH'))
    exp.add_argument('--pattern',type=int)
    exp.add_argument('--output',type=Path,required=True)
    imp=sub.add_parser('import',help='preview or confirmed upload of a JSON export')
    imp.add_argument('--input',type=Path,required=True)
    imp.add_argument('--preview',action='store_true')
    imp.add_argument('--backup',type=Path)
    imp.add_argument('--confirm',default='')
    cp=sub.add_parser('copy-song',help='copy song row range on device without round-trip writes')
    cp.add_argument('--source',type=int,required=True);cp.add_argument('--source-start',type=int,required=True)
    cp.add_argument('--destination',type=int,required=True);cp.add_argument('--destination-start',type=int,required=True)
    cp.add_argument('--count',type=int,required=True);cp.add_argument('--backup',type=Path,required=True)
    cp.add_argument('--confirm',default='')
    args=p.parse_args(argv)
    if args.cmd=='import':
        obj=json.loads(args.input.read_text(encoding='utf-8'))
        items=parse_document(obj)
        print('PREVIEW:',*[str(x[0]) for x in items],sep='\n  ')
        if args.preview:return 0
        if args.confirm!='WRITE IMPORT' or not args.backup:p.error('Requires --backup and --confirm "WRITE IMPORT"')
    elif args.cmd=='copy-song':
        number(args.source,1,32,'source');number(args.destination,1,32,'destination')
        number(args.source_start,1,64,'source-start');number(args.destination_start,1,64,'destination-start')
        number(args.count,1,64,'count')
        if args.source_start+args.count>65 or args.destination_start+args.count>65:p.error('Copy exceeds 64 song rows')
        if args.confirm!='COPY SONG':p.error('Requires --confirm "COPY SONG"')
    else:
        if args.type=='all':keys=[('pattern',b,p) for b in 'ABCDEFGH' for p in range(1,17)]+[('song',i) for i in range(1,33)]+[('album',i) for i in range(1,5)]
        elif args.type=='song':keys=[('song',number(args.id,1,32,'id'))]
        elif args.type=='album':keys=[('album',number(args.id,1,4,'id'))]
        else:
            if not args.bank:p.error('--bank required for pattern export')
            keys=[('pattern',args.bank,number(args.pattern,1,16,'pattern'))]
    destination=args.backup if args.cmd!='export' else args.output
    if destination.exists():p.error(f'File already exists: {destination}. Backup never overwritten.')
    midi=Midi(args.in_port,args.out_port)
    try:
        if args.cmd=='export':
            items=[(k,midi.read_key(k)) for k in keys]
            write_new(args.output,make_doc(items));return 0
        if args.cmd=='import':
            before=[(key,midi.read_key(key)) for key,_ in items]
            write_new(args.backup,make_doc(before))
            for key,rec in items:
                midi.write_key(key,rec)
                if midi.read_key(key)!=rec:raise RuntimeError(f'Readback mismatch: {key}')
                print('VERIFIED',key)
            midi.wait_saved(any(k[0]=='pattern' and k[1]<='D' for k,_ in items),
                            any(k[0]!='pattern' or k[1]>='E' for k,_ in items))
            return 0
        src=midi.read_song(args.source);dst=midi.read_song(args.destination)
        if args.source_start+args.count-1>src['count']:
            raise ValueError('Requested source steps exceed active Song count')
        write_new(args.backup,make_doc([(('song',args.source),src)] if args.source==args.destination
                                       else [(('song',args.source),src),(('song',args.destination),dst)]))
        request=[args.source-1,args.source_start-1,args.destination-1,args.destination_start-1,args.count]
        if midi.request(29,request)!=request:raise RuntimeError('Copy ACK mismatch')
        actual=midi.read_song(args.destination)
        expected=dst['rows'][:]
        # Support overlapping copy within the same song using pre-copy snapshot.
        expected[args.destination_start-1:args.destination_start-1+args.count]=src['rows'][args.source_start-1:args.source_start-1+args.count]
        if actual['rows']!=expected or actual['count']!=max(dst['count'],args.destination_start-1+args.count):
            raise RuntimeError('Copy readback mismatch; check backup before any retry')
        midi.wait_saved(False,True)
        print(f'PASS: SONG {args.source} {args.source_start}..{args.source_start+args.count-1} '
              f'-> SONG {args.destination} {args.destination_start}..{args.destination_start+args.count-1}')
        return 0
    finally:midi.close()


if __name__=='__main__':
    try:sys.exit(main())
    except (OSError,ValueError,RuntimeError,TimeoutError,ImportError,AssertionError) as e:
        print('ERROR:',e,file=sys.stderr);sys.exit(2)
