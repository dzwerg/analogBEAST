#!/usr/bin/env python3
"""QA4: unchanged Python Midi class -> mock mido wire -> real FM-1 C dispatcher.
All transfers stay in a child host simulator; no physical MIDI port is opened.
"""
import importlib.util
import json
import subprocess
import sys
import types
from pathlib import Path

project=Path(sys.argv[1]) if len(sys.argv)>1 else Path('.')
executable=project/'build'/'qa4_device_bridge'
tool=project/'tools'/'analogbeast_transfer.py'
checks=0
_seen_labels=set()

def ck(test,title):
    global checks
    checks+=1
    if not test:raise AssertionError('FAIL '+title)
    if title not in _seen_labels:
        print('PASS '+title)
        _seen_labels.add(title)

class Harness:
    def __init__(self):
        self.process=subprocess.Popen([str(executable)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,
                                      text=True,bufsize=1,stderr=sys.stderr)
        intro=self.process.stdout.readline().strip()
        ck(intro=='READY QA4_SIMULATED_RAM_ONLY','actual C firmware dispatcher booted in RAM-only mode')
        self.input=SimInput(self)
        self.output=SimOutput(self)
        self.messages=[]
    def exchange(self,bytes_):
        wire=' '.join(f'{b:02X}' for b in (0xF0,*bytes_,0xF7))
        self.process.stdin.write(wire+'\n');self.process.stdin.flush()
        reply=self.process.stdout.readline().strip()
        if not reply.startswith('F0 '):raise AssertionError('invalid C dispatcher response: '+reply)
        data=[int(v,16) for v in reply.split()]
        ck(data[0]==0xF0 and data[-1]==0xF7 and data[1:5]==[0x7D,0x46,0x4C,96],
           'firmware reply has correct F0/MAGIC/SysEx96/F7 envelope')
        ck(all(0<=x<128 for x in data[1:-1]),'response has only 7-bit payload bytes')
        self.messages.append(SimMessage('sysex',data[1:-1]))
    def busy(self,on):
        self.process.stdin.write('BUSY '+('1' if on else '0')+'\n');self.process.stdin.flush()
        ck(self.process.stdout.readline().strip()=='OK','simulated transport busy state set')
    def close(self):
        self.process.stdin.write('EXIT\n');self.process.stdin.flush()
        self.process.wait(timeout=10)
        ck(self.process.returncode==0,'simulated dispatcher exited cleanly')

class SimMessage:
    def __init__(self,typ,data=()):
        self.type=typ
        self.data=tuple(data)

class SimInput:
    def __init__(self,h):self.h=h
    def iter_pending(self):
        result=self.h.messages[:];self.h.messages.clear();return result
    def close(self):pass

class SimOutput:
    def __init__(self,h):self.h=h
    def send(self,msg):
        assert msg.type=='sysex'
        ck(tuple(msg.data[:4])==(0x7D,0x46,0x4C,96),'Python MIDI uses unchanged firmware manufacturer/magic/protocol')
        ck(all(0<=b<128 for b in msg.data),'Python sends strictly 7-bit SysEx data')
        self.h.exchange(tuple(msg.data))
    def close(self):pass

h=Harness()
fake=types.ModuleType('mido')
fake.Message=SimMessage
fake.get_input_names=lambda:['QA4 Virtual In']
fake.get_output_names=lambda:['QA4 Virtual Out']
fake.open_input=lambda name:h.input
fake.open_output=lambda name:h.output
sys.modules['mido']=fake
spec=importlib.util.spec_from_file_location('analogbeast_transfer_qa4',tool)
mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
m=None
try:
    # Explicitly test the user-facing 1.0 export schema, not a made-up new format.
    oldrows=[[8,16,1,0] for _ in range(64)]
    oldrows[0]=[0,0,16,0]
    oldrows[1]=[9,0,1,200]
    oldrows[2]=[10,0,1,3]
    oldrows[3]=[12,0,1,1]
    old={'format':'analogBEAST-v1.0','songs':[{'song':1,'count':5,'loop':0,'rows':oldrows}]}
    ck(len(mod.parse_document(old))==1,'canonical v1.0 JSON backup is accepted')
    ck(json.loads(json.dumps(old))==old,'canonical v1.0 JSON survives UTF-8/JSON roundtrip')
    newrows=[row.copy() for row in oldrows]
    rows=[(13,6),(14,48),(15,5),(16,12),(17,13),(18,0)]
    for i,(kind,val) in enumerate(rows):newrows[4+i]=[kind,0,1,val]
    rec={'song':1,'count':11,'loop':1,'rows':newrows}
    doc={'format':'analogBEAST-v1.0','patterns':[],'songs':[rec],'albums':[]}
    ck(len(mod.parse_document(doc))==1,'same 1.0 JSON format accepts all six 1.1 commands')
    for invalid in [[13,0,1,7],[14,0,1,49],[15,0,1,0],[16,0,1,0],[17,0,1,65],[18,0,1,1]]:
        try:mod.row_valid(invalid)
        except ValueError:pass
        else:raise AssertionError('Invalid 1.1 command accepted '+str(invalid))
    ck(True,'invalid 1.1 command values correctly rejected by CLI')
    m=mod.Midi('QA4 Virtual In','QA4 Virtual Out')
    before=m.read_song(1)
    ck(before['song']==1 and len(before['rows'])==64,'read-only baseline backup retrieved through actual C dispatcher')
    m.write_song(old['songs'][0])
    ck(m.read_song(1)==old['songs'][0],'legacy v1.0 SONG restored and read back exactly')
    m.write_song(rec)
    ck(m.read_song(1)==rec,'1.1 SONG read-write-read exact, including 200 BPM and all new commands')
    # Verify real album op25/op26 and 127 empty-slot conversion.
    album_before=m.read_album(1)
    album={'album':1,'count':2,'loop':1,'song_ids':[1,2]+[None]*30}
    m.write_album(album)
    ck(m.read_album(1)==album,'ALBUM 32-slot SysEx write and readback including empty 127 markers')
    # Device-side copy invokes the real overlapping-row-safe 1.1 parser.
    returned=m.request(29,[0,4,1,0,6])
    ck(returned==[0,4,1,0,6],'SONG range copy acknowledgment from actual dispatcher')
    dest=m.read_song(2)
    ck(dest['rows'][:6]==rec['rows'][4:10],'SONG copy readback retains all six new 1.1 commands')
    # STOP/PLAY gate is in the real firmware dispatcher, not in the mock.
    h.busy(True)
    try:m.write_song(rec)
    except RuntimeError as exc:ck('status 2' in str(exc),'firmware rejects SysEx write while transport runs')
    else:raise AssertionError('Busy gate failed')
    h.busy(False)
    ck(m.read_song(1)==rec,'blocked busy write left original SONG unchanged')
    # Backup restore: original 1.0-shaped state, byte-for-byte after a real write.
    m.write_song(before)
    ck(m.read_song(1)==before,'full original SONG restored from read-only backup')
    m.write_album(album_before)
    ck(m.read_album(1)==album_before,'full original ALBUM restored from read-only backup')
    print(f'QA4 Python/client <-> real C device simulator: {checks} checks PASSED')
finally:
    if m:m.close()
    h.close()
