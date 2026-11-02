#!/usr/bin/env python3
# miniBRUTE stage 1: per-instrument SYNTH MODE, legacy-compatible preset, MONO/4 voices.
from pathlib import Path
import argparse, shutil, sys

def replace(s, old, new, file, detail):
    n=s.count(old)
    if n!=1: raise RuntimeError(f'{file}: {detail}: expected 1 anchor, found {n}; no files changed')
    return s.replace(old,new,1)

def main():
    ap=argparse.ArgumentParser()
    g=ap.add_mutually_exclusive_group(required=True)
    g.add_argument('--check',action='store_true'); g.add_argument('--apply',action='store_true')
    args=ap.parse_args()
    paths=['firmware/src/core.h','firmware/src/params.c','firmware/src/golana_prog.c',
           'firmware/src/voice.c','firmware/src/golana_ui.c','tests/run_tests.sh']
    if Path.cwd().name.lower() != 'minibrute':
        raise RuntimeError('Safety guard: cd C:\\FM1\\miniBRUTE before running the patch')
    if not Path('firmware/src/golana_gfx.c').exists():
        raise RuntimeError('Run from C:\\FM1\\miniBRUTE, not the Golana-DZ project')
    original={p:Path(p).read_text(encoding='utf-8') for p in paths}
    changed=dict(original)
    if 'P_GLSYNTH' in original['firmware/src/core.h']:
        raise RuntimeError('Mode already installed; refusing to patch twice')

    p='firmware/src/core.h'
    changed[p]=replace(changed[p],
        '    P_E0, P_E1, P_E2, P_E3, P_E4, P_E5, P_E6, P_E7,',
        '    P_GLSYNTH, /* miniBRUTE: 0 MIKROKORG, 1 BRUTE; placed before E0 for legacy mapping */\n'
        '    P_E0, P_E1, P_E2, P_E3, P_E4, P_E5, P_E6, P_E7,',p,'parameter')

    p='firmware/src/params.c'
    changed[p]=replace(changed[p],
        'static const char *const N_VOICE[] = {"POLY", "MONO", "LEG", "UNI"};   /* V_POLY .. V_UNISON */',
        'static const char *const N_VOICE[] = {"POLY", "MONO", "LEG", "UNI"};   /* V_POLY .. V_UNISON */\n'
        'static const char *const N_GLSYNTH[] = {"MIKROKORG", "BRUTE"};',p,'mode choices')
    changed[p]=replace(changed[p],
        '    [P_ESYNC] = PE("ESYNC", N_ONOFF, 0),          /* 1.5 (#175): OFF = ATK DEC REL in ms, as before (voice.c esync) */',
        '    [P_ESYNC] = PE("ESYNC", N_ONOFF, 0),          /* 1.5 (#175): OFF = ATK DEC REL in ms, as before (voice.c esync) */\n'
        '    [P_GLSYNTH] = PE("SYNTH", N_GLSYNTH, 0),   /* old presets default MIKROKORG */',p,'mode descriptor')

    p='firmware/src/golana_prog.c'
    changed[p]=replace(changed[p],'    p[P_VOICE] = V_POLY;',
        '    p[P_VOICE] = V_POLY;\n'
        '    p[P_GLSYNTH] = 0;                       /* factory sounds use MIKROKORG */',p,'factory default')

    p='firmware/src/voice.c'
    changed[p]=replace(changed[p],
        '    return ENGINES[t->engine]->oneshot ? V_POLY : (uint32_t)t->p[P_VOICE];',
        '#if GOLANA\n'
        '    /* BRUTE is truly mono, but the instrument retains its original ASSIGN setting. */\n'
        '    if (t->engine == ENGI_GOLANA && t->p[P_GLSYNTH] == 1)\n'
        '        return V_MONO;\n'
        '#endif\n'
        '    return ENGINES[t->engine]->oneshot ? V_POLY : (uint32_t)t->p[P_VOICE];',p,'mono override')
    changed[p]=replace(changed[p],
        'uint32_t mode = (uint32_t)t->p[P_VOICE], nv = mode == V_UNISON ? trk_nvoice(t) : 1u, i;',
        'uint32_t mode = trk_vmode(t), nv = mode == V_UNISON ? trk_nvoice(t) : 1u, i;',p,'mono_play mode')

    p='firmware/src/golana_ui.c'
    changed[p]=replace(changed[p],
        '{{P_VOICE, "ASSIGN", GN(N_GVOICE)}, {P_DETUNE, "DETUNE", 0}, {P_GLIDE, "PORTA", 0}, {GL_EGRESET, "EG RESET", 0}}}',
        '{{P_VOICE, "ASSIGN", GN(N_GVOICE)}, {P_DETUNE, "DETUNE", 0}, {P_GLIDE, "PORTA", 0}, {P_GLSYNTH, "SYNTH MODE", 0}}}',p,'VOICE knob 4')
    changed[p]=replace(changed[p],
        '{B_GLO, "SETUP", "SETUP", {{GL_ROUTE, "FX ORDER", 0}, {GV_PAL, "PALETTE", 0}, {GV_VIEW, "HOME", 0}, {GV_NONE, 0, 0}}}',
        '{B_GLO, "SETUP", "SETUP", {{GL_ROUTE, "FX ORDER", 0}, {GV_PAL, "PALETTE", 0}, {GV_VIEW, "HOME", 0}, {GL_EGRESET, "EG RESET", 0}}}',p,'move EG RESET to SETUP')
    changed[p]=replace(changed[p],
        'if (q->id == P_VOICE && t->p[P_VOICE] != v)',
        'if ((q->id == P_VOICE || q->id == P_GLSYNTH) && t->p[q->id] != v)',p,'reset voices on switch')

    testpath='tests/minibrute_synth_mode.c'
    if Path(testpath).exists(): raise RuntimeError(f'{testpath} exists; abort')
    test=r'''/* Native host test: miniBRUTE mode and simulated NOR flash, never physical hardware. */
#include <stdio.h>
#include <string.h>
#include "../web/emu/felucca_web.c"
static int total, failed;
static void check(int ok, const char *msg) {
    total++;
    printf("%s %s\n", ok ? "PASS" : "FAIL", msg);
    if (!ok) failed++;
}
static void advance(unsigned ms) {
    unsigned n = (unsigned)(ms * 44.1 + 0.5);
    while (n) {
        unsigned k = n > 128 ? 128 : n;
        web_render(k);
        n -= k;
    }
}
int main(void) {
    static const gu_par_t synth = {P_GLSYNTH, "SYNTH MODE", 0};
    static const gu_par_t assign = {P_VOICE, "ASSIGN", 0};
    int16_t def[P_COUNT], legacy[P_COUNT - 1], restored[P_COUNT];
    unsigned i;
    int page = 0;
    web_nor_erase(); web_boot(); advance(1600);
    web_keys(1u << 12); advance(100); web_keys(0); advance(300);
    for (i = 0; i < GU_NPAGE; i++)
        if (GU_PAGES[i].btn == B_GLO && GU_PAGES[i].k[3].id == P_GLSYNTH)
            page = 1;
    check(gu.ready && page, "SYNTH MODE on GLOBAL/VOICE knob 4");
    check(gu_get(&synth) == 0 && trk_vmode(gu_t()) == V_POLY,
          "MIKROKORG factory sound is polyphonic");
    gu_set(&synth, 1);
    check(gu_get(&synth) == 1 && trk_vmode(gu_t()) == V_MONO,
          "BRUTE mode forces mono");
    check(gu_t()->p[P_VOICE] == V_POLY, "Stored ASSIGN is not overwritten");
    gu_set(&assign, V_UNISON);
    check(trk_vmode(gu_t()) == V_MONO && gu_t()->p[P_VOICE] == V_UNISON,
          "BRUTE stays mono while preserving UNISON preference");
    gu_set(&synth, 0);
    check(trk_vmode(gu_t()) == V_UNISON, "MIKROKORG restores stored ASSIGN");
    gu_set(&assign, V_POLY); gu_set(&synth, 1);
    check(gu_store(31u, "BRUTE TEST") == 0, "Save BRUTE P32 in simulated NOR");
    gu_load(0u);
    check(gu_get(&synth) == 0 && trk_vmode(gu_t()) == V_POLY,
          "Factory sound restores MIKROKORG");
    gu_load(31u);
    check(gu_get(&synth) == 1 && trk_vmode(gu_t()) == V_MONO,
          "Saved BRUTE mode reloads from simulated NOR");
    gl_defaults(def);
    for (i = 0; i < P_COUNT - 1u - 8u; i++) legacy[i] = def[i];
    for (i = 0; i < 8u; i++) legacy[P_COUNT - 1u - 8u + i] = def[P_E0 + i];
    params_by_count(restored, legacy, P_COUNT - 1u, def);
    check(restored[P_GLSYNTH] == 0 && restored[P_E0] == def[P_E0] &&
          restored[P_E7] == def[P_E7], "Old presets default to MIKROKORG; engine params preserved");
    check(GL_NPROG == 32u && sizeof(up_rec_t) == 192u,
          "32 slots; original 192-byte preset record unchanged");
    printf("\nminiBRUTE SYNTH MODE: %d/%d passed; %d failures\n",
           total - failed, total, failed);
    return failed ? 1 : 0;
}
'''
    p='tests/run_tests.sh'
    old='    $CC $G1 -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/golana_sim" tests/golana_sim.c -lm'
    changed[p]=replace(changed[p],old,
        '    $CC $G1 -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/minibrute_synth_mode" tests/minibrute_synth_mode.c -lm\n'
        '    run "miniBRUTE per-preset SYNTH MODE, mono and legacy records" "$OUT/minibrute_synth_mode"\n'+old,
        p,'include regression test')
    for p in paths:
        print(f'READY  {p}')
        if changed[p]==original[p]: raise RuntimeError(f'{p} was not modified')
    print(f'READY  {testpath} (new)')
    if args.check:
        print('CHECK ONLY: no files changed'); return
    for p in paths:
        backup=Path(p+'.before-synth-mode')
        if backup.exists(): raise RuntimeError(f'Existing backup {backup}, refusing to overwrite')
    for p in paths:
        shutil.copy2(p,p+'.before-synth-mode')
        Path(p).write_text(changed[p],encoding='utf-8')
        print(f'PATCHED {p}')
    Path(testpath).write_text(test,encoding='utf-8')
    print('CREATED '+testpath)
    print('OK. BRUTE mode is currently MONO only; new oscillator/filter/feedback still TODO.')
    print('No firmware built/flashed. Golana-DZ untouched.')

if __name__=='__main__':
    try: main()
    except (RuntimeError,OSError) as e: sys.exit('ERROR: '+str(e))
