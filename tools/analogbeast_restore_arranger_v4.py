#!/usr/bin/env python3
"""Recover the analogBEAST ab3.5 song list on v4 WITHOUT changing patterns.

Default: offline PREVIEW only, no MIDI.  --send requires full read-only v4
backup, double comparison, strict device state, and typed human confirmation.
Two old SONG GATE commands have no v4 equivalent; they are REMOVED, never
misinterpreted as ARP ON.  Unused v4 song rows are explicitly set to END.

Run from C:\\FM1\\analogBEAST (Windows PowerShell), with the original tools present.
"""
from __future__ import annotations
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path

END=(4,16,1,0)
BLANK=(0,0,64,0)
NAMES={4:'END',5:'TEMPO',6:'PRESET',7:'REST',8:'GATE (legacy: unsupported)'}


def read_v3(path: Path) -> dict:
    doc=json.loads(path.read_text(encoding='utf-8'))
    if doc.get('format') != 'analogBEAST-v3-read-only':
        raise RuntimeError('Not a v3 READ ONLY backup')
    if len(doc.get('patterns',[]))!=64 or len(doc.get('song_rows',[]))!=128:
        raise RuntimeError('v3 backup incomplete: expected 64 patterns and 128 song rows')
    for p in doc['patterns']:
        if len(p['steps'])!=64:raise RuntimeError('v3 pattern has fewer than 64 steps')
    meta=doc.get('song_meta',[])
    if len(meta)!=3 or not 1<=meta[0]<=128 or meta[1] not in (0,1) or meta[2] not in (0,1):
        raise RuntimeError('Unexpected v3 song metadata: '+str(meta))
    return doc


def plan_from_v3(doc: dict):
    active=[]; dropped=[]
    n,loop,mode=doc['song_meta']
    seen_end=False
    for i,orig in enumerate(doc['song_rows'][:n],1):
        if not (isinstance(orig,list) and len(orig)==4 and all(isinstance(x,int) for x in orig)):
            raise RuntimeError(f'Invalid v3 row {i}')
        bank,pat,steps,value=orig
        if bank==8:
            # Previous ab3.5 opcode 8 = SONG GATE. v4 opcode 8 = SONG ARP!
            if pat!=0 or not 1<=value<=127:
                raise RuntimeError(f'Invalid legacy GATE row {i}')
            dropped.append((i,value))
            continue
        if bank<4:
            if pat not in range(16) or value!=0 or not 1<=steps<=64:
                raise RuntimeError(f'Invalid pattern row {i}')
        elif bank==4:
            if (bank,pat,value)!=(4,16,0):raise RuntimeError(f'Invalid END row {i}')
            seen_end=True
        elif bank==5:
            if pat!=0 or not 40<=value<=240:raise RuntimeError(f'Invalid TEMPO row {i}')
        elif bank==6:
            if pat!=0 or not 0<=value<=31:raise RuntimeError(f'Invalid PRESET row {i}')
        elif bank==7:
            if pat!=0 or value!=0:raise RuntimeError(f'Invalid REST row {i}')
        else:
            raise RuntimeError(f'Unsupported v3 kind {bank} in row {i}')
        if not 1<=steps<=64:raise RuntimeError(f'Invalid steps at row {i}')
        active.append((bank,pat,steps,value))
        if seen_end and i!=n:raise RuntimeError('Old END not at end; manual review required')
    if not seen_end:
        raise RuntimeError('No END marker in original active song. Manual review required.')
    if len(active)>256:raise RuntimeError('Song too large for v4')
    return active, dropped, [len(active),loop,mode]


def desc(row):
    k,p,s,v=row
    if k<4:return f'{"ABCD"[k]}{p+1:02d} | PLAY STEPS {s}'
    if k==4:return 'END'
    if k==5:return f'TEMPO {v}'
    if k==6:return f'PRESET P{v+1:02d}'
    if k==7:return f'REST {s} Steps'
    return f'UNKNOWN {row}'


def verify_old_pattern_match(old:dict, current:dict):
    # Verify that a documented, known v3 pattern set is currently loaded in v4.
    old_index={(p['bank'],p['pattern']):p for p in old['patterns']}
    current_patterns=current.get('patterns',[])
    if len(old_index)!=64 or len(current_patterns)!=64:raise RuntimeError('64 patterns missing')
    for p in current_patterns:
        key=p['bank'],p['pattern']
        o=old_index.get(key)
        if o is None or p['length']!=o['length']:
            raise RuntimeError(f'Legacy length mismatch in {key}')
        if len(p['steps'])!=64 or len(o['steps'])!=64:
            raise RuntimeError(f'Incomplete steps in {key}')
        for j,(newstep,oldstep) in enumerate(zip(p['steps'],o['steps']),1):
            if list(newstep[:2])!=oldstep:
                raise RuntimeError(f'v3/v4 NOTE/FLAGS mismatch at {key} step {j}')


def verify_blank_song(current:dict):
    # Do NOT silently erase a song that the user already edited on v4.
    if current.get('format')!='analogBEAST-v4-read-only':
        raise RuntimeError('Unrecognized v4 backup format')
    if len(current.get('song',[])) != 256:
        raise RuntimeError('v4 export incomplete: expected 256 song rows')
    if not all(list(row)==list(BLANK) for row in current['song']):
        raise RuntimeError('Existing v4 song differs from blank A01 placeholders; refusing overwrite')
    # Selecting a row in the v4 UI can extend song_count while leaving every
    # row at the untouched A01 placeholder. Protect actual CONTENT, not count 1.
    meta = current.get('song_meta')
    if not (isinstance(meta, list) and len(meta) == 4 and
            all(type(x) is int and 0 <= x <= 127 for x in meta) and
            meta[2] in (0, 1) and meta[3] in (0, 1)):
        raise RuntimeError('Invalid v4 song metadata; refusing overwrite: '+repr(meta))
    count = meta[0] | (meta[1] << 7)
    if not 1 <= count <= 256:
        raise RuntimeError('Invalid v4 song count; refusing overwrite: '+str(count))
    if count != 1:
        print(f'SAFE: v4 song_count={count}, but ALL 256 song rows are default A01; no song contents to replace')


def verify_status(dev):
    st=dev.dz(18)
    if st != [0,0,1,0,1,0]:
        raise RuntimeError('Unsafe device state; STOP/REC/NOR status must be clean: '+repr(st))


def wire_chunk(start,rows):
    if not 0<=start<256 or not 1<=len(rows)<=24 or start+len(rows)>256:
        raise ValueError('Invalid song-row chunk')
    a=[start&127,start>>7,len(rows)]
    for k,p,s,v in rows:
        if not all(isinstance(x,int) for x in (k,p,s,v)) or not 0<=v<=255:
            raise ValueError('Invalid v4 row')
        a.extend((k,p,s,v&127,v>>7))
    return a


def sync_song(dev,planned,meta):
    # This function calls *only* v4 song op 10 (write rows), op 11 (read rows),
    # op 12 (write metadata), op 13 (read metadata). No op14/16 pattern writes,
    # no old project writes, no RESET/erase, no COMMIT V4 opcode 19.
    allrows=planned+[END]*(256-len(planned))
    for offset in range(0,256,24):
        chunk=allrows[offset:offset+24]
        payload=wire_chunk(offset,chunk)
        a=payload[:3]
        if dev.dz(10,payload)!=a:
            raise RuntimeError(f'Song write ACK failed at offset {offset} (STOP; do not power-cycle)')
        if dev.dz(11,a)!=payload:
            raise RuntimeError(f'Song readback failed at offset {offset} (STOP; do not power-cycle)')
        print(f'  Verified song rows {offset+1}..{offset+len(chunk)}')
    m=[meta[0]&127,meta[0]>>7,meta[1],meta[2]]
    if dev.dz(12,m)!=m or dev.dz(13)!=m:
        raise RuntimeError('Song metadata readback failed (STOP; do not power-cycle)')
    print(f'  Song metadata verified: {meta}')


def compare_after(before,after,planned,meta):
    if before['patterns']!=after.get('patterns'):
        raise RuntimeError('CRITICAL: pattern mismatch after song-only update!')
    if after.get('song')!=[list(r) for r in planned+[END]*(256-len(planned))]:
        raise RuntimeError('CRITICAL: song mismatch after writing')
    if after.get('song_meta')!=[meta[0]&127,meta[0]>>7,meta[1],meta[2]]:
        raise RuntimeError('CRITICAL: song metadata mismatch after writing')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--v3-backup',required=True,type=Path)
    parser.add_argument('--send',action='store_true',help='Write ONLY song rows after double full READ ONLY validation and human confirmation')
    parser.add_argument('--in-port',default='Felucca 3')
    parser.add_argument('--out-port',default='Felucca 4')
    args=parser.parse_args()
    old=read_v3(args.v3_backup)
    planned,dropped,meta=plan_from_v3(old)
    print('Original ab3.5:',old['song_meta'][0],'active rows; loop=',old['song_meta'][1],'song=',old['song_meta'][2])
    for i,row in enumerate(planned,1):print(f' {i:03}: {desc(row)}')
    print('Legacy GATE omitted (v4 has NO arranger GATE command):',dropped)
    print(f'Preview: {len(planned)} playable v4 song rows; remaining {256-len(planned)} rows set to END')
    print('NOTE/FLAGS/pattern lengths are NEVER written by this tool.')
    if not args.send:
        print('PREVIEW ONLY: no MIDI opened; NO changes made')
        return
    # No device write before all checks and an EXPLICIT human confirmation.
    from analogbeast_import_songs_v4 import FM1
    from analogbeast_backup_v4 import collect
    import mido
    dev=FM1(mido,args.in_port,args.out_port)
    try:
        dev.fw_check(require_flash=True)
        verify_status(dev)
        print('READ ONLY: full backup of the CURRENT v4 state...')
        before=collect(dev)
        verify_old_pattern_match(old,before)
        verify_blank_song(before)
        verify_status(dev)
        output=Path.cwd()/('analogBEAST-v4-before-arranger-restore-'+datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S')+'.json')
        payload=json.dumps(before,ensure_ascii=False,indent=2).encode('utf-8')+b'\n'
        with output.open('xb') as file:
            file.write(payload);file.flush()
            import os; os.fsync(file.fileno())
        if output.read_bytes()!=payload:raise RuntimeError('Backup file readback failed')
        print('SAFE COPY:',output)
        print('SHA256:',hashlib.sha256(payload).hexdigest())
        print('READ ONLY: recheck all 4096 steps and entire song before consent...')
        latest=collect(dev)
        verify_status(dev)
        if any(before[k]!=latest[k] for k in ('patterns','song','song_meta')):
            raise RuntimeError('Device changed during read-only checks; NO WRITE')
        confirm=input('To restore ONLY the arranger type RESTORE ARRANGER: ').strip()
        if confirm!='RESTORE ARRANGER':
            print('CANCELLED: no MIDI write commands sent')
            return
        # one more pre-write check after human confirmation
        verify_status(dev)
        now=collect(dev)
        if any(before[k]!=now[k] for k in ('patterns','song','song_meta')):
            raise RuntimeError('Device changed after confirmation; NO WRITE')
        print('Writing ONLY song rows. DO NOT UNPLUG until full readback and autosave complete.')
        sync_song(dev,planned,meta)
        after=collect(dev)
        compare_after(before,after,planned,meta)
        print('PASS: all patterns unchanged, 256 song rows and metadata verified in RAM')
        dev.save()
        if dev.dz(18)!=[0,0,1,0,1,0]:
            raise RuntimeError('Status after autosave unexpected; do not power-cycle')
        print('PASS: NOR autosave committed; leave the device ON until post-check complete')
    finally:
        dev.close()

if __name__=='__main__':main()
