#!/usr/bin/env python3
"""Fix ONLY analogBEAST v1 LIVE-PLAY's UI id 255 collision. No device I/O."""
import argparse
import hashlib
import os
from pathlib import Path
import shutil

BEFORE_SHA = "0d6b2767d4353280c7af69da36f9147f056c7f379da9c067fdca35a3e12f7fb5"
AFTER_SHA  = "cae5f59d8b7aaf9872b761c6c2d5d5a18d2c65f2e9be2cf1417738bf2bbfc852"

OLD_ENUM = b'       GV_V5_EDIT_LENGTH, GV_V5_EDIT_RES, GV_AB_PLAY_REPEAT, GV_AB_ALBUM_KIND, GV_AB_PLAY_ALBUM_SONG };'
NEW_ENUM = b'       GV_V5_EDIT_LENGTH, GV_V5_EDIT_RES, GV_AB_PLAY_REPEAT, GV_AB_ALBUM_KIND, GV_AB_PLAY_ALBUM_SONG = 199 };'
OLD_ASSERT = b'_Static_assert(GV_AB_PLAY_ALBUM_SONG < GV_NONE, "V5 UI IDs must fit in a byte without colliding with NONE");   /* (the step sequencer: REC) */'
NEW_ASSERT = b'_Static_assert(GV_AB_ALBUM_KIND < GV_NONE, "V5 UI IDs must not collide with NONE");\n_Static_assert(P_COUNT <= GV_AB_PLAY_ALBUM_SONG && GV_AB_PLAY_ALBUM_SONG < GV_RES,\n               "ALBUM NEXT SONG virtual ID must not overlap real parameters or V5 IDs");'

def sha(b: bytes) -> str:
    return hashlib.sha256(b).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check-only", action="store_true")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    source = root / "firmware/src/golana_ui.c"
    backup = source.with_name("golana_ui.c.before-ab-v1-ui-id-fix")
    if not source.is_file():
        raise SystemExit(f"REFUSED: file not found: {source}")
    current = source.read_bytes()
    if sha(current) == AFTER_SHA:
        print("ALREADY: UI ID collision fixed; no changes made")
        return
    if sha(current) != BEFORE_SHA:
        raise SystemExit("REFUSED: unexpected golana_ui.c SHA256=" + sha(current) +
                         " (no modifications). Send this hash before proceeding.")
    if current.count(OLD_ENUM) != 1 or current.count(OLD_ASSERT) != 1:
        raise SystemExit("REFUSED: cannot find unique exact patch anchors")
    patched = current.replace(OLD_ENUM, NEW_ENUM, 1).replace(OLD_ASSERT, NEW_ASSERT, 1)
    if sha(patched) != AFTER_SHA:
        raise SystemExit("REFUSED: result does not match verified patch")
    if backup.exists() and sha(backup.read_bytes()) != BEFORE_SHA:
        raise SystemExit("REFUSED: existing safety backup differs")
    if args.check_only:
        print("PASS: ONE UI source file verified; safe ID 199 available; no changes made")
        return
    if not backup.exists():
        shutil.copy2(source, backup)
    tmp = source.with_name("golana_ui.c.ab-v1-ui-id-fix-tmp")
    try:
        tmp.write_bytes(patched)
        if sha(tmp.read_bytes()) != AFTER_SHA:
            raise SystemExit("REFUSED: staged file hash mismatch")
        os.replace(tmp, source)
    finally:
        if tmp.exists():
            tmp.unlink()
    print("PATCHED: firmware/src/golana_ui.c (only ID 255 collision)")
    print("PASS: ALBUM NEXT SONG ID=199; GV_NONE=255; original 6 pages unchanged")
    print("NO BUILD, NO MIDI, NO USB, NO FLASH, NO NOR OPERATIONS")

if __name__ == "__main__":
    main()
