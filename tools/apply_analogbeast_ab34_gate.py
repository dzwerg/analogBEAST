#!/usr/bin/env python3
"""analogBEAST ab3.3 -> ab3.4 GATE command (source only).
--check is read-only, --apply makes backups, --verify compares installed bytes.
No flash, preset, or MIDI action is performed by this installer.
"""
from pathlib import Path
import argparse, hashlib, sys, zipfile
ROOT=Path(__file__).resolve().parent.parent
PAYLOAD=Path(__file__).resolve().with_name('analogbeast_ab34_payload.zip')
BASE={'firmware/src/golana_pattern_model.h': '08d984f8e22168281a9b3084cd2cf1b351416cc71f8263bf5fa1289245cb13aa', 'firmware/src/golana_pattern_model.c': 'f66464331fddb6b2c68927e1b5ac120e3f3630f0f08ebe9248d9a5585f7d8283', 'firmware/src/golana_ui.c': '8e0a9a1773e95bd597f5b2fdf36ba85542effbfab823603a56965a9310968b7c', 'firmware/src/golana_dz_store.c': '7b3bbcbae256f52215f3a219be40b31621e411dc66cac36d0a32a185ee71c8f1', 'firmware/src/golana_dz_midi.c': 'b3b6399a8cadfed2659d93ade0bdfabe8d094a7b97682b05763dda1bb957cbc2', 'tools/analogbeast_import_songs.py': '1c258b816a77034ab886bd320a491e3d6ec83baee5990b6bbc19cd4f3859a84a'}
TARGET={'firmware/src/golana_pattern_model.h': '4fa65e45737663cb6c1b3d956fb6d9655deeb0db6398c3367c3f7201b01f5540', 'firmware/src/golana_pattern_model.c': '2e1c49ae78af2bba4dc8312439e5c95be28fbf9e39b9b3e83dc0bdef817ebee7', 'firmware/src/golana_ui.c': 'a5cced29acd5f5c1528bbf8c21a431c392de66ce8fdd8744e8c6c5c25d785edb', 'firmware/src/golana_dz_store.c': '211a8f5c5c6d08615f779c373eafd933a645fe49e040f5622bdf0cdd4324245a', 'firmware/src/golana_dz_midi.c': 'fc5949b4edecc02d33318d44855dbf4405b753680352f7c351175b959213d644', 'tools/analogbeast_import_songs.py': '8eacd868cb65906a619e996dc1707802d7d8e69514baa89cb8159fbddeaf5726', 'tools/analogbeast_album_gate.py': '998c54fd1451e859c17f68b4bbe977cda37235b7a06a9a1796f36f5c1043a035', 'tests/analogbeast_gate_ab34.c': 'c8d6f87372e8ac4299cd9345fa41c1faf6dcaaa6589eb2378e6fb6400c667bc6', 'tests/analogbeast_gate_ab34_storage.c': 'b9a11f25c212ea1e9d3738ee896e714aec522e866a38901970ca566179100f5f', 'tests/analogbeast_gate_ab34_protocol.c': '60dd51e3817dc1f20f88da8703b5fe5e64db5ba76db56dc973b7b88cc348ce35'}
ADD=['tools/analogbeast_album_gate.py', 'tests/analogbeast_gate_ab34.c', 'tests/analogbeast_gate_ab34_storage.c', 'tests/analogbeast_gate_ab34_protocol.c']
def norm(b): return b.replace(b'\r\n',b'\n').replace(b'\r',b'\n')
def sha(b): return hashlib.sha256(norm(b)).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    g=p.add_mutually_exclusive_group(required=True)
    for x in ('check','apply','verify'):g.add_argument('--'+x,action='store_true')
    a=p.parse_args()
    eng=ROOT/'firmware/src/eng_golana.c'
    bak=ROOT/'firmware/src/eng_golana.c.dev5-before-fb-fix.bak'
    if not eng.is_file() or not bak.is_file() or eng.read_bytes()!=bak.read_bytes():
        raise RuntimeError('STOP: BRUTE engine not byte-identical to dev5')
    print('PASS BRUTE dev5 byte-identisch')
    with zipfile.ZipFile(PAYLOAD) as z:
        names=sorted(z.namelist())
        if names!=sorted(TARGET):raise RuntimeError('Payload file list invalid')
        payload={f:norm(z.read(f)) for f in TARGET}
    for f,expected in TARGET.items():
        if sha(payload[f])!=expected:raise RuntimeError('Payload checksum incorrect: '+f)
    changes=[]
    for f in TARGET:
        path=ROOT/f
        if not path.is_file():
            if f not in ADD:raise RuntimeError('Missing source: '+f)
            state='CREATE'; old=None
        else:
            old=path.read_bytes(); current=sha(old)
            if current==TARGET[f]:state='ALREADY'
            elif f in BASE and current==BASE[f]:state='READY'
            else:raise RuntimeError('STOP unknown source '+f+' expected '+str(BASE.get(f))+' actual '+current)
        if a.verify and state!='ALREADY':raise RuntimeError('NOT INSTALLED: '+f)
        if state in ('READY','CREATE'):changes.append((f,old))
        print(state,f)
    if a.verify:print('PASS ab3.4 installed');return
    if a.check:print('CHECK ONLY; no files written');return
    # Validate ALL backups first. Never overwrite an existing backup.
    for f,old in changes:
        path=ROOT/f
        if old is not None and path.with_name(path.name+'.before-ab34-gate').exists():
            raise RuntimeError('STOP Backup exists: '+str(path)+'.before-ab34-gate')
    for f,old in changes:
        path=ROOT/f
        path.parent.mkdir(parents=True,exist_ok=True)
        if old is not None:
            path.with_name(path.name+'.before-ab34-gate').write_bytes(old)
            new=payload[f].replace(b'\n',b'\r\n') if b'\r\n' in old else payload[f]
        else:new=payload[f]
        path.write_bytes(new)
        print('PATCHED',f)
    print('PASS ab3.4 source installed; run host tests and target build before flashing')
if __name__=='__main__':
    try:main()
    except Exception as e:sys.exit('ERROR: '+str(e))
