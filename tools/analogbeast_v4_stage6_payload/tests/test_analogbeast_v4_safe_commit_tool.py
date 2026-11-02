#!/usr/bin/env python3
"""Offline guard test: no MIDI hardware and no flash writes."""
import copy
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent.parent/'tools'))
from analogbeast_backup_v4 import collect
from analogbeast_commit_v4 import verify_backup,check_status
from test_analogbeast_v4_tools import FakeDevice

class LockedDevice(FakeDevice):
    def dz(self,op,args=()):
        if op==18:
            self.calls.append(op)
            return [1,1,1,0,0,0]
        return super().dz(op,args)

d=LockedDevice()
backup=collect(d)
verify_backup(backup,collect(d))
check_status(d)
assert 19 not in d.calls
for kind in ('song','patterns','song_meta'):
    wrong=copy.deepcopy(backup)
    if kind=='song':wrong['song'][255][1]=10
    if kind=='patterns':wrong['patterns'][3]['steps'][2][2]=48
    if kind=='song_meta':wrong['song_meta'][0]=2
    try:verify_backup(wrong,collect(d))
    except RuntimeError:pass
    else:raise AssertionError('mismatch accepted: '+kind)
try:verify_backup({'format':'analogBEAST-v4-read-only','song':[]},collect(d))
except RuntimeError:pass
else:raise AssertionError('incomplete backup accepted')
d.dz=lambda op,args=():[0,0,1,0,1,0] if op==18 else []
try:check_status(d)
except RuntimeError:pass
else:raise AssertionError('already committed must be refused')
print('PASS Stage6: external v4 backup exact-match guards and migration state, no MIDI writes')
