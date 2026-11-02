#!/usr/bin/env python3
"""analogBEAST ab2.2 navigation repair; exact ab2 source matching, safe backups.

--check: inspect without writing
--apply: modify UI and Stage5 regression tests only
--restore: restore those two files from ab2.2 navigation backups
"""
from __future__ import annotations
import argparse
import hashlib
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parent.parent
UI = ROOT / "firmware/src/golana_ui.c"
TEST = ROOT / "tests/dz_stage5_ui.c"
DSP = ROOT / "firmware/src/eng_golana.c"
DEV5 = ROOT / "firmware/src/eng_golana.c.dev5-before-fb-fix.bak"
MARKER = "analogBEAST ab2.2: navigation repair - SELECT preserved, SEQ cycles"
BACKUP_SUFFIX = ".before-analogbeast-ab22-nav"


def replace_once(source: str, before: str, after: str, what: str) -> str:
    count = source.count(before)
    if count != 1:
        raise ValueError(f"{what}: expected one exact occurrence, found {count}")
    return source.replace(before, after, 1)


def select_behavior(source: str) -> bytes:
    a = source.index("static void gu_select_turn(int32_t steps)")
    b = source.index("/* DZ Stage 3:", a)
    sub = source[a:b]
    # SELECT is and remains the global navigator, through every visible page.
    assert "if (gu_page_visible(page)) break;" in sub
    assert "gu_go(page);" in sub
    assert "s = panel_enc(EN_SELECT);\n        if (s)\n            gu_select_turn(s);" in source
    return sub.encode('utf-8')


def patch_ui(source: str) -> str:
    if MARKER in source:
        raise ValueError("navigation repair already present")
    if "analogBEAST ab2: per-engine navigation and contextual ALGO" not in source:
        raise ValueError("requires analogBEAST ab2; ab2 source marker missing")
    if "analogBEAST ab2.1 SEQ/UI" in source or "analogBEAST ab2.2 SEQ/SELECT/UI" in source:
        raise ValueError("different SEQ patch already installed; stop to avoid mixing versions")
    source = replace_once(source,
        "/* analogBEAST ab2: per-engine navigation and contextual ALGO */",
        "/* analogBEAST ab2: per-engine navigation and contextual ALGO */\n/* " + MARKER + " */",
        "marker")
    source = replace_once(source, '''    if (b == B_SEQ) {
        /* SEQ resets to ARP; SELECT alone navigates TIMING / STEPS / BANK / SONG. */
        gu.dz_seq_mode = 0u;
        gu_go(first);
        return;
    }
''', '''    /* SEQ follows the same page-cycling behavior as every other menu button.
     * From a different menu it opens ARP. Repeated presses visit TIMING,
     * STEPS, BANK/PATTERN, SONG, then ARP. SELECT remains unchanged. */
''', 'SEQ forced reset')
    source = replace_once(source,
        'gu_button(b);             /* SEQ always resets to ARP, including from REC */',
        'gu_button(b);             /* SEQ cycles within pages; from REC enters ARP */',
        'SEQ input comment')
    return source


def patch_test(source: str) -> str:
    before = '''    press(B_SEQ);
    check(gu.dz_seq_mode == 0 && GU_PAGES[gu.page].k[0].id == GL_ARPON,
          "SEQ opens ARP");
    press(B_SEQ);
    check(gu.dz_seq_mode == 0 && GU_PAGES[gu.page].k[0].id == GL_ARPON,
          "Repeated SEQ stays on ARP");
    turn(EN_ALGO, 1);
    check(GU_PAGES[gu.page].k[0].id == GL_ARPON,
          "ALGO in ARP does not navigate");'''
    after = '''    /* SELECT remains the global navigator across menu groups in either direction.
     * Hidden engine-only pages are skipped; no special SELECT -> SONG shortcut. */
    press(B_HOME);
    turn(EN_SELECT, 1);
    check(GU_PAGES[gu.page].btn == B_EDIT && GU_PAGES[gu.page].k[0].id == GL_O1WAVE,
          "SELECT goes HOME -> OSC 1");
    turn(EN_SELECT, -1);
    check(gu.page == 0 && GU_PAGES[gu.page].btn == B_HOME,
          "SELECT goes OSC 1 -> HOME");
    select_page(GL_FTYPE);
    turn(EN_SELECT, 1);
    check(GU_PAGES[gu.page].k[0].id == GL_KTRK,
          "SELECT goes FILTER -> KEY TRACK");
    turn(EN_SELECT, 1);
    check(GU_PAGES[gu.page].k[0].id == GL_FA,
          "SELECT skips hidden BRUTE page in MIKROKORG");
    press(B_HOME);

    press(B_SEQ);
    check(gu.dz_seq_mode == 0 && GU_PAGES[gu.page].k[0].id == GL_ARPON,
          "SEQ opens ARP");
    press(B_SEQ);
    check(gu.dz_seq_mode == 0 && GU_PAGES[gu.page].k[0].id == P_AGATE,
          "SEQ advances to TIMING");
    press(B_SEQ);
    check(gu.dz_seq_mode == 0 && GU_PAGES[gu.page].k[0].id == GL_ARPLAST,
          "SEQ advances to STEPS");
    press(B_SEQ);
    check(gu.dz_seq_mode == 1 && GU_PAGES[gu.page].k[0].id == GV_DZ_BANK,
          "SEQ advances to BANK/PATTERN");
    press(B_SEQ);
    check(gu.dz_seq_mode == 2 && GU_PAGES[gu.page].k[0].id == GV_DZ_SONG_STEP,
          "SEQ advances to SONG");
    press(B_SEQ);
    check(gu.dz_seq_mode == 0 && GU_PAGES[gu.page].k[0].id == GL_ARPON,
          "SEQ wraps SONG -> ARP");
    turn(EN_ALGO, 1);
    check(GU_PAGES[gu.page].k[0].id == GL_ARPON,
          "ALGO in ARP does not navigate");
    /* Physical ARP button retains its original toggle function. */
    int arp_initial = gu_t()->p[GL_ARPON];
    press(B_ARP);
    check(gu_t()->p[GL_ARPON] != arp_initial,
          "ARP physical button toggles ARP");
    press(B_ARP);
    check(gu_t()->p[GL_ARPON] == arp_initial,
          "ARP physical button toggles back");'''
    return replace_once(source, before, after, 'SEQ/SELECT/ARP tests')


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument('--check', action='store_true')
    group.add_argument('--apply', action='store_true')
    group.add_argument('--restore', action='store_true')
    args = parser.parse_args()

    for file in (UI, TEST, DSP, DEV5):
        if not file.is_file():
            sys.exit(f"STOP: missing {file}")
    if DSP.read_bytes() != DEV5.read_bytes():
        sys.exit("STOP: BRUTE DSP differs from dev5 backup. Nothing changed.")
    backups = [(file, file.with_name(file.name + BACKUP_SUFFIX)) for file in (UI, TEST)]

    if args.restore:
        if any(not backup.is_file() for _, backup in backups):
            sys.exit("STOP: repair backups missing; cannot restore")
        if MARKER not in UI.read_text(encoding='utf-8'):
            sys.exit("STOP: installed patch marker missing; refusing to overwrite")
        for file, backup in backups:
            file.write_bytes(backup.read_bytes())
            print(f"RESTORED: {file.relative_to(ROOT)}")
        print("PASS: back to ab2 UI, dev5 BRUTE untouched")
        return

    ui = UI.read_text(encoding='utf-8')
    test = TEST.read_text(encoding='utf-8')
    if MARKER in ui:
        if "SEQ advances to TIMING" not in test:
            sys.exit("STOP: partially installed patch")
        print("ALREADY INSTALLED: analogBEAST ab2.2 navigation repair")
        print("PASS: BRUTE dev5 unchanged")
        return
    try:
        old_select = select_behavior(ui)
        new_ui = patch_ui(ui)
        new_test = patch_test(test)
        if select_behavior(new_ui) != old_select:
            raise ValueError("SELECT behavior would change")
    except (ValueError, AssertionError) as err:
        sys.exit(f"STOP: source mismatch: {err}; nothing changed")
    print("READY: SELECT code preserved byte-for-byte (global menu navigator)")
    print("READY: SEQ cycles ARP / TIMING / STEPS / BANK / SONG")
    print("READY: ARP on/off button, ALGO HOME BPM, REC/WRITE kept intact")
    print("READY: Stage5 regression adds navigation/ARP tests")
    print("PASS: BRUTE dev5 byte-identical")
    if args.check:
        print("CHECK ONLY: no files written")
        return
    if any(backup.exists() for _, backup in backups):
        sys.exit("STOP: existing navigation backup(s); manual inspection required")
    for file, backup in backups:
        backup.write_bytes(file.read_bytes())
    try:
        UI.write_text(new_ui, encoding='utf-8')
        TEST.write_text(new_test, encoding='utf-8')
    except Exception:
        for file, backup in backups:
            file.write_bytes(backup.read_bytes())
        raise
    print("PATCHED: firmware/src/golana_ui.c")
    print("PATCHED: tests/dz_stage5_ui.c")
    print("BACKUP:  *.before-analogbeast-ab22-nav")
    print("OK: build and flash not performed")


if __name__ == '__main__':
    main()
