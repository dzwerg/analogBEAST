#!/usr/bin/env python3
"""analogBEAST v1.0 SONG LOOP/ONCE: one verified UI-only source change."""
import argparse
import hashlib
from pathlib import Path

SOURCE = "firmware/src/golana_ui.c"
BEFORE = "876b5a7a63f528e2bee0ec24b0f99ff86553f6b2fc79f895c8c15e1a4f44fac8"
AFTER = "dfdcacede67ab43f454de3d4182bd48b2cf2e7cf9255db385fd174b2dffece24"
OLD = b'    if (q->id == GV_AB_PLAY_REPEAT) {\n        if (dz_play_has_backup || transport_busy() || gu.dz_edit_active || gu.save) {\n            gu_say("STOP FIRST", 0); return;\n        }\n'
NEW = b'    if (q->id == GV_AB_PLAY_REPEAT) {\n        /* A running v5 SONG may switch LOOP/ONCE: ab5_seek() reads the\n         * selected song\'s loop flag again at its next END. Do not allow\n         * edits to PATTERN, ALBUM, or a running legacy v4 transport. */\n        uint8_t live_song = ab5_model.play_mode == AB5_MODE_SONG &&\n            ab5_play_active && ab5_play.mode == AB5_MODE_SONG &&\n            ab5_play.song_id == ab5_model.selected_song;\n        if (gu.dz_edit_active || gu.save ||\n            ((dz_play_has_backup || transport_busy()) && !live_song)) {\n            gu_say("STOP FIRST", 0); return;\n        }\n'

def digest(data):
    return hashlib.sha256(data).hexdigest()

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--check-only", action="store_true")
    args=ap.parse_args()
    root=Path(__file__).resolve().parents[1]
    p=root/SOURCE
    if not p.is_file():
        raise SystemExit("REFUSED: missing " + SOURCE)
    current=p.read_bytes()
    current_hash=digest(current)
    if current_hash==AFTER:
        print("PASS: SONG LOOP/ONCE patch already installed; no changes")
        return
    if current_hash!=BEFORE:
        raise SystemExit("REFUSED: unexpected golana_ui.c checksum " + current_hash + "\nProvide current source; no files modified.")
    if current.count(OLD)!=1:
        raise SystemExit("REFUSED: original UI guard missing or duplicated")
    result=current.replace(OLD,NEW)
    if digest(result)!=AFTER:
        raise SystemExit("REFUSED: patch content hash mismatch")
    bak=p.with_name(p.name+".before-ab-v1-song-once")
    if bak.exists() and digest(bak.read_bytes()) != BEFORE:
        raise SystemExit("REFUSED: existing source backup differs: " + str(bak))
    if args.check_only:
        print("PASS: one UI-only change ready; no files modified")
        return
    if not bak.exists():
        bak.write_bytes(current)
    tmp=p.with_name(p.name+".song-once-tmp")
    tmp.write_bytes(result)
    if digest(tmp.read_bytes())!=AFTER:
        tmp.unlink(missing_ok=True)
        raise SystemExit("REFUSED: staged source verification failed")
    tmp.replace(p)
    if digest(p.read_bytes())!=AFTER:
        raise SystemExit("VERIFY FAILED: golana_ui.c differs from verified result")
    print("PASS: SONG LOOP/ONCE knob 4 enabled during SONG playback")
    print("PASS: PATTERN/ALBUM and legacy v4 playback guards unchanged")
    print("NO MIDI, FLASH or NOR operations performed")

if __name__=='__main__':
    main()
