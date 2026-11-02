#!/usr/bin/env python3
"""Restore Engine/Presets selectors hidden by analogBEAST ARP Cleanup.

Only patches CSS in web/editor.html, never accesses a MIDI device.
Usage:
  py tools\restore_analogbeast_engine_preset.py --check-only
  py tools\restore_analogbeast_engine_preset.py
  py tools\restore_analogbeast_engine_preset.py --restore
"""
from __future__ import annotations
import argparse
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
HTML = ROOT / "web" / "editor.html"
BACKUP = ROOT / "web" / "editor.html.before-engine-preset-restore"
OLD = ("#p-sound #browser > label,\n"
       "#p-sound #init, #p-sound #factlib { display:none !important; }")
NEW = "#p-sound #init, #p-sound #factlib { display:none !important; }"
MARK = "<!-- analogBEAST v1.0 Studio ARP cleanup -->"

class Refused(Exception):
    pass

def transform(src: str) -> tuple[str, str]:
    if MARK not in src:
        raise Refused("ARP cleanup marker missing; editor version not recognized")
    if src.count(OLD) == 1:
        if src.count('<select id="engine"') != 1 or src.count('<select id="preset"') != 1:
            raise Refused("Engine/Preset control structure not recognized")
        return src.replace(OLD, NEW, 1), "READY"
    if OLD in src:
        raise Refused("Duplicate hidden-selector rule; refusing partial edit")
    if src.count(NEW) == 1 and "#p-sound #browser > label" not in src:
        return src, "ALREADY"
    raise Refused("Unexpected CSS layout; no changes made")

def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    g=ap.add_mutually_exclusive_group()
    g.add_argument('--check-only',action='store_true')
    g.add_argument('--restore',action='store_true')
    args=ap.parse_args()
    try:
        if args.restore:
            if not BACKUP.is_file():
                raise Refused("No backup found")
            if not HTML.is_file():
                raise Refused("Editor file missing")
            _, status = transform(HTML.read_text(encoding='utf-8'))
            if status != "ALREADY":
                raise Refused("Editor differs from expected patched state; refusing restore")
            shutil.copy2(BACKUP,HTML)
            print("RESTORED: Engine/Preset visibility to pre-fix state")
            return 0
        if not HTML.is_file():
            raise Refused("Missing web/editor.html: run from analogBEAST project")
        old=HTML.read_text(encoding='utf-8')
        new,status=transform(old)
        if args.check_only:
            print(f"PASS: {status}; Engine/Presets can be restored; no files changed")
            return 0
        if status=="ALREADY":
            print("ALREADY: Engine and Preset selectors visible")
            return 0
        if BACKUP.exists():
            raise Refused("Backup already exists and patch is not present; refusing overwrite")
        shutil.copy2(HTML,BACKUP)
        tmp=HTML.with_name('editor.html.engine-preset.tmp')
        try:
            tmp.write_text(new,encoding='utf-8')
            tmp.replace(HTML)
        except Exception:
            if tmp.exists(): tmp.unlink()
            shutil.copy2(BACKUP,HTML)
            raise
        print("PATCHED: web/editor.html (CSS only)")
        print("BACKUP: web/editor.html.before-engine-preset-restore")
        print("PASS: Engine + Presets visible in SOUND; legacy Library remains hidden")
        print("NO MIDI / NOR / FIRMWARE / FLASH / SONG CHANGES")
        return 0
    except (Refused,OSError,UnicodeError) as e:
        print('REFUSED:',e,file=sys.stderr)
        return 2
if __name__=='__main__':sys.exit(main())
