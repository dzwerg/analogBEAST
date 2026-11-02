#!/usr/bin/env python3
"""analogBEAST v1.0 UI fix: verified five-source patch, no MIDI/flash access."""
import argparse, hashlib, json, shutil
from pathlib import Path
VERSIONS=json.loads("{\"firmware/src/analogbeast_v5_controls.c\": {\"after\": \"b4c392d22f9adcf85e97b57946ab97854b2b7090beedfda6788316deefeb4c59\", \"before\": \"ed45702ca9ccefaeff528d691fcc9db44ad9e4c01a6756452dd1659abbd81aed\"}, \"firmware/src/analogbeast_v5_controls.h\": {\"after\": \"fdfd6b5e7ad995cd49afe0881ffda6264a32fcfebc1af640bf2be3c5b549f451\", \"before\": \"609305c3af77871931231422feaff0914c0b8747be6f8850c84625343ddddd7e\"}, \"firmware/src/analogbeast_v5_runtime.c\": {\"after\": \"ad4a09fef662151ab2d74a6c90e2a5ecd1f35a8e6551aed555c5346de288cd7b\", \"before\": \"d797c409fc020d3b535d3ea5fda8beab1a24034e6dcf392b0cd841503d0f955c\"}, \"firmware/src/analogbeast_v5_runtime.h\": {\"after\": \"5855b1646492250cb3d3c616dc4c389532b745e180085ff4479a59f05f76ab15\", \"before\": \"a4a401d02ad2f56312a3e59749e64bd9eb7fc3a17609189649a736bde50d57f4\"}, \"firmware/src/golana_ui.c\": {\"after\": \"876b5a7a63f528e2bee0ec24b0f99ff86553f6b2fc79f895c8c15e1a4f44fac8\", \"before\": \"460a0f02f25b25a9479649c570e282a77d7cd27f05193aeae1c957783d9a5547\"}}")
FILES=tuple(VERSIONS)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    arg=argparse.ArgumentParser(description=__doc__)
    arg.add_argument('--check-only',action='store_true')
    args=arg.parse_args()
    root=Path(__file__).resolve().parents[1]
    updates=[]
    for name in FILES:
        dst=root/name
        payload=root/'tools/ab_v1_ui6_payload'/name
        if not dst.is_file() or not payload.is_file() or sha(payload)!=VERSIONS[name]['after']:
            raise SystemExit('REFUSED: missing or damaged source/payload: '+name)
        before=sha(dst)
        if before==VERSIONS[name]['after']:
            print('ALREADY:',name)
            continue
        if before!=VERSIONS[name]['before']:
            raise SystemExit('REFUSED: your file differs from tested analogBEAST v1.0: '+name+' SHA256='+before)
        bak=dst.with_name(dst.name+'.before-ab-v1-ui6')
        if bak.exists() and sha(bak)!=VERSIONS[name]['before']:
            raise SystemExit('REFUSED: existing backup must be protected: '+str(bak))
        updates.append((dst,payload,bak))
    if args.check_only:
        print(f'PASS: {len(updates)} verified source changes ready; no files modified')
        return
    for dst,payload,bak in updates:
        if not bak.exists():shutil.copy2(dst,bak)
        temp=dst.with_name(dst.name+'.ab-v1-ui6-tmp')
        shutil.copy2(payload,temp)
        temp.replace(dst)
        if sha(dst)!=VERSIONS[dst.relative_to(root).as_posix()]['after']:
            raise SystemExit('VERIFY FAILED after patch: '+str(dst))
        print('PATCHED:',dst.relative_to(root))
    print('PASS: SEQ six-page UI installed; no USB, MIDI, NOR, or flash operations')
if __name__=='__main__': main()
