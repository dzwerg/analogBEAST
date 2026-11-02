#!/usr/bin/env python3
"""Patch only the miniBRUTE splash screen in the Golana-derived firmware."""
from pathlib import Path
import argparse
import re
import shutil

p = argparse.ArgumentParser(description="miniBRUTE boot splash only; preserves all other firmware")
p.add_argument("--check", action="store_true", help="check whether this source version is supported")
p.add_argument("--apply", action="store_true", help="backup and patch the splash")
a = p.parse_args()
if a.apply == a.check:
    p.error("choose either --check or --apply")

src = Path("firmware/src/golana_gfx.c")
if not src.is_file():
    p.error("run from C:\\FM1\\miniBRUTE (firmware/src/golana_gfx.c missing)")
old = src.read_text(encoding="utf-8")
if "GSP_GLYPH[9][7]" in old and '"miniBRUTE \\xB7 by datenzwerg"' in old:
    print("ALREADY OK: miniBRUTE splash installed")
    raise SystemExit(0)

# 5x7 bitmap alphabet with 9 letters: lowercase mini + uppercase BRUTE.
# x=8..227 within the 240-pixel LCD; original stripe/glint/wipe animation unchanged.
new_glyphs = '''static const uint8_t GSP_GLYPH[9][7] = {      /* miniBRUTE, 5 x 7, bit 4 = left */
    {0x00, 0x00, 0x1A, 0x15, 0x15, 0x15, 0x15}, /* m */
    {0x04, 0x00, 0x0C, 0x04, 0x04, 0x04, 0x0E}, /* i */
    {0x00, 0x00, 0x1E, 0x11, 0x11, 0x11, 0x11}, /* n */
    {0x04, 0x00, 0x0C, 0x04, 0x04, 0x04, 0x0E}, /* i */
    {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}, /* B */
    {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}, /* R */
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, /* U */
    {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}, /* T */
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}, /* E */
};'''

s, n = re.subn(r'static const uint8_t GSP_GLYPH\[6\]\[7\] = \{[^;]*?^\};',
               lambda _: new_glyphs, old, count=1, flags=re.M | re.S)
if n != 1:
    p.error("glyph table unexpected; no changes made")

replacements = [
    ("for (k = 0; k < 6; k++)", "for (k = 0; k < 9; k++)"),
    ("int32_t px = 15 + k * 36 + x * 6, d = px - sweep;",
     "int32_t px = 8 + k * 25 + x * 4, d = px - sweep;"),
    ("cv_rect(px, 75 + y * 6, 5, 5,", "cv_rect(px, 75 + y * 6, 3, 5,"),
    ('static const char V[] = FELUCCA_VERSION " \\xB7 by Gadlele";',
     'static const char V[] = "miniBRUTE \\xB7 by datenzwerg";'),
    ("GOLANA: 6 x 6 px blocks (5 lit), 36 px a letter, from 15, 75",
     "miniBRUTE: 9 letters, 4 px column spacing, from x=8"),
]
for find, replace in replacements:
    count = s.count(find)
    if count != 1:
        p.error(f"expected exactly one occurrence of {find!r}; found {count}; no changes made")
    s = s.replace(find, replace, 1)

if a.check:
    print("READY: firmware/src/golana_gfx.c (bootlogo GOLANA -> miniBRUTE)")
    print("CHECK ONLY: no files changed")
else:
    backup = src.with_name(src.name + ".before-minibrute-bootlogo")
    if backup.exists():
        p.error(f"backup already exists: {backup}; no changes made")
    shutil.copy2(src, backup)
    src.write_text(s, encoding="utf-8")
    print("OK: miniBRUTE splash installed")
    print("Backup:", backup)
    print("Only the boot splash was changed. No firmware built or flashed.")
