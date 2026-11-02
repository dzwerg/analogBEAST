#!/usr/bin/env python3
"""miniBRUTE: SEQ always returns to ARP, even after ALGO/REC.

Only patches firmware/src/golana_ui.c and the matching native UI regression test.
No firmware build, flash operation, preset change or Git operation.
"""
from pathlib import Path
import argparse
import shutil
import sys

ap = argparse.ArgumentParser(description="miniBRUTE SEQ button reset to ARP")
choice = ap.add_mutually_exclusive_group(required=True)
choice.add_argument("--check", action="store_true", help="preview compatible changes")
choice.add_argument("--apply", action="store_true", help="backup and patch sources")
opts = ap.parse_args()

UI = Path("firmware/src/golana_ui.c")
TEST = Path("tests/dz_stage5_ui.c")

BUTTON_OLD = '''    if (b == B_SEQ) {
        if (GU_PAGES[gu.page].btn != B_SEQ) {
            gu_go(gu_dz_seq_page(gu.dz_seq_mode));
            return;
        }
        if (gu.dz_seq_mode != 0)
            return;    /* ALGO alone switches BANK/PATTERN/SONG */
    }
    if (GU_PAGES[gu.page].btn == b) {
        pi = gu.page + 1u;
        if (b == B_SEQ && (pi >= GU_NPAGE || GU_PAGES[pi].btn != B_SEQ ||
                           GU_PAGES[pi].k[0].id == GV_DZ_BANK))
            pi = first;
        gu_go(pi < GU_NPAGE && GU_PAGES[pi].btn == b ? pi : first);
'''
BUTTON_NEW = '''    if (b == B_SEQ) {
        /* SEQ always returns to ARP, the first normal sequencer page.
         * SELECT navigates TIMING / STEPS; ALGO opens DZ BANK / SONG. */
        gu.dz_seq_mode = 0u;
        gu_go(first);
        return;
    }
    if (GU_PAGES[gu.page].btn == b) {
        pi = gu.page + 1u;
        gu_go(pi < GU_NPAGE && GU_PAGES[pi].btn == b ? pi : first);
'''
REC_OLD = '''            if (gu.dz_edit_active) {
                gu_dz_editor_end();    /* commit to RAM bank, restore synth program */
                if (b == B_SEQ) {
                    gu_go(gu_dz_seq_page(1u));
                    break;            /* return to BANK/PATTERN */
                }
            }
            gu_button(b);
'''
REC_NEW = '''            if (gu.dz_edit_active)
                gu_dz_editor_end();    /* commit to RAM bank, restore synth program */
            gu_button(b);             /* SEQ always resets to ARP, including from REC */
'''

TEST_OLD = '''    press(B_HOME);
    press(B_SEQ);
    check(gu.dz_seq_mode == 1 && GU_PAGES[gu.page].k[0].id == GV_DZ_BANK,
          "SEQ returns to the selected BANK/PATTERN page");
    turn(EN_ALGO, 1);
    check(gu.dz_seq_mode == 2 && GU_PAGES[gu.page].k[0].id == GV_DZ_SONG_STEP,
          "ALGO toggles to SONG editor");
'''
TEST_NEW = '''    press(B_HOME);
    press(B_SEQ);
    check(gu.dz_seq_mode == 0 && GU_PAGES[gu.page].k[0].id == GL_ARPON,
          "SEQ always returns to first normal ARP page");
    press(B_SEQ);
    check(gu.dz_seq_mode == 0 && GU_PAGES[gu.page].k[0].id == GL_ARPON,
          "Repeated SEQ stays on first ARP page");
    turn(EN_ALGO, 1);
    check(gu.dz_seq_mode == 1 && GU_PAGES[gu.page].k[0].id == GV_DZ_BANK,
          "ALGO from ARP enters BANK/PATTERN");
    turn(EN_ALGO, 1);
    check(gu.dz_seq_mode == 2 && GU_PAGES[gu.page].k[0].id == GV_DZ_SONG_STEP,
          "ALGO toggles to SONG editor");
    press(B_SEQ);
    check(gu.dz_seq_mode == 0 && GU_PAGES[gu.page].k[0].id == GL_ARPON,
          "SEQ exits SONG menu to first normal ARP page");
    turn(EN_ALGO, 1);
    check(gu.dz_seq_mode == 1 && GU_PAGES[gu.page].k[0].id == GV_DZ_BANK,
          "ALGO reopens BANK after SEQ reset");
    turn(EN_ALGO, 1);
    check(gu.dz_seq_mode == 2 && GU_PAGES[gu.page].k[0].id == GV_DZ_SONG_STEP,
          "ALGO reopens SONG after SEQ reset");
'''
TEST_REC_OLD = '''    check(!gu.dz_edit_active && GU_PAGES[gu.page].k[0].id == GV_DZ_BANK,
          "SEQ exits REC back to BANK/PATTERN");
'''
TEST_REC_NEW = '''    check(!gu.dz_edit_active && gu.dz_seq_mode == 0 && GU_PAGES[gu.page].k[0].id == GL_ARPON,
          "SEQ exits REC to first normal ARP page");
'''
TEST_SELECT_OLD = '''    turn(EN_K2, 1);
    check(dz_model.pattern == 3 &&
'''
TEST_SELECT_NEW = '''    turn(EN_ALGO, 1); /* reopen selected BANK/PATTERN after SEQ reset */
    check(gu.dz_seq_mode == 1 && GU_PAGES[gu.page].k[0].id == GV_DZ_BANK,
          "ALGO after SEQ reset reopens BANK/PATTERN");
    turn(EN_K2, 1);
    check(dz_model.pattern == 3 &&
'''


def patch(text, pairs, name):
    old_count = sum(text.count(old) for old, new in pairs)
    new_count = sum(text.count(new) for old, new in pairs)
    if new_count == len(pairs):
        return "ALREADY OK", text
    if old_count != len(pairs) or new_count:
        problem = [(i + 1, text.count(old), text.count(new)) for i, (old, new) in enumerate(pairs)]
        raise ValueError(f"{name}: unexpected or partially patched version: {problem}")
    for old, new in pairs:
        text = text.replace(old, new, 1)
    return "READY", text

pairs_by_file = {
    UI: [(BUTTON_OLD, BUTTON_NEW), (REC_OLD, REC_NEW)],
    TEST: [(TEST_OLD, TEST_NEW), (TEST_REC_OLD, TEST_REC_NEW),
           (TEST_SELECT_OLD, TEST_SELECT_NEW)],
}

prepared = []
try:
    for path, pairs in pairs_by_file.items():
        original = path.read_text(encoding="utf-8")
        state, updated = patch(original, pairs, str(path))
        print(f"{state:10} {path}")
        prepared.append((path, state, original, updated))
    if opts.check:
        print("OK: source compatibility checked; no changes made.")
        sys.exit(0)
    for path, state, _, _ in prepared:
        backup = path.with_name(path.name + ".before-minibrute-seqfix")
        if state == "READY" and backup.exists():
            raise ValueError(f"Backup already exists: {backup}. Refusing to overwrite.")
    for path, state, original, updated in prepared:
        if state == "READY":
            backup = path.with_name(path.name + ".before-minibrute-seqfix")
            shutil.copy2(path, backup)
            path.write_text(updated, encoding="utf-8", newline="")
            print(f"PATCHED    {path} (backup: {backup})")
    print("OK: SEQ always opens ARP. No firmware built/flashed. No MIDI/flash data touched.")
except (ValueError, OSError) as exc:
    print("STOP: no further changes. " + str(exc), file=sys.stderr)
    sys.exit(1)
