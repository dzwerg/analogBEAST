#!/usr/bin/env python3
"""analogBEAST ab2 UI/navigation patch; --check is strictly read-only."""
import argparse
import hashlib
from pathlib import Path
import sys

PROJECT = Path(__file__).resolve().parent.parent
UI = PROJECT / 'firmware/src/golana_ui.c'
DSP = PROJECT / 'firmware/src/eng_golana.c'
DSP_DEV5 = PROJECT / 'firmware/src/eng_golana.c.dev5-before-fb-fix.bak'
TEST = PROJECT / 'tests/dz_stage5_ui.c'
TEST_PAYLOAD = Path(__file__).resolve().parent / 'dz_stage5_ui.ab2.c'
ORIG_TEST_SHA = 'e6f9b7e3b8fc8df548103eb0687a7ebfaf4eb17c55965ef7c2d8605a19764cf3'
MARKER = 'analogBEAST ab2: per-engine navigation and contextual ALGO'


def replace_once(s, old, new, label):
    cnt = s.count(old)
    if cnt != 1:
        raise ValueError(f'{label}: expected once, found {cnt}')
    return s.replace(old, new, 1)


def patch_ui(src):
    s = src
    s = replace_once(s,
        ' *   ALGORITHM  CUTOFF, whatever the page (as the original\'s cutoff knob)',
        ' *   ALGORITHM  context knob: HOME -> BPM, other pages reserved for explicit assignments', 'UI header')
    s = replace_once(s,
        '/* ------------------------------------------------------------ the matrix --- */',
        f'/* {MARKER} */\n/* ------------------------------------------------------------ the matrix --- */', 'marker')
    # Visibility: only BRUTE-specific page is hidden in MIKROKORG; shared controls are retained.
    anchor = 'static track_t *gu_t(void) { return &trk[0]; }\n'
    addition = '''
/* DSP-supported modes are currently 0=MIKROKORG, 1=BRUTE (dev5).
 * Future B1/BEAST pages enter this predicate only when their DSP is added. */
static int gu_page_visible(uint32_t page)
{
    uint32_t mode = (uint32_t)gu_t()->p[P_GLSYNTH];
    if (page >= GU_NPAGE) return 0;
    if (GU_PAGES[page].k[0].id == P_GLBRUTE && mode != 1u)
        return 0;                 /* BRUTE FACTOR has no effect in MIKROKORG */
    return 1;
}
'''
    s = replace_once(s, anchor, anchor + addition, 'page visibility')

    anchor = '    static const gu_par_t STEINER_TYPE = {P_GLSPMODE, "TYPE", 0};\n'
    addition = '''    static const gu_par_t HIDDEN = {GV_NONE, 0, 0, 0};
'''
    s = replace_once(s, anchor, anchor + addition, 'hidden card')
    anchor = '    /* The FILTER page switches only its first knob; MIKROKORG preserves GL_FTYPE. */\n'
    addition = '''    /* In BRUTE forced MONO: ASSIGN and DETUNE do nothing; SPREAD needs
     * POLY/UNISON.  Hide these cards, but leave shared AMP/PORTA/SYNTH. */
    if (gu_t()->p[P_GLSYNTH] == 1) {
        if (pg->k[0].id == P_VOICE && (k == 0u || k == 1u)) return &HIDDEN;
        if (pg->k[0].id == P_LEVEL && k == 3u) return &HIDDEN;
    }
'''
    s = replace_once(s, anchor, addition + anchor, 'brute muted cards')
    anchor = '    h = gu_hashv(h, (int32_t)gu.page);\n'
    s = replace_once(s, anchor,
        anchor + '    h = gu_hashv(h, (int32_t)gu_t()->p[P_GLSYNTH]);\n', 'tabs cache')
    anchor = '''        if (GU_PAGES[pi].btn != btn)
            continue;
'''
    s = replace_once(s, anchor,
        '''        if (GU_PAGES[pi].btn != btn || !gu_page_visible(pi))
            continue;
''', 'tabs filter')

    anchor = '''static void gu_draw(void)
{
    uint32_t k;
    if (!gu.ready)
        return;'''
    s = replace_once(s, anchor,
        '''static void gu_draw(void)
{
    uint32_t k;
    if (!gu.ready)
        return;
    /* Preset load can switch synth while we are on a now-hidden page. */
    if (!gu_page_visible(gu.page)) {
        gu.page = 0u;
        ui.force = 1u;
    }''', 'safe load page')

    anchor = '''static void gu_go(uint32_t page)
{
    gu.page = (uint8_t)(page % GU_NPAGE);
}
'''
    replacement = '''static void gu_go(uint32_t page)
{
    page %= GU_NPAGE;
    gu.page = (uint8_t)(gu_page_visible(page) ? page : 0u);
    if (GU_PAGES[gu.page].btn == B_SEQ) {
        uint8_t first = GU_PAGES[gu.page].k[0].id;
        gu.dz_seq_mode = first == GV_DZ_BANK ? 1u : first == GV_DZ_SONG_STEP ? 2u : 0u;
    }
}

/* SELECT visits every visible page. A hidden page cannot be selected. */
static void gu_select_turn(int32_t steps)
{
    uint32_t page = gu.page, remaining, scan;
    int direction = steps < 0 ? -1 : 1;
    if (!steps) return;
    remaining = (uint32_t)(steps < 0 ? -(int64_t)steps : steps);
    remaining %= GU_NPAGE;   /* full turns end at the same visible page */
    while (remaining--) {
        for (scan = 0; scan < GU_NPAGE; scan++) {
            page = (uint32_t)(((int32_t)page + direction + (int32_t)GU_NPAGE) % (int32_t)GU_NPAGE);
            if (gu_page_visible(page)) break;
        }
    }
    gu_go(page);
}
'''
    # mod counts using GU_NPAGE causes skip when filtered; only huge delta; fix use current per reasonable steps
    replacement = replacement.replace('    remaining %= GU_NPAGE;   /* full turns end at the same visible page */\n',
        '    if (remaining > 64u) remaining = 64u; /* bound input spikes */\n')
    s = replace_once(s, anchor, replacement, 'navigation')

    start = s.index('static uint32_t gu_dz_seq_page(uint8_t mode)\n')
    end = s.index('/* a page button:', start)
    s = s[:start] + s[end:]  # ALGO is no longer navigation; SEQ/SELECT own all pages.
    anchor = '''    for (pi = 0; pi < GU_NPAGE; pi++)
        if (GU_PAGES[pi].btn == b) {'''
    s = replace_once(s, anchor,
        '''    for (pi = 0; pi < GU_NPAGE; pi++)
        if (GU_PAGES[pi].btn == b && gu_page_visible(pi)) {''', 'button first visible')
    anchor = '''        /* SEQ always returns to ARP, the first normal sequencer page.
         * SELECT navigates TIMING / STEPS; ALGO opens DZ BANK / SONG. */'''
    s = replace_once(s, anchor,
        '''        /* SEQ resets to ARP; SELECT alone navigates TIMING / STEPS / BANK / SONG. */''', 'SEQ comment')
    anchor = '''    if (GU_PAGES[gu.page].btn == b) {
        pi = gu.page + 1u;
        gu_go(pi < GU_NPAGE && GU_PAGES[pi].btn == b ? pi : first);
    } else {'''
    s = replace_once(s, anchor,
        '''    if (GU_PAGES[gu.page].btn == b) {
        for (pi = gu.page + 1u; pi < GU_NPAGE && GU_PAGES[pi].btn == b; pi++)
            if (gu_page_visible(pi)) break;
        gu_go(pi < GU_NPAGE && GU_PAGES[pi].btn == b ? pi : first);
    } else {''', 'button cycle')

    anchor = 'static void gu_save_open(void)\n'
    addition = '''/* Context mapping for ALGO: add {button, page-first-id, target} here.
 * The input handler never needs another navigation special case. */
typedef struct { uint8_t button, page_first; gu_par_t target; } gu_algo_binding_t;
static const gu_algo_binding_t GU_ALGO_BINDINGS[] = {
    {B_HOME, GL_CUTOFF, {GV_BPM, "TEMPO", 0, 0}},
};
static void gu_algo_turn(int32_t steps)
{
    const gu_page_t *page = &GU_PAGES[gu.page];
    uint32_t i;
    if (!steps) return;
    for (i = 0; i < NELEM(GU_ALGO_BINDINGS); i++) {
        const gu_algo_binding_t *binding = &GU_ALGO_BINDINGS[i];
        if (page->btn == binding->button && page->k[0].id == binding->page_first) {
            int32_t lo, hi;
            char value[16];
            gu_range(&binding->target, &lo, &hi);
            gu_set(&binding->target, gu_get(&binding->target) + gu_accel(steps, hi - lo));
            fmt_int(value, gu_get(&binding->target));
            gu_say("TEMPO ", value);
            return;
        }
    }
    /* Other pages currently unassigned: never change synth/sequence silently. */
}

'''
    s = replace_once(s, anchor, addition + anchor, 'ALGO binding')
    anchor = '''        s = panel_enc(EN_ALGO);
        if (s) {
            /* ALGO always opens MicroSTEP, including from unsaved WRITE.
             * Cancel the dialog explicitly: do not silently store a preset. */
            gu.save = 0u;
            gu.dz_seq_mode = 1u;
            gu_go(gu_dz_seq_page(gu.dz_seq_mode));
            gu.hot = -1;
            ui.force = 1u;
            gu_say("CANCELLED", 0);
        }
'''
    s = replace_once(s, anchor,
        '''        (void)panel_enc(EN_ALGO); /* ALGO cannot cancel WRITE */
''', 'WRITE ALGO')
    anchor = '''        s = panel_enc(EN_SELECT);
        if (s)
            gu_go((uint32_t)(((int32_t)gu.page + s % (int32_t)GU_NPAGE + (int32_t)GU_NPAGE) % (int32_t)GU_NPAGE));'''
    s = replace_once(s, anchor,
        '''        s = panel_enc(EN_SELECT);
        if (s)
            gu_select_turn(s);''', 'SELECT handler')
    anchor = '''        s = panel_enc(EN_ALGO);
        if (s) {
            /* From ANY page, ALGO opens MicroSTEP's BANK/PATTERN menu.
             * When already there, alternate BANK/PATTERN and SONG. */
            if (gu.dz_edit_active)
                gu_dz_editor_end();  /* commit pending REC before leaving */
            if (GU_PAGES[gu.page].btn != B_SEQ || gu.dz_seq_mode == 0u)
                gu.dz_seq_mode = 1u;
            else
                gu.dz_seq_mode = gu.dz_seq_mode == 1u ? 2u : 1u;
            gu_go(gu_dz_seq_page(gu.dz_seq_mode));
            gu.hot = -1;
            ui.force = 1;
        }
'''
    s = replace_once(s, anchor,
        '''        gu_algo_turn(panel_enc(EN_ALGO));
''', 'ALGO input')
    return s


def main():
    pa = argparse.ArgumentParser()
    group = pa.add_mutually_exclusive_group(required=True)
    group.add_argument('--check', action='store_true')
    group.add_argument('--apply', action='store_true')
    args = pa.parse_args()
    if not all(p.exists() for p in (UI, DSP, DSP_DEV5, TEST, TEST_PAYLOAD)):
        sys.exit('ERROR: required project source, test payload or dev5 backup missing')
    if DSP.read_bytes() != DSP_DEV5.read_bytes():
        sys.exit('STOP: BRUTE DSP differs from dev5 backup; nothing changed')
    old = UI.read_text(encoding='utf-8')
    installed = MARKER in old
    test_before = TEST.read_bytes()
    test_after = TEST_PAYLOAD.read_bytes()
    test_same = test_before == test_after
    if not test_same and hashlib.sha256(test_before).hexdigest() != ORIG_TEST_SHA:
        sys.exit('STOP: Stage5 test changed since source ZIP; nothing changed')
    if installed:
        if not test_same:
            sys.exit('STOP: UI patch exists but regression test is old; check installation')
        print('ALREADY INSTALLED: analogBEAST ab2 UI and regression test')
        print('PASS: BRUTE DSP byte-identical with dev5')
        return
    try:
        new = patch_ui(old)
    except ValueError as e:
        sys.exit(f'STOP: source mismatch: {e}; nothing changed')
    assert MARKER in new and 'gu_algo_turn(panel_enc(EN_ALGO));' in new
    assert 'gu_page_visible' in new and 'gu_select_turn(s)' in new
    assert b'select_page(GV_DZ_BANK)' in test_after and b'ALGO during WRITE cannot silently cancel' in test_after
    print('READY: contextual ALGO HOME -> TEMPO, other menus reserved')
    print('READY: SELECT reaches BANK/PATTERN and SONG; SEQ opens ARP')
    print('READY: hide BRUTE-only page in MIKROKORG and inactive mono cards in BRUTE')
    print('READY: updated Stage5 UI regression test')
    print('PASS: BRUTE DSP byte-identical with dev5')
    if args.check:
        print('CHECK ONLY: no files written')
        return
    bak = UI.with_name(UI.name + '.before-analogbeast-ab2')
    test_bak = TEST.with_name(TEST.name + '.before-analogbeast-ab2')
    if bak.exists() or test_bak.exists():
        sys.exit('STOP: existing ab2 backups; no files changed')
    bak.write_text(old, encoding='utf-8')
    test_bak.write_bytes(test_before)
    try:
        UI.write_text(new, encoding='utf-8')
        TEST.write_bytes(test_after)
    except Exception:
        UI.write_text(old, encoding='utf-8')
        TEST.write_bytes(test_before)
        raise
    print(f'PATCHED: {UI.relative_to(PROJECT)}')
    print(f'PATCHED: {TEST.relative_to(PROJECT)}')
    print(f'BACKUP:  {bak.relative_to(PROJECT)}')
    print(f'BACKUP:  {test_bak.relative_to(PROJECT)}')
    print('OK: no build or flash performed')

if __name__ == '__main__':
    main()
