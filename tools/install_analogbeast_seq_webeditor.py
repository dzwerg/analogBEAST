#!/usr/bin/env python3
"""Append analogBEAST SEQ interface to the existing Felucca editor; no firmware edits.

Usage:
 py tools/install_analogbeast_seq_webeditor.py --check-only
 py tools/install_analogbeast_seq_webeditor.py

Conservative, idempotent. Copies external JS, inserts one script tag into editor.html,
updates make_site.py so GitHub Pages editor bundles the JS too.
"""
from __future__ import annotations
import argparse
from pathlib import Path
import shutil
import sys

ROOT=Path(__file__).resolve().parent.parent
EDITOR=ROOT/'web'/'editor.html'
JS=ROOT/'web'/'analogbeast-seq.js'
SOURCE=Path(__file__).resolve().parent/'ab_seq_payload'/'analogbeast-seq.js'
SITE=ROOT/'web'/'make_site.py'
INCLUDE='<script src="analogbeast-seq.js" defer></script>'
SITE_ANCHOR='shutil.copy(HERE / "editor.html", ed / "index.html")'
SITE_EXTRA='    shutil.copy(HERE / "analogbeast-seq.js", ed / "analogbeast-seq.js")'


def prepare():
    if not SOURCE.is_file():raise ValueError('Editor JavaScript payload missing')
    if not EDITOR.is_file():raise ValueError(f'Missing {EDITOR}; never create or replace an existing editor blindly')
    original=EDITOR.read_text(encoding='utf-8')
    if original.count('</body>')!=1 or original.count('</html>')!=1 or '<script' not in original:
        raise ValueError('Unexpected editor.html structure; original file left untouched')
    if original.count(INCLUDE)>1:raise ValueError('Duplicate analogBEAST script tags')
    if 'analogbeast-seq.js' in original and INCLUDE not in original:
        raise ValueError('Unexpected analogBEAST script reference; manual review needed')
    new=original if INCLUDE in original else original.replace('</body>',INCLUDE+'\n</body>')
    site_new=None
    if SITE.is_file():
        s=SITE.read_text(encoding='utf-8')
        if s.count(SITE_ANCHOR)!=1:raise ValueError('Unexpected make_site.py; refusing to make incomplete release bundle')
        if s.count(SITE_EXTRA)>1:raise ValueError('Duplicate site JS copy directive')
        site_new=s if SITE_EXTRA in s else s.replace(SITE_ANCHOR,SITE_ANCHOR+'\n'+SITE_EXTRA)
    else:print('NOTICE: web/make_site.py missing; local editor works but release packaging must be checked')
    if JS.exists() and JS.read_bytes()!=SOURCE.read_bytes():
        raise ValueError('A different web/analogbeast-seq.js already exists: refusing to overwrite')
    return new,site_new


def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--check-only',action='store_true')
    args=p.parse_args(argv)
    try:editor,site=prepare()
    except (ValueError,OSError) as e:print('REFUSED:',e,file=sys.stderr);return 2
    if args.check_only:
        print('PASS: editor.html and optional publishing script verified; no files modified')
        return 0
    # Refuse before *any* write if a backup filename is already occupied.
    if EDITOR.read_text(encoding='utf-8')!=editor and EDITOR.with_suffix('.html.before-ab-seq').exists():
        print('REFUSED: editor backup already exists; no files modified',file=sys.stderr);return 2
    if site is not None and SITE.read_text(encoding='utf-8')!=site and SITE.with_suffix('.py.before-ab-seq').exists():
        print('REFUSED: site backup already exists; no files modified',file=sys.stderr);return 2
    # Preflight above is complete; write only our editor-related paths, with backups.
    if EDITOR.read_text(encoding='utf-8')!=editor:
        backup=EDITOR.with_suffix('.html.before-ab-seq')
        if backup.exists():
            print('REFUSED: editor backup already exists; no files modified',file=sys.stderr);return 2
        shutil.copy2(EDITOR,backup)
        EDITOR.write_text(editor,encoding='utf-8')
        print('PATCHED:',EDITOR.relative_to(ROOT),'backup:',backup.name)
    else:print('ALREADY: web/editor.html')
    if site is not None:
        if SITE.read_text(encoding='utf-8')!=site:
            backup=SITE.with_suffix('.py.before-ab-seq')
            if backup.exists():
                print('REFUSED: site script backup already exists; manual review required',file=sys.stderr);return 2
            shutil.copy2(SITE,backup)
            SITE.write_text(site,encoding='utf-8')
            print('PATCHED:',SITE.relative_to(ROOT),'backup:',backup.name)
        else:print('ALREADY: web/make_site.py')
    if not JS.exists():
        JS.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(SOURCE,JS)
        print('INSTALLED:',JS.relative_to(ROOT))
    else:print('ALREADY: web/analogbeast-seq.js')
    print('PASS: SEQ editor installed, existing editor logic preserved; NO firmware, flash, NOR or MIDI operation')
    return 0

if __name__=='__main__':sys.exit(main())
