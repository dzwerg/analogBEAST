#!/usr/bin/env python3
"""Safely repair analogBEAST v1.0 REC A-H selection and E-H PAGE 1/2 routing.

Usage:
  py tools/fix_analogbeast_rec_ah.py --check-only
  py tools/fix_analogbeast_rec_ah.py

Only firmware/src/golana_ui.c is changed. No firmware build, USB, MIDI or flash operation.
"""
from __future__ import annotations
import argparse
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "firmware" / "src" / "golana_ui.c"
BACKUP = SOURCE.with_name("golana_ui.c.before-rec-ah-fix")

CHANGES = [
    (
        "REC bank range A-H",
        "case GV_DZ_BANK: *lo = 0; *hi = 3; return;",
        "case GV_DZ_BANK: *lo = 0; *hi = AB5_BANKS - 1; return;",
    ),
    (
        "REC bank value follows unified selection",
        "case GV_DZ_BANK: return dz_model.bank;\n    case GV_DZ_PATTERN: return (int32_t)dz_model.pattern + 1;",
        "case GV_DZ_BANK: return ab5_model.bank;\n    case GV_DZ_PATTERN: return (int32_t)ab5_model.pattern + 1;",
    ),
    (
        "REC bank/pattern selection safely transitions between A-D and E-H",
        '''    case GV_DZ_BANK:
        gu_dz_editor_change_pattern((uint8_t)v, dz_model.pattern);
        gu_dz_mark_dirty(); return;
    case GV_DZ_PATTERN:
        gu_dz_editor_change_pattern(dz_model.bank, (uint8_t)(v - 1));
        gu_dz_mark_dirty(); return;''',
        '''    case GV_DZ_BANK:
        gu_dz_editor_change_pattern((uint8_t)v, ab5_model.pattern);
        return;  /* selecting a bank is not a stored-pattern edit */
    case GV_DZ_PATTERN:
        gu_dz_editor_change_pattern(ab5_model.bank, (uint8_t)(v - 1));
        return;''',
    ),
    (
        "E-H REC page 3 uses the same A-H bank/pattern selectors",
        '''    static const gu_par_t REC5_PATT[4] = {
        {GV_V5_BANK,"BANK",0,0}, {GV_V5_PATTERN,"PATTERN",0,0},
        {GV_V5_EDIT_LENGTH,"LENGTH",0,0}, {GV_V5_EDIT_RES,"RES",0,0}};''',
        '''    static const gu_par_t REC5_PATT[4] = {
        {GV_DZ_BANK,"BANK",0,0}, {GV_DZ_PATTERN,"PATTERN",0,0},
        {GV_V5_EDIT_LENGTH,"LENGTH",0,0}, {GV_V5_EDIT_RES,"RES",0,0}};''',
    ),
    (
        "REC E-H STEP/TIME pages are distinct",
        '''    if (pg->btn==B_REC && gu.v5_rec_active) {
        if (pg->k[0].id==GV_SSTEP) return &REC5_STEP[k];
        if (pg->k[0].id==GV_DZ_BANK) return &REC5_PATT[k];
        return &REC5_TIME[k];
    }''',
        '''    if (pg->btn==B_REC && gu.v5_rec_active) {
        /* Both original STEP and TIME pages use GV_SSTEP as knob 1.
         * Distinguish them by knob 2 instead; never mirror STEP onto TIME. */
        if (pg->k[1].id==GV_SNOTE) return &REC5_STEP[k];
        if (pg->k[1].id==GV_SVELO) return &REC5_TIME[k];
        if (pg->k[0].id==GV_DZ_BANK) return &REC5_PATT[k];
    }''',
    ),
    (
        "REC transition commits A-D only and selects E-H without DZ out-of-bounds",
        '''/* REC 3 bank/pattern selection switches the actual editor buffer, not merely the labels. */
static void gu_dz_editor_change_pattern(uint8_t bank, uint8_t pattern)
{
    uint8_t was_editing = gu.dz_edit_active;
    if (bank == dz_model.bank && pattern == dz_model.pattern) return;
    if (was_editing) gu_dz_editor_end();    /* commit the previous pattern */
    dz_seq_select_pattern(&dz_model, bank, pattern);
    if (was_editing) gu_dz_editor_begin();  /* REC 1/2 now show the newly selected pattern */
    ui.force = 1u;
}''',
        '''/* REC 3: A-D are the original 64-step DZ patterns; E-H are 16-step V5.
 * Commit the old DZ editor before changing bank. Never index DZ with E-H. */
static void gu_dz_editor_change_pattern(uint8_t bank, uint8_t pattern)
{
    if (bank >= AB5_BANKS || pattern >= AB5_PAT_PER_BANK) return;
    if (bank == ab5_model.bank && pattern == ab5_model.pattern) return;
    if (transport_busy() || dz_play_has_backup || gu.save) {
        gu_say("STOP FIRST", 0);
        return;
    }
    if (gu.dz_edit_active) gu_dz_editor_end();  /* commit previous A-D buffer */
    ab5_model.bank = bank;
    ab5_model.pattern = pattern;
    ab5_model.edit_step = 0u;
    gu.v5_rec_active = (uint8_t)(bank >= AB5_EXISTING_BANKS);
    if (!gu.v5_rec_active) {
        dz_seq_select_pattern(&dz_model, bank, pattern);
        gu_dz_editor_begin();               /* load the selected A-D 64-step buffer */
    }
    gu.sstep = 0u;
    ab5_prepared_valid = 0u;
    ui.force = 1u;
}''',
    ),
    (
        "REC entry always uses the selected A-H bank",
        '''                if (ab5_model.play_mode==AB5_MODE_PATTERN &&
                    ab5_model.bank>=AB5_EXISTING_BANKS) {
                    gu.v5_rec_active=1u; /* use v5 16-step editor on existing REC button */
                } else {
                    gu.v5_rec_active=0u;
                    if (ab5_model.play_mode==AB5_MODE_PATTERN &&
                        ab5_model.bank<AB5_EXISTING_BANKS)
                        dz_seq_select_pattern(&dz_model,ab5_model.bank,ab5_model.pattern);
                    gu_dz_editor_begin(); /* A-D: unchanged v4 editor */
                }''',
        '''                if (ab5_model.bank>=AB5_EXISTING_BANKS) {
                    gu.v5_rec_active=1u; /* E-H: 16-step editor */
                } else {
                    gu.v5_rec_active=0u;
                    dz_seq_select_pattern(&dz_model,ab5_model.bank,ab5_model.pattern);
                    gu_dz_editor_begin(); /* A-D: original 64-step editor */
                }''',
    ),
]


def patched(src: str) -> tuple[str, list[str]]:
    old_found = []
    new_found = []
    for label, before, after in CHANGES:
        b, a = src.count(before), src.count(after)
        if b == 1 and a == 0:
            old_found.append(label)
        elif b == 0 and a == 1:
            new_found.append(label)
        else:
            raise ValueError(f"REFUSED: {label}: expected exactly one old or new anchor (old={b}, new={a}); source left untouched")
    if old_found and new_found:
        raise ValueError("REFUSED: partially applied/modified REC patch; source left untouched")
    if new_found:
        return src, []
    out = src
    for _, before, after in CHANGES:
        out = out.replace(before, after, 1)
    return out, old_found


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--check-only", action="store_true")
    opts = ap.parse_args()
    if not SOURCE.is_file():
        print("REFUSED: missing firmware/src/golana_ui.c"); return 2
    original = SOURCE.read_bytes()
    try:
        text = original.decode("utf-8")
        crlf = b"\r\n" in original
        updated, changes = patched(text.replace("\r\n", "\n"))
        if crlf:
            updated = updated.replace("\n", "\r\n")
    except (ValueError, UnicodeDecodeError) as exc:
        print(exc); return 2
    if not changes:
        print("ALREADY: A-H REC and distinct E-H STEP/TIME pages already fixed; no changes made")
        return 0
    if opts.check_only:
        print(f"PASS: {len(changes)} verified REC fixes ready; no files modified; no MIDI/flash")
        return 0
    if BACKUP.exists() and BACKUP.read_bytes() != original:
        print(f"REFUSED: previous backup differs: {BACKUP.name}; no overwrite")
        return 2
    if not BACKUP.exists():
        BACKUP.write_bytes(original)
    SOURCE.write_bytes(updated.encode("utf-8"))
    print(f"PATCHED: firmware/src/golana_ui.c; backup: {BACKUP.name}")
    print("PASS: REC A-H selection, 64/16-step editor routing, separate E-H STEP/TIME")
    print("NO BUILD, NO USB, NO MIDI, NO FLASH, NO NOR OPERATIONS")
    return 0

if __name__ == "__main__":
    sys.exit(main())
