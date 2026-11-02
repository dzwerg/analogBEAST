#!/usr/bin/env python3
"""Brahms + Musette -> analogBEAST v4 BANK A01..A08; 256-row SONG upload/readback.

Requires firmware v4 Stage3 editor command 96 protocol 5, mido, python-rtmidi.
--preview is safe without hardware; --read only queries the FM-1 (no writing).
--send always verifies written banks and song.
No old Golana P24..P31, no Program Change, no sound preset/flash-sector erase.
"""
from __future__ import annotations
import argparse
from collections import Counter, defaultdict
from dataclasses import dataclass
import math
from pathlib import Path
import sys
import time

HERE=Path(__file__).resolve().parent
MAGIC=[0x7D,0x46,0x4C]
SYSEX_DZ=96
G_BPM,G_CLOCK=0,2
P_SDIV,P_SSWING,P_SGATE=30,31,32
SONG_END,SONG_TEMPO,SONG_PRESET,SONG_REST,SONG_ARP=4,5,6,7,8
BPM_LIMIT=40,240

@dataclass
class Piece:
    key:str
    name:str
    path:Path
    gate:int
    startpat:int
    count:int
    bpm:int=0
    signature:str=''
    notes_count:int=0
    grids:list|None=None

def midi_file(name):
    preferred=HERE/'song_data'/name
    return preferred if preferred.is_file() else HERE.parent/'analogBEAST-v4-stage3-MIDI256'/'reference-midi'/name
PIECES=[
    Piece('brahms','Brahms Wiegenlied', midi_file('Brahms_Wiegenlied.mid'),122,0,3),
    Piece('musette','Musette BWV Anh. 126', midi_file('Musette_BWV_Anh126_Bonus.mid'),105,3,5),
]

def load_piece(p):
    import mido
    midi=mido.MidiFile(str(p.path))
    tpq=midi.ticks_per_beat
    if tpq<=0 or len(midi.tracks)<2: raise ValueError(p.name+': MIDI-Spuren/PPQ fehlen')
    events=[];meta=[]
    for tr in midi.tracks:
        pos=0
        for msg in tr:
            pos+=msg.time
            if msg.type in ('set_tempo','time_signature'):
                meta.append((pos,msg))
    tempo=[(t,m.tempo) for t,m in meta if m.type=='set_tempo']
    sign=[(t,m.numerator,m.denominator) for t,m in meta if m.type=='time_signature']
    if any(t!=0 or v!=tempo[0][1] for t,v in tempo):
        raise ValueError(p.name+': wechselndes MIDI-Tempo, hier nicht stillschweigend quantisieren')
    if any(t!=0 or (num,den)!=(sign[0][1],sign[0][2]) for t,num,den in sign):
        raise ValueError(p.name+': wechselnde Taktart')
    p.bpm=round(60000000/(tempo[0][1] if tempo else 500000))
    if not BPM_LIMIT[0]<=p.bpm<=BPM_LIMIT[1]: raise ValueError('BPM außerhalb Firmwarebereich')
    p.signature=f'{sign[0][1]}/{sign[0][2]}' if sign else '?'
    pos=0;active=defaultdict(list)
    for msg in midi.tracks[1]:
        pos+=msg.time
        if msg.type=='note_on' and msg.channel==0 and msg.velocity:
            active[msg.note].append((pos,msg.velocity))
        elif msg.type in ('note_off','note_on') and getattr(msg,'channel',-1)==0:
            if not active[msg.note]: raise ValueError(f'Ungepaartes note_off bei {pos}')
            st,vel=active[msg.note].pop(0)
            events.append((st,pos,msg.note,vel))
    if any(active.values()): raise ValueError('Unbeendete MIDI-Note')
    events.sort()
    if not events: raise ValueError('MIDI-Melodie leer')
    grid=[[(0,0,96,p.gate,0) for _ in range(64)] for _ in range(p.count)]
    last=-1
    for st,en,note,vel in events:
        x=st/(tpq/4)
        start=round(x)
        if abs(x-start)>0.04: raise ValueError(f'Off-grid MIDI-Note bei Tick {st}')
        raw_steps=(en-st)/(tpq/4)
        dur=round(raw_steps*128/p.gate)
        if dur<1 or abs(raw_steps-dur*p.gate/128)>0.04:
            raise ValueError(f'Notenende bei Tick {en} passt nicht zum Gate {p.gate}/128')
        if start<last: raise ValueError('Polyphonie/überlappende Noten in monophoner Melodie')
        if start+dur>64*p.count: raise ValueError('Melodie überschreitet vorgesehene Patterns')
        if start//64 != (start+dur-1)//64:
            raise ValueError('TIE über Pattern-Grenze; Erweiterung erforderlich')
        for j in range(dur):
            idx=start+j
            bank=idx//64; step=idx%64
            if grid[bank][step]!=(0,0,96,p.gate,0):raise ValueError('Notenkollision im Step-Raster')
            grid[bank][step]=(note,0,vel,p.gate,0) if j==0 else (0,4,vel,p.gate,0)
        last=start+dur
    p.notes_count=len(events)
    p.grids=grid
    if not any(c for block in grid for c,_,_,_,_ in block):raise ValueError('Leere Melodie')
    return p

def song_rows(p, preset=None):
    rows=[]
    if preset is not None: rows.append((SONG_PRESET,0,1,preset-1))
    rows.append((SONG_TEMPO,0,1,p.bpm))
    for i in range(p.count): rows.append((0,p.startpat+i,64,0))
    rows.append((SONG_END,16,1,0))
    return rows

def preview(pieces,selected,preset):
    print('analogBEAST – direkte BANK/PATTERN-Übertragung, KEINE Golana-Programmplätze')
    for p in pieces:
        print(f'\n{p.name}: {p.bpm} BPM; {p.signature}; {p.notes_count} Noten; GATE {p.gate}/128')
        for i,grid in enumerate(p.grids):
            c=Counter('NOTE' if note else 'TIE' if flags&4 else 'REST' for note,flags,velo,gate,swing in grid)
            print(f'  A{p.startpat+i+1:02d}: {c["NOTE"]:3d} NOTE, {c["TIE"]:3d} TIE, {c["REST"]:3d} REST | 64 STEPS')
    p=next(p for p in pieces if p.key==selected)
    print(f'\nAktive SONG-Liste bei --send --song {selected}:')
    for i,(kind,pat,steps,val) in enumerate(song_rows(p,preset),1):
        label= f'A{pat+1:02d}' if kind==0 else ('END' if kind==4 else f'TEMPO {val}' if kind==5 else f'PRESET P{val+1:02d}' if kind==SONG_PRESET else f'ARP {"ON" if val else "OFF"}')
        print(f'  {i:03}: {label}' + (f' | PLAY STEPS {steps}' if kind==0 else ''))
    print('\nWichtig: Die Firmware hat EINE aktive Songliste. Beide Lieder liegen gleichzeitig in A01-A08;')
    print('mit --song brahms bzw. --song musette wird die jeweils gewünschte Songliste eingesetzt.')

class FM1:
    def __init__(self,mido,inname,outname):
        inports=mido.get_input_names();outports=mido.get_output_names()
        if not inname:
            found=[p for p in inports if 'felucca' in p.lower()]
            if len(found)!=1: raise RuntimeError(f'FM-1 Antwort-Port nicht eindeutig: {inports}; --in-port angeben')
            inname=found[0]
        if not outname:
            found=[p for p in outports if 'felucca' in p.lower()]
            if len(found)!=1: raise RuntimeError(f'FM-1 Sende-Port nicht eindeutig: {outports}; --out-port angeben')
            outname=found[0]
        if inname not in inports or outname not in outports: raise RuntimeError('Felucca-USB-MIDI-Port nicht gefunden')
        self.mido=mido; self.inname=inname;self.outname=outname;self.token=0
        self.inp=mido.open_input(inname)
        try: self.out=mido.open_output(outname)
        except Exception:
            self.inp.close();raise
        print(f'USB MIDI: Eingang {inname!r}, Ausgang {outname!r}')
    def close(self):self.inp.close();self.out.close()
    def request(self,cmd,args,matcher,attempts=3):
        for attempt in range(attempts):
            for _ in self.inp.iter_pending():pass
            self.out.send(self.mido.Message('sysex',data=MAGIC+[cmd]+list(args)))
            deadline=time.monotonic()+3.5
            while time.monotonic()<deadline:
                for msg in self.inp.iter_pending():
                    if msg.type=='sysex':
                        a=list(msg.data)
                        if len(a)>=4 and a[:4]==MAGIC+[cmd] and matcher(a[4:]):return a[4:]
                time.sleep(0.006)
        raise TimeoutError(f'USB-MIDI Antwort fehlt cmd={cmd}; STOP / MIDI-Editor schließen / Ports prüfen')
    def dz(self,op,args=()):
        token=self.token;self.token=(self.token+1)%128
        p=self.request(96,[op,token]+list(args),lambda v: len(v)>=3 and v[:2]==[op,token])
        if p[2]:
            reasons={1:'ungültige Daten',2:'Gerät beschäftigt – STOP / REC beenden',3:'RAM-only/kein Flash',4:'Flashfehler'}
            raise RuntimeError(f'FM-1 Kommando {op} meldet {p[2]}: '+reasons.get(p[2],'unbekannt'))
        return p[3:]
    def parameter(self,scope,pid,val):
        v=val+8192
        self.request(3,[scope,pid,v&127,v>>7],lambda p: len(p)>=4 and p[:2]==[scope,pid] and ((p[2]|p[3]<<7)-8192)==val)
        ret=self.request(2,[scope,pid],lambda p:len(p)>=4 and p[:2]==[scope,pid])
        if ((ret[2]|ret[3]<<7)-8192)!=val:raise RuntimeError('Parameter-Readback fehlgeschlagen')
    def fw_check(self,require_flash=True):
        info=self.dz(0)
        if len(info)<10 or info[0]!=5 or info[5]!=64 or not (info[7]&2) or info[8:10]!=[0,2]:
            raise RuntimeError('Erfordert analogBEAST v4 Stage 3 mit Step-/256er-MIDI-Protokoll: '+repr(info))
        if not info[1] and require_flash:
            raise RuntimeError('NOR-Flash nicht freigegeben. Keine Sample-Slots automatisch löschen!')
        print('PASS: analogBEAST v4 Protokoll 5; 64 Patterns x 64 Steps; 256 Songzeilen; '+
              ('NOR aktiv' if info[1] else 'RAM-only'))
    def read_song(self, all_rows=False):
        """Reads only op13 and op11, no write or erase. Limits: 256 song rows."""
        meta=self.dz(13)
        if len(meta)!=4 or meta[2] not in (0,1) or meta[3] not in (0,1):
            raise RuntimeError('Ungültige v4-Song-Metadaten: '+repr(meta))
        used=meta[0]|(meta[1]<<7)
        if used<1 or used>256:raise RuntimeError('Ungültige Songlänge: '+str(used))
        requested=256 if all_rows else used
        rows=[]
        for offset in range(0,requested,24):
            count=min(24,requested-offset)
            address=[offset&127,offset>>7,count]
            ret=self.dz(11,address)
            if ret[:3]!=address or len(ret)!=3+5*count:
                raise RuntimeError(f'Song-Readback ab Zeile {offset+1} fehlerhaft')
            for i in range(count):
                kind,pat,steps,lo,hi=ret[3+5*i:8+5*i]
                rows.append((kind,pat,steps,lo|(hi<<7)))
        if self.dz(13)!=meta:raise RuntimeError('Song-Metadaten während Readback geändert')
        print(f'\nSongmode: {"ON" if meta[3] else "OFF"} | PLAY: {"LOOP" if meta[2] else "ONCE"} | {used}/256')
        for i,(kind,pat,steps,val) in enumerate(rows,1):
            if 0<=kind<=3: desc=f'{"ABCD"[kind]}{pat+1:02d} ({steps} Steps)'
            elif kind==SONG_END: desc='END'
            elif kind==SONG_TEMPO: desc=f'TEMPO {val}'
            elif kind==SONG_PRESET: desc=f'PRESET P{val+1:02d}'
            elif kind==SONG_REST: desc=f'REST ({steps} Steps)'
            elif kind==SONG_ARP: desc=f'ARP {"ON" if val else "OFF"}'
            else: desc=f'UNBEKANNT kind={kind}, pattern={pat}, value={val}'
            print(f'  {i:03d}: {desc}')
        print('READ ONLY – keine Daten verändert')
        return meta,rows
    def put_pattern(self,index,grid):
        if not 0<=index<16 or len(grid)!=64:raise ValueError('Ungültiges A-Pattern')
        for start in range(0,64,16):
            chunk=grid[start:start+16]
            values=[0,index,start,len(chunk)]
            for note,flags,velo,gate,swing in chunk:
                values.extend((note,flags,velo,gate,swing))
            ack=self.dz(14,values)
            if ack!=values[:4]:raise RuntimeError(f'A{index+1:02d}: Step-ACK falsch')
            check=self.dz(15,values[:4])
            if check!=values:raise RuntimeError(f'A{index+1:02d}: Step-Readback falsch ab {start+1}')
        ack=self.dz(16,[0,index,64,2])  # LENGTH=64, RES=2 => 1/16
        if ack!=[0,index,64,2] or self.dz(17,[0,index])!=[0,index,64,2]:
            raise RuntimeError(f'A{index+1:02d}: LENGTH/RES-Readback falsch')
        print(f'PASS: A{index+1:02d} mit NOTE/FLAGS/VELO/GATE/SWING und 1/16 geprüft')
    def put_song(self,rows):
        if not 1<=len(rows)<=256:raise ValueError('Song braucht 1..256 Zeilen')
        blank=(SONG_END,16,1,0)
        allrows=rows+[blank]*(256-len(rows))
        for offset in range(0,256,24):
            chunk=allrows[offset:offset+24]
            address=[offset&127,offset>>7,len(chunk)]
            payload=address[:]
            for kind,pat,steps,val in chunk:payload.extend((kind,pat,steps,val&127,val>>7))
            ack=self.dz(10,payload)
            if ack!=address:raise RuntimeError(f'Song-ACK fehlerhaft ab {offset+1}')
            if self.dz(11,address)!=payload:raise RuntimeError(f'Song-Readback ab {offset+1} falsch')
        count=len(rows)
        meta=[count&127,count>>7,0,1]  # ONCE, Song ON
        if self.dz(12,meta)!=meta or self.dz(13)!=meta:
            raise RuntimeError('256er-Song-Metadaten Readback falsch')
        print(f'PASS: Song {count}/256 Zeilen, MODE ON, ONCE')
    def save(self):
        for _ in range(20):
            ret=self.dz(6)
            if len(ret)>=2 and not ret[0] and ret[1]:
                print('PASS: Flash-A/B Commit abgeschlossen');return
            time.sleep(.5)
        raise TimeoutError('Flash-Speicherung nicht bestätigt – bitte nicht ausschalten')

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    mode=ap.add_mutually_exclusive_group(required=True)
    mode.add_argument('--preview',action='store_true'); mode.add_argument('--send',action='store_true')
    mode.add_argument('--read',action='store_true',help='Aktive Songliste vom FM-1 auslesen; 100%% read-only')
    ap.add_argument('--read-all',action='store_true',help='Mit --read auch unbenutzte Einträge bis 256 anzeigen')
    ap.add_argument('--song',choices=('brahms','musette'),default='brahms',help='Eine aktive Songliste auf dem FM-1')
    ap.add_argument('--preset',type=int,choices=range(1,33),metavar='1..32',help='Optional vor TEMPO Sound P01..P32 aktivieren (nicht überschreiben)')
    ap.add_argument('--in-port'); ap.add_argument('--out-port');ap.add_argument('--yes',action='store_true',help='Bestätigung für unbeaufsichtigten Import')
    args=ap.parse_args()
    if args.read_all and not args.read:
        ap.error('--read-all erfordert --read')
    import mido
    if args.read:
        d=FM1(mido,args.in_port,args.out_port)
        try:
            d.fw_check(require_flash=False)
            d.read_song(all_rows=args.read_all)
        finally:
            d.close()
        return
    pieces=[load_piece(p) for p in PIECES]
    preview(pieces,args.song,args.preset)
    if not args.send:return
    print('\nACHTUNG: Überschreibt die acht DZ-Bank-Patterns A01..A08 und die EINE Songliste.')
    print('Die 32 Klang-Presets P01..P32 bleiben unangetastet. PLAY STOP, REC aus, Browser/DAW schließen.')
    if not args.yes and input('Für Upload exakt JA A01-A08 eingeben: ').strip()!='JA A01-A08':
        print('Abgebrochen, nichts übertragen');return
    p=next(p for p in pieces if p.key==args.song)
    d=FM1(mido,args.in_port,args.out_port)
    try:
        d.fw_check()
        # Each pattern step includes its own MIDI VELO and GATE. No arranger GATE.
        d.parameter(1,G_CLOCK,0)
        print('PASS: pro-Step VELO/GATE; RES=1/16 und SWING=0 pro Pattern/Step')
        for piece in pieces:
            for i,grid in enumerate(piece.grids):d.put_pattern(piece.startpat+i,grid)
        d.put_song(song_rows(p,args.preset))
        d.save()
        print(f'\nFERTIG: A01–A08 mit per-Step VELO/GATE. SONG={p.name}; TEMPO={p.bpm}.')
        print('Für das andere Lied denselben Upload mit --song musette bzw. --song brahms ausführen.')
    finally:d.close()

if __name__=='__main__':
    try:main()
    except (ValueError,RuntimeError,TimeoutError,OSError,EOFError) as e:sys.exit('FEHLER: '+str(e))
