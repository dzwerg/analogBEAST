#!/usr/bin/env python3
"""Checked, fail-closed analogBEAST v1.0 PLAY expansion. NEVER talks to FM-1."""
import argparse,hashlib,shutil
from pathlib import Path
HASHES={'firmware/src/golana_ui.c': ('dfdcacede67ab43f454de3d4182bd48b2cf2e7cf9255db385fd174b2dffece24', '0d6b2767d4353280c7af69da36f9147f056c7f379da9c067fdca35a3e12f7fb5'), 'firmware/src/analogbeast_v5_runtime.c': ('ad4a09fef662151ab2d74a6c90e2a5ecd1f35a8e6551aed555c5346de288cd7b', 'cf330eff6107be2129547c79c7d503d43ad2bcab7b4e54917b7f70012d1da3e9'), 'firmware/src/analogbeast_v5_runtime.h': ('5855b1646492250cb3d3c616dc4c389532b745e180085ff4479a59f05f76ab15', 'ee94a6c8631d3a9eadb6c281c4b7e2660e529e8bd30456b3d81f4e94b68521c2'), 'firmware/src/seq.c': ('a6b2c8e0abe8cc9a8bb1d1fa91e980f117590c23bdab21ad838c35c8869eeed8', 'c82cd5a74e883ff13878507d4303075eac2eca2b6551a32586535734c562f297')}
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
 ap=argparse.ArgumentParser(description=__doc__)
 ap.add_argument('--check-only',action='store_true')
 args=ap.parse_args()
 root=Path(__file__).resolve().parents[1]
 todo=[]
 for rel,(before,after) in HASHES.items():
  p=root/rel;payload=root/'tools/ab_v1_live_payload'/rel
  if not p.is_file() or not payload.is_file() or sha(payload)!=after:
   raise SystemExit('REFUSED: missing source or incorrect bundled payload: '+rel)
  now=sha(p)
  if now==after:
   print('ALREADY:',rel);continue
  if now!=before:
   raise SystemExit('REFUSED: unexpected local source (no changes made): '+rel+' sha='+now)
  bak=p.with_name(p.name+'.before-ab-v1-liveplay')
  if bak.exists() and sha(bak)!=before:
   raise SystemExit('REFUSED: existing backup differs: '+str(bak))
  todo.append((p,payload,bak,after))
 if args.check_only:
  print('PASS:',len(todo),'verified source files ready; no changes made; no MIDI/flash activity')
  return
 # Validate every target and all backups BEFORE any write.
 for p,payload,bak,expected in todo:
  if not bak.exists():shutil.copy2(p,bak)
 for p,payload,bak,expected in todo:
  tmp=p.with_name(p.name+'.ab-v1-liveplay-tmp')
  shutil.copy2(payload,tmp)
  if sha(tmp)!=expected:raise SystemExit('REFUSED: staged patch verification failed: '+str(p))
  tmp.replace(p)
  if sha(p)!=expected:raise SystemExit('VERIFY FAIL: '+str(p))
  print('PATCHED:',p.relative_to(root))
 print('PASS: live PLAY, queue-at-boundary, OCT transpose and six-page order installed')
 print('NO BUILD, NO USB, NO MIDI, NO FLASH, NO NOR OPERATIONS')
if __name__=='__main__':main()
