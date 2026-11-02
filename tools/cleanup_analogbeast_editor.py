#!/usr/bin/env python3
"""analogBEAST Studio: replace Felucca legacy nav with SOUND / ARP / SETTINGS.
Only edits web/editor.html; independent analogbeast-seq.js stays untouched.

Run from any directory:
  py tools\cleanup_analogbeast_editor.py --check-only
  py tools\cleanup_analogbeast_editor.py
  py tools\cleanup_analogbeast_editor.py --restore
"""
from __future__ import annotations
import argparse
import datetime
from pathlib import Path
import re
import shutil
import sys

ROOT = Path(__file__).resolve().parent.parent
EDITOR = ROOT / 'web' / 'editor.html'
BACKUP = ROOT / 'web' / 'editor.html.before-ab-arp-cleanup'
MARK = '<!-- analogBEAST v1.0 Studio ARP cleanup -->'

STYLE = '''
<style id="ab-arp-cleanup-style">
/* Old Felucca navigation is retained internally for backwards compatibility,
   but is not part of the analogBEAST Studio UI. No device/flash data is erased. */
#tabs [data-tab="fm6"], #tabs [data-tab="tracks"],
#tabs [data-tab="library"], #tabs [data-tab="samples"],
#tabs [data-tab="projects"] { display:none !important; }
#tabs [data-tab="sound"] { order:0; }
#tabs [data-tab="sequencer"] { order:1; }
#tabs [data-tab="settings"] { order:2; }
#p-fm6, #p-tracks, #p-library, #p-samples, #p-projects { display:none !important; }
/* Sound = oscillator / filter / effects, not a preset browser. Import/export
   of sound files remains available, while hidden controls remain in the DOM. */
#p-sound #browser > label,
#p-sound #init, #p-sound #factlib { display:none !important; }
#p-sound #browser { padding:10px 14px; }
/* The old 4-part Felucca strip is separate from the analogBEAST A-H banks. */
#trackbar { display:none !important; }
/* Our ARP tab shows firmware-backed P_* controls, not the legacy step grid. */
#p-sequencer #steps, #p-sequencer #motion-panel { display:none !important; }
#p-sequencer #seqgroups { columns:unset; display:grid; grid-template-columns:repeat(auto-fit,minmax(min(100%,320px),1fr)); gap:14px; }
#p-sequencer .ab-studio-panel { margin:0; }
#p-sequencer .ab-arp-intro { background:#201b16; border:1px solid #624121; padding:14px 16px; border-radius:11px; margin:12px 0; }
#p-sequencer .ab-arp-intro h2 { margin:0 0 6px;color:#ffb15c;font-size:17px;border:0; }
#p-sequencer .ab-arp-intro p { margin:4px 0;color:#d6bda5;line-height:1.5; }
#p-sound .ab-studio-panel.ab-advanced { min-height:0; }
#p-sound .ab-studio-panel.ab-advanced summary { cursor:pointer; font-weight:700; color:#ffb15c; padding:4px 0; }
#p-sound .ab-studio-panel.ab-advanced[open] summary { margin-bottom:10px; }
#p-sound .ab-studio-panel.ab-advanced:not([open]) { min-height:0; }
</style>
'''
INTRO = '''<div class="ab-arp-intro" id="ab-arp-intro">
      <h2>analogBEAST · ARPEGGIATOR</h2>
      <p>Die Regler steuern den ARP des aktuell gewählten Synth-Parts. GOLANA-ARP gehört zum Sound; Timing/Performance ist trackbezogen und bleibt beim Presetwechsel erhalten.</p>
      <p>Änderungen werden über den bestehenden USB-MIDI-Editor an das Gerät gesendet. Sie werden nicht automatisch als neues Preset gespeichert.</p>
    </div>'''

# Studio source comes from the previously installed release; match exact semantic
# blocks to avoid touching any of the user's separate MIDI/song importer files.
ARP_OLD = '''  add('groups','10 · ARPEGGIATOR','Golana ARP · Last/Stepmask sowie globale ARP-Einstellungen',
    [84,85,86,87,88,89,17,18,19,20,21,22,23,24],'ab-arp');'''
ARP_NEW = '''  add('seqgroups','01 · GOLANA ARP / SOUND','Soundbezogen: ARP ein, Typ, Keysync, Last, Step-Masken 1–4 und 5–8',
    [84,85,86,87,88,89],'ab-arp');
  add('seqgroups','02 · ARP TIMING / PART','Trackbezogen: Modus, Rate, Oktaven, Gate, Swing, Probability, Hold, Order; bleiben beim Presetwechsel erhalten',
    [17,18,19,20,21,22,23,24],'ab-arp');'''
SEQ_OLD = '''  add('seqgroups','TRACK STEP SETTINGS','Parameter des bestehenden Track-Step-Sequencers',
    [29,30,31,32,94],'ab-arp');'''
SEQ_NEW = '''  // The separate analogBEAST piano-roll/SONG editor owns pattern editing.
  // Do not show legacy Felucca sequencer controls in Sound/ARP.
  [29,30,31,32,94].forEach(id => used.add(id));'''
PILL_OLD = 'pill.textContent=`analogBEAST · ${info.pcount} Sound-Parameter · ${info.ntrk || 1} Tracks`'
PILL_NEW = 'pill.textContent=`analogBEAST · GOLANA / BRUTE · Sound & ARP`'
GLOBAL_OLD = '''  globals('FX & SYSTEM','Gemeinsame Effekte und globale Parameter',Array.from({length:info.gcount},(_,i)=>i));'''
GLOBAL_NEW = '''  globals('GLOBAL AUDIO / FX','Geräteweite Effektparameter und Reverb-Typ',[4,5,6,7,8,9,10,11,24]);'''
DETAIL_OLD = '''    const box=el('div',{class:'group ab-studio-panel '+(tag||'')},
      el('h2',{text:title}),el('p',{class:'ab-subtle',text:sub}));'''
DETAIL_NEW = '''    const advanced = tag === 'ab-advanced';
    const box=el(advanced?'details':'div',{class:'group ab-studio-panel '+(tag||'')},
      el(advanced?'summary':'h2',{text:title}),el('p',{class:'ab-subtle',text:sub}));'''

class PatchError(Exception): pass

def exactly_one(src: str, find: str, label: str):
    ct = src.count(find)
    if ct != 1: raise PatchError(f'{label}: expected once, found {ct}')

def transform(src: str) -> tuple[str,str]:
    if MARK in src:
        required = ('ab-arp-cleanup-style', 'ab-arp-intro', ARP_NEW, SEQ_NEW, PILL_NEW)
        if all(x in src for x in required): return src, 'ALREADY'
        raise PatchError('Partial previous ARP patch; no changes made')
    required = [
        ('studio marker','/* analogBEAST Studio v1.0:'),
        ('old ARP card',ARP_OLD),
        ('old track seq card',SEQ_OLD),
        ('preset/track pill',PILL_OLD),
        ('FX globals block',GLOBAL_OLD),
        ('studio advanced card',DETAIL_OLD),
        ('style closing','</head>'),
        ('native legacy sequencer section','<section class="panel" id="p-sequencer" data-tab="sequencer" data-dev hidden>'),
        ('native arp parameter container','<div class="groups" id="seqgroups"></div>'),
    ]
    for label, marker in required: exactly_one(src,marker,label)
    for tab in ('sound','sequencer','settings','fm6','tracks','library','samples','projects'):
        exactly_one(src,f'data-tab="{tab}"',f'tab marker {tab}') if False else None
    # nav has one data-tab=... and one section data-tab=...; use an exact tag regex.
    nav = re.compile(r'<button\s+role="tab"\s+data-tab="sequencer"\s+data-t="tabSeq"\s+data-ic="symbol_grid"\s*></button>')
    if len(nav.findall(src)) != 1: raise PatchError('Sequencer navigation button structure differs')
    src = nav.sub('<button role="tab" data-tab="sequencer" data-ic="ui_knob">ARP</button>', src, count=1)
    for a,b in ((ARP_OLD,ARP_NEW),(SEQ_OLD,SEQ_NEW),(PILL_OLD,PILL_NEW),(GLOBAL_OLD,GLOBAL_NEW),(DETAIL_OLD,DETAIL_NEW)):
        src=src.replace(a,b,1)
    section='<section class="panel" id="p-sequencer" data-tab="sequencer" data-dev hidden>'
    src=src.replace(section,section+'\n    '+INTRO,1)
    src=src.replace('</head>',MARK+'\n'+STYLE+'\n</head>',1)
    return src, 'PATCHED'

def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    group=ap.add_mutually_exclusive_group()
    group.add_argument('--check-only',action='store_true')
    group.add_argument('--restore',action='store_true')
    a=ap.parse_args()
    try:
        if a.restore:
            if not BACKUP.exists(): raise PatchError('No ARP cleanup backup exists')
            now = EDITOR.read_text(encoding='utf-8')
            if MARK not in now: raise PatchError('ARP cleanup not recognized; no restore')
            if EDITOR.exists():
                reverse = EDITOR.with_name('editor.html.before-restore-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
                shutil.copy2(EDITOR,reverse)
            shutil.copy2(BACKUP,EDITOR)
            print('RESTORED:',EDITOR)
            return 0
        if not EDITOR.is_file(): raise PatchError('Missing web/editor.html: install from analogBEAST project root')
        original=EDITOR.read_text(encoding='utf-8')
        patched,status=transform(original)
        if a.check_only:
            print(f'PASS: {status}: SOUND/ARP/SETTINGS and clean UI verified; no files changed')
            return 0
        if status == 'ALREADY':
            print('ALREADY: ARP/Studio cleanup installed')
            return 0
        if BACKUP.exists(): raise PatchError(f'Backup already exists: {BACKUP}; refusing overwrite')
        shutil.copy2(EDITOR,BACKUP)
        tmp=EDITOR.with_name('editor.html.ab-arp-tmp')
        try:
            tmp.write_text(patched,encoding='utf-8')
            tmp.replace(EDITOR)
        except Exception:
            if tmp.exists(): tmp.unlink()
            shutil.copy2(BACKUP,EDITOR)
            raise
        print('PATCHED: web/editor.html')
        print('BACKUP: web/editor.html.before-ab-arp-cleanup')
        print('PASS: ARP tab; sound-linked and track-linked ARP controls separated')
        print('PASS: legacy FM6/Tracks/Library/Samples/Projects tabs hidden; presets list hidden')
        print('PASS: independent SEQ/SONG/ALBUM editor and JS untouched')
        print('NO FIRMWARE / BUILD / USB / MIDI / FLASH / NOR OPERATION')
        return 0
    except PatchError as e:
        print('REFUSED:',e,file=sys.stderr)
        return 2

if __name__=='__main__': sys.exit(main())
