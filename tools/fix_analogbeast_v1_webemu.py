#!/usr/bin/env python3
"""Integrate analogBEAST v1.0 firmware into Felucca WebAssembly emulator.
Only patches web/emu/felucca_web.c. Never modifies device firmware, flash, songs, or editor.
Supports --check-only. Refuses unfamiliar layouts instead of forcing replacements.
"""
from pathlib import Path
import argparse
import shutil
import sys

INC = '#include "../../firmware/src/{}.c"'
PRE_PROJECT = ('golana_ext', 'analogbeast_snapshot', 'golana_pattern_model')
POST_PROJECT = ('golana_dz_store', 'analogbeast_v5_extra', 'analogbeast_v5_controls',
                'analogbeast_v5_nor', 'analogbeast_v5_runtime')

def exactly_one(src, anchor, label):
    if src.count(anchor) != 1:
        raise ValueError(f'{label}: expected exactly one anchor; found {src.count(anchor)}')

def includes_order(src):
    a = [x for x in PRE_PROJECT if INC.format(x) in src]
    b = [x for x in POST_PROJECT[1:] if INC.format(x) in src]
    for x in PRE_PROJECT + POST_PROJECT + ('golana_ui', 'golana_dz_midi'):
        if src.count(INC.format(x)) > 1:
            raise ValueError(f'duplicate include: {x}')
    if a and len(a) != len(PRE_PROJECT):
        raise ValueError('partial legacy Golana modules found; refusing to guess include order')
    if b and len(b) != len(POST_PROJECT[1:]):
        raise ValueError('partial AB5 modules found; refusing to guess include order')
    pos = lambda x: src.find(INC.format(x))
    seq = [x for x in PRE_PROJECT + ('project',) + POST_PROJECT + ('golana_ui',)
           if pos(x) >= 0]
    if [pos(x) for x in seq] != sorted(pos(x) for x in seq):
        raise ValueError('unexpected include order; source needs manual review')

def update(s):
    if '\r\n' in s: raise ValueError('CRLF sources not supported by this conservative fix')
    for x in ('upreset', 'project', 'golana_ui', 'ui_draw'):
        exactly_one(s, INC.format(x), x)
    for k in ('static void web_power_on(void)', 'static void web_frame(void)', 'static void web_ms(void)'):
        exactly_one(s,k,k)
    includes_order(s)
    # Branding lives in felucca.c, which the web emulator does not include.
    if '#define AB_BRAND_NAME ' not in s:
        mark=INC.format('ui_draw')
        s=s.replace(mark, '#ifndef AB_BRAND_NAME\n#define AB_BRAND_NAME "analogBEAST"\n#endif\n'+mark,1)
    # As in firmware/src/felucca.c: Golana's model before project, V5 model after it.
    if INC.format('golana_ext') not in s:
        pre='\n'.join(INC.format(x) for x in PRE_PROJECT)+'\n'
        s=s.replace(INC.format('project'),pre+INC.format('project'),1)
    if INC.format('golana_dz_store') not in s:
        s=s.replace(INC.format('project'),INC.format('project')+'\n'+INC.format('golana_dz_store'),1)
    if INC.format('analogbeast_v5_extra') not in s:
        ab='\n'.join(INC.format(x) for x in POST_PROJECT[1:])
        s=s.replace(INC.format('golana_dz_store'),INC.format('golana_dz_store')+'\n'+ab,1)
    if INC.format('golana_dz_midi') not in s:
        s=s.replace(INC.format('golana_ui'),INC.format('golana_ui')+'\n'+INC.format('golana_dz_midi'),1)

    # Startup must mirror main.c's GOLANA branch, not Felucca session restore.
    st=s.index('static void web_power_on(void)')
    en=s.index('static void web_frame(void)',st)
    block=s[st:en]
    if 'gu_boot();' not in block:
        mark='    felucca_init();\n'
        exactly_one(block,mark,'web_power_on felucca_init')
        block=block.replace(mark,mark+'#if GOLANA\n    gu_boot();\n#endif\n',1)
    if '#if !GOLANA\n    autosave_boot(boot_clean);' not in block:
        mark='    autosave_boot(boot_clean);'
        exactly_one(block,mark,'web_power_on autosave')
        block=block.replace(mark,'#if !GOLANA\n'+mark+'\n#endif',1)
    s=s[:st]+block+s[en:]

    st=s.index('static void web_frame(void)')
    en=s.index('static void web_ms(void)',st)
    block=s[st:en]
    if '    gu_input();' not in block:
        mark='    ui_input();\n    settings_poll();\n    autosave_poll();\n    ui_leds();\n    ui_draw();'
        exactly_one(block,mark,'web_frame Felucca UI branch')
        repl=("""#if GOLANA
    gu_input();
    settings_poll();
    gu_leds();
    gu_draw();
#else
    ui_input();
    settings_poll();
    autosave_poll();
    ui_leds();
    ui_draw();
#endif""")
        block=block.replace(mark,repl,1)
    s=s[:st]+block+s[en:]

    st=s.index('static void web_ms(void)')
    # Only the wait-loop UI call, not the UI calls in web_frame.
    block=s[st:]
    if '        gu_input();' not in block:
        mark='    } else {\n        ui_input();'
        exactly_one(block,mark,'web_ms wait loop')
        block=block.replace(mark,"""    } else {
#if GOLANA
        gu_input();
#else
        ui_input();
#endif""",1)
    s=s[:st]+block
    includes_order(s)
    for x in PRE_PROJECT + POST_PROJECT + ('golana_ui', 'golana_dz_midi'):
        exactly_one(s,INC.format(x),x)
    return s

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--check-only',action='store_true')
    o=ap.parse_args()
    root=Path(__file__).resolve().parents[1]
    target=root/'web/emu/felucca_web.c'
    if not target.is_file(): sys.exit('REFUSED: no web/emu/felucca_web.c under project root')
    raw=target.read_bytes()
    try:
        src=raw.decode('utf-8')
        result=update(src)
    except (ValueError,UnicodeDecodeError) as e:
        sys.exit('REFUSED: '+str(e))
    if result==src:
        print('ALREADY: analogBEAST WASM integration present; no changes')
        return
    if o.check_only:
        print('PASS: web/emu/felucca_web.c is compatible; one-file patch ready, NO changes made')
        return
    backup=target.with_name('felucca_web.c.before-analogbeast-emu-v1')
    if backup.exists() and backup.read_bytes()!=raw:
        sys.exit('REFUSED: existing backup differs; refusing to overwrite it')
    if not backup.exists(): shutil.copy2(target, backup)
    tmp=target.with_name('felucca_web.c.ab-emu-temp')
    tmp.write_bytes(result.encode('utf-8'))
    tmp.replace(target)
    print('PATCHED: web/emu/felucca_web.c (only)')
    print('PASS: AB5 modules, analogBEAST branding, Golana boot/UI/render loop integrated')
    print('NO FIRMWARE, MIDI, USB, FLASH OR NOR OPERATIONS')

if __name__=='__main__':
    main()
