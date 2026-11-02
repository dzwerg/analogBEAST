#!/usr/bin/env python3
"""Static verification of the analogBEAST ab2 UI patch, no build/flash."""
from pathlib import Path
import hashlib

root=Path(__file__).resolve().parent.parent
ui=(root/'firmware/src/golana_ui.c').read_text(encoding='utf-8')
test=(root/'tests/dz_stage5_ui.c').read_text(encoding='utf-8')
dsp=(root/'firmware/src/eng_golana.c').read_bytes()
ref=(root/'firmware/src/eng_golana.c.dev5-before-fb-fix.bak').read_bytes()
checks = [
 ('dev5 BRUTE DSP unchanged', dsp == ref),
 ('context ALGO dispatcher', 'gu_algo_turn(panel_enc(EN_ALGO));' in ui),
 ('HOME tempo binding', '{B_HOME, GL_CUTOFF, {GV_BPM, "TEMPO", 0, 0}}' in ui),
 ('SELECT skips hidden pages', 'gu_select_turn(s);' in ui and 'gu_page_visible(page)' in ui),
 ('tabs hide unused pages', '!gu_page_visible(pi)' in ui),
 ('BRUTE unused MONO controls hidden', 'pg->k[0].id == P_VOICE && (k == 0u || k == 1u)' in ui),
 ('WRITE no ALGO cancellation', '(void)panel_enc(EN_ALGO); /* ALGO cannot cancel WRITE */' in ui),
 ('Stage5 regression updated', 'ALGO during WRITE cannot silently cancel' in test),
 ('sequencer untouched in DSP', 'int32_t injection = (fb * (bf * 3)) >> 7;' in ui or b'int32_t injection = (fb * (bf * 3)) >> 7;' in dsp),
]
for label, ok in checks:
 print(('PASS' if ok else 'FAIL') + ': ' + label)
if not all(ok for _,ok in checks):
 raise SystemExit(1)
print('OK: static source checks (not a firmware build or hardware test)')
