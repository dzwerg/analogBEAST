#!/usr/bin/env python3
"""Defensive source checks, not a target-CPU build or hardware test."""
from pathlib import Path
r=Path(__file__).resolve().parent.parent
s=r/'firmware/src'
st=(s/'golana_dz_store.c').read_text()
md=(s/'golana_dz_midi.c').read_text()
ui=(s/'golana_ui.c').read_text()
assert 'dz_store_write_locked = 1u; dz_store_import_pending = 0u;' in st
assert 'dz_store_write_locked || !dz_store_dirty' in st
assert 'dz_store_import_pending = 1u;' in st
assert 'dz_store_dirty = 0u;' in st
assert 'dz_store_explicit_commit(' in st
assert st.index('dz_store_write_locked = 0u;',st.index('static uint8_t dz_store_explicit_commit')) > st.index('if (rc || !dz_store_nor.valid)')
assert 'op == 18u && n == 2u' in md and 'op == 19u' in md
assert "'C','O','M','M','I','T',' ','V','4'" in md
assert 'dz_store_poll(&dz_model, 1)' in md
assert 'dz_store_write_locked) { out[2] = 3;' in md
assert 'dz_store_import_pending' in ui and 'IMPORT RAM ONLY' in ui
assert 'STR2[] = {16, 3' in (s/'usb.c').read_text()
assert (s/'eng_golana.c').read_bytes()==(s/'eng_golana.c.dev5-before-fb-fix.bak').read_bytes()
assert 'DZ_SONG_GATE' not in md
print('PASS Stage6 source: safe first-boot RAM import, locked poll, explicit MIDI commit, UI warning, unchanged BRUTE/USB')
