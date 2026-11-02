#!/usr/bin/env python3
"""miniBRUTE Stage 3: Steiner-Parker-inspired nonlinear 2-pole LP/BP/HP (BRUTE only).

Run from C:\\FM1\\miniBRUTE after Stage 2.  No hardware flash/writes.
"""
import argparse
from pathlib import Path
import shutil
import sys


def replace(s, old, new, path, task):
    count = s.count(old)
    if count != 1:
        raise RuntimeError(f'{path}: {task}: expected 1 anchor, found {count}; nothing changed')
    return s.replace(old, new, 1)


HOST_TEST = r'''/* miniBRUTE Stage 3: dedicated 2-pole nonlinear BRUTE multimode filter.
 * Native host simulation; no physical FM-1 is written. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "../web/emu/felucca_web.c"

static unsigned total, passed;
static void ck(int yes, const char *name)
{
    total++;
    passed += yes != 0;
    printf("%s %s\n", yes ? "PASS" : "FAIL", name);
}
static void advance(unsigned ms)
{
    unsigned n = (unsigned)(ms * 44.1 + 0.5);
    while (n) {
        unsigned k = n > 128 ? 128 : n;
        web_render(k);
        n -= k;
    }
}
static void render_case(int brute, int spmode, int factor, int resonance,
                        int cutoff, int korg_filter, int32_t *samples, unsigned n)
{
    track_t *t = gu_t();
    voice_t *v = &t->v[0];
    glp_t *part = gl_part(t);
    vmod_t m;
    memset(&part->v[0], 0, sizeof part->v[0]);
    memset(v, 0, sizeof *v);
    memset(&m, 0, sizeof m);
    memset(samples, 0, n * sizeof *samples);
    part->seed = 0x5EED1234;
    part->inc1 = part->inc2 = 0;
    v->note = 60;
    v->vel = 100;
    v->gate = v->active = 1;
    v->ph[1] = 0x40000000u;
    m.pitch16 = 60 * 16;
    m.inc = pitch_inc(m.pitch16);
    m.amp0 = m.amp1 = m.envq15 = 32767;
    t->p[P_GLSYNTH] = (int16_t)brute;
    t->p[P_GLSPMODE] = (int16_t)spmode;
    t->p[P_GLBRUTE] = (int16_t)factor;
    t->p[P_VOICE] = V_MONO;
    t->p[GL_O1WAVE] = 0;
    t->p[GL_MIX1] = 127;
    t->p[GL_MIX2] = 0;
    t->p[GL_NOISE] = 0;
    t->p[GL_FOLD] = 0;
    t->p[GL_DIST] = 0;
    t->p[GL_FTYPE] = (int16_t)korg_filter;
    t->p[GL_CUTOFF] = (int16_t)cutoff;
    t->p[GL_RESO] = (int16_t)resonance;
    t->p[GL_FEGINT] = 0;
    t->p[GL_KTRK] = 0;
    for (unsigned k = 0; k < 4u; k++)
        t->p[GL_P1INT + 3u*k] = 0;
    gol_render(t, v, samples, n, &m);
}
static int differs(const int32_t *a, const int32_t *b, unsigned n)
{
    return memcmp(a, b, n * sizeof *a) != 0;
}
int main(void)
{
    int32_t klp[256], kbp[256], kbrute_on[256];
    int32_t lp[256], bp[256], hp[256], extreme[256];
    int16_t def[P_COUNT], prev[P_COUNT - 1], mapped[P_COUNT];
    unsigned i, pg = GU_NPAGE, bounded = 1, active = 0;
    static const gu_par_t mode = {P_GLSPMODE, "TYPE", 0};
    web_nor_erase();
    web_boot();
    advance(1600);
    web_keys(1u << 12); advance(100); web_keys(0); advance(300);

    for (i = 0; i < GU_NPAGE; i++)
        if (GU_PAGES[i].btn == B_SCL && GU_PAGES[i].k[0].id == GL_FTYPE) { pg = i; break; }
    ck(gu.ready && pg < GU_NPAGE, "Existing FILTER page still exists");
    if (pg == GU_NPAGE) return 1;
    gu_t()->p[P_GLSYNTH] = 0;
    ck(gu_pk(&GU_PAGES[pg], 0)->id == GL_FTYPE, "MIKROKORG filter TYPE control unchanged");
    gu_t()->p[P_GLSYNTH] = 1;
    ck(gu_pk(&GU_PAGES[pg], 0)->id == P_GLSPMODE, "BRUTE filter TYPE selects Steiner LP/BP/HP");
    ck(gu_get(&mode) == 0, "Existing BRUTE preset defaults to Steiner LP");
    gu_set(&mode, 2);
    ck(gu_get(&mode) == 2 && gu_t()->p[GL_FTYPE] >= 0,
       "HP mode editable without replacing MIKROKORG filter TYPE");

    render_case(0, 0, 0, 65, 70, 0, klp, 256);
    render_case(0, 2, 127, 65, 70, 0, kbrute_on, 256);
    ck(!differs(klp, kbrute_on, 256), "MIKROKORG ignores Steiner TYPE and BRUTE FACTOR");
    render_case(0, 0, 0, 65, 70, 2, kbp, 256);
    ck(differs(klp, kbp, 256), "MIKROKORG retains original BPF");
    render_case(1, 0, 0, 65, 70, 0, lp, 256);
    render_case(1, 1, 0, 65, 70, 0, bp, 256);
    render_case(1, 2, 0, 65, 70, 0, hp, 256);
    ck(differs(lp, bp, 256) && differs(bp, hp, 256) && differs(lp, hp, 256),
       "Steiner LP, BP and HP produce different audio");
    ck(differs(klp, lp, 256), "BRUTE Steiner is a distinct nonlinear DSP path");
    render_case(1, 2, 0, 65, 70, 4, extreme, 256);
    ck(!differs(hp, extreme, 256), "BRUTE filter ignores stored MIKROKORG MS20 type");

    /* Stress: maximal resonance, drive and feedback at low/mid/high cutoff. */
    for (int ft = 0; ft < 3; ft++)
        for (int cut = 0; cut <= 127; cut += 63) {
            render_case(1, ft, 127, 127, cut, 4, extreme, 256);
            for (i = 0; i < 256; i++) {
                if (extreme[i] > 100000 || extreme[i] < -100000) bounded = 0;
                if (extreme[i]) active = 1;
            }
        }
    ck(bounded && active, "Max resonance plus Brute Factor remains bounded in all 3 modes");

    gu_t()->p[P_GLSYNTH] = 1;
    gu_set(&mode, 1);
    gu_t()->p[P_GLBRUTE] = 85;
    ck(gu_store(31u, "STEINER BP") == 0, "Steiner program writes to simulated NOR P32");
    gu_load(0);
    ck(gu_t()->p[P_GLSPMODE] == 0 && gu_t()->p[P_GLSYNTH] == 0,
       "Old factory program defaults to MIKROKORG and Steiner LP");
    gu_load(31u);
    ck(gu_t()->p[P_GLSPMODE] == 1 && gu_t()->p[P_GLSYNTH] == 1 && gu_t()->p[P_GLBRUTE] == 85,
       "Steiner BP, BRUTE and factor reload together from NOR");

    gl_defaults(def);
    for (i = 0; i < P_COUNT - 1u - 8u; i++) prev[i] = def[i];
    prev[P_GLSYNTH] = 1;
    prev[P_GLBRUTE] = 95;
    for (i = 0; i < 8; i++) prev[P_COUNT-1u-8u+i] = def[P_E0+i];
    params_by_count(mapped, prev, P_COUNT - 1u, def);
    ck(mapped[P_GLSYNTH] == 1 && mapped[P_GLBRUTE] == 95 &&
       mapped[P_GLSPMODE] == 0 && mapped[P_E0] == def[P_E0] && mapped[P_E7] == def[P_E7],
       "Stage 2 saved sounds map to Steiner LP without losing feedback or OSC parameters");
    ck(GL_NPROG == 32u && sizeof(up_rec_t) == 192u,
       "Exactly 32 instrument slots; NOR record remains 192 bytes");
    printf("\nminiBRUTE STEINER Stage 3: %u/%u passed, %u failures\n",
           passed, total, total-passed);
    return passed != total;
}
'''


def main():
    ap = argparse.ArgumentParser()
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument('--check', action='store_true')
    g.add_argument('--apply', action='store_true')
    opts = ap.parse_args()
    if Path.cwd().name.lower() != 'minibrute' or not Path('firmware/src/golana_gfx.c').is_file():
        raise RuntimeError('Safety guard: run only from C:\\FM1\\miniBRUTE')
    files = ['firmware/src/core.h', 'firmware/src/params.c', 'firmware/src/golana_prog.c',
             'firmware/src/golana_ui.c', 'firmware/src/golana_gfx.c',
             'firmware/src/eng_golana.c', 'tests/run_tests.sh',
             'tests/minibrute_brute_factor.c']
    orig = {f: Path(f).read_text(encoding='utf-8') for f in files}
    out = dict(orig)
    if 'P_GLBRUTE' not in orig['firmware/src/core.h'] or 'P_GLSYNTH' not in orig['firmware/src/core.h']:
        raise RuntimeError('Stage 1 + 2 must be installed before Steiner filter')
    if 'P_GLSPMODE' in orig['firmware/src/core.h']:
        raise RuntimeError('Stage 3 already appears installed; stopping')
    if not Path('tests/minibrute_brute_factor.c').exists():
        raise RuntimeError('Stage 2 host test missing; refusing to apply')
    testpath = Path('tests/minibrute_steiner.c')
    if testpath.exists():
        raise RuntimeError('Stage 3 test already exists; refusing to overwrite')

    p = 'firmware/src/core.h'
    out[p] = replace(out[p],
        '    P_GLBRUTE, /* miniBRUTE: per-sound BRUTE FACTOR, 0..127; old sounds default 0 */',
        '    P_GLBRUTE, /* miniBRUTE: per-sound BRUTE FACTOR, 0..127; old sounds default 0 */\n'
        '    P_GLSPMODE, /* BRUTE Steiner LP/BP/HP; old sounds default LP */',
        p, 'append SP mode before E0')
    # track_t must remain a multiple of 16 bytes for the target compiler.
    out[p] = replace(out[p], 'uint8_t rsv_sz[10];', 'uint8_t rsv_sz[8];', p, 'reclaim 2 padding bytes')

    p = 'firmware/src/params.c'
    out[p] = replace(out[p],
        'static const char *const N_VOICE[] = {"POLY", "MONO", "LEG", "UNI"};   /* V_POLY .. V_UNISON */',
        'static const char *const N_VOICE[] = {"POLY", "MONO", "LEG", "UNI"};   /* V_POLY .. V_UNISON */\n'
        'static const char *const N_GLSPMODE[] = {"LP", "BP", "HP"};',
        p, 'Steiner filter names')
    out[p] = replace(out[p],
        '    [P_GLBRUTE] = PD("BFACT", F_PCT, 0, 127, 0),',
        '    [P_GLBRUTE] = PD("BFACT", F_PCT, 0, 127, 0),\n'
        '    [P_GLSPMODE] = PE("SPMODE", N_GLSPMODE, 0),',
        p, 'persisted filter selection descriptor')

    p = 'firmware/src/golana_prog.c'
    out[p] = replace(out[p],
        '    p[P_GLBRUTE] = 0;                     /* no feedback on all factory sounds */',
        '    p[P_GLBRUTE] = 0;                     /* no feedback on all factory sounds */\n'
        '    p[P_GLSPMODE] = 0;                   /* new/old BRUTE sounds default to LP */',
        p, 'preset default')

    p = 'firmware/src/golana_ui.c'
    out[p] = replace(out[p],
        '    static const gu_par_t METAL[3] = {{P_IA, "GAIN", 0}, {P_IB, "MID", 0}, {P_IC, "MID FREQ", 0}};',
        '    static const gu_par_t METAL[3] = {{P_IA, "GAIN", 0}, {P_IB, "MID", 0}, {P_IC, "MID FREQ", 0}};\n'
        '    static const gu_par_t STEINER_TYPE = {P_GLSPMODE, "TYPE", 0};\n'
        '    /* The FILTER page switches only its first knob; MIKROKORG preserves GL_FTYPE. */\n'
        '    if (pg->k[0].id == GL_FTYPE && k == 0 && gu_t()->p[P_GLSYNTH] == 1)\n'
        '        return &STEINER_TYPE;',
        p, 'mode-aware filter type control')

    p = 'firmware/src/golana_gfx.c'
    out[p] = replace(out[p],
        '    int32_t ft = clamp(p[GL_FTYPE], 0, 4), cut = clamp(p[GL_CUTOFF], 0, 127), res = clamp(p[GL_RESO], 0, 127);',
        '    int32_t ft = p[P_GLSYNTH] == 1\n'
        '        ? clamp(p[P_GLSPMODE], 0, 2) + 1  /* map LP/BP/HP to the plot below */\n'
        '        : clamp(p[GL_FTYPE], 0, 4);\n'
        '    int32_t cut = clamp(p[GL_CUTOFF], 0, 127), res = clamp(p[GL_RESO], 0, 127);',
        p, 'dynamic filter graph')
    # Plot's existing indices 0 LP24, 1 LP12, 2 BP, 3 HP, 4 MS20
    out[p] = replace(out[p],
        'GL_O2SEMI, GL_FTYPE, GL_CUTOFF, GL_RESO,',
        'GL_O2SEMI, GL_FTYPE, P_GLSYNTH, P_GLSPMODE, GL_CUTOFF, GL_RESO,',
        p, 'redraw filter when BRUTE mode/type changes')

    p = 'firmware/src/eng_golana.c'
    before = 'static void gol_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)'
    addition = '''/* Steiner-Parker-inspired *digital* 2-pole filter, not a circuit-level diode model.
 * Trapezoidal SVF core: LP/BP/HP taps, soft-saturated integrator feedback and input.
 * Both integration states stay bounded even at maximum resonance + Brute Factor.
 * Unlike the MIKROKORG SVF it is intentionally nonlinear; MIDI/FX are not fed back.
 * Input is Q15-ish, coefficients are Q13, resonance damping k is Q12. */
static inline int32_t gl_steiner(const tsvf_t *c, int32_t k, int32_t in,
                                 int32_t *ic1, int32_t *ic2, uint32_t mode)
{
    int32_t x = softclip(clamp(in * 2, -65535, 65535)) >> 1;
    int32_t v3 = x - *ic2;
    int32_t v1 = (c->a1 * *ic1 + c->a2 * v3) >> 13;
    int32_t v2 = *ic2 + ((c->a2 * *ic1 + c->a3 * v3) >> 13);
    *ic1 = softclip(clamp(2 * v1 - *ic1, -65535, 65535)); /* nonlinear BP feedback */
    *ic2 = clamp(2 * v2 - *ic2, -65535, 65535);
    return mode == 0u ? v2 : mode == 1u ? v1 : x - ((k * v1) >> 12) - v2;
}

'''
    out[p] = replace(out[p], before, addition + before, p, 'add nonlinear Steiner core')
    out[p] = replace(out[p],
        '    int32_t bf = p[P_GLSYNTH] == 1 ? gp(t, P_GLBRUTE, 0, 127) : 0;',
        '    int32_t bf = p[P_GLSYNTH] == 1 ? gp(t, P_GLBRUTE, 0, 127) : 0;\n'
        '    int32_t sp_mode = p[P_GLSYNTH] == 1 ? gp(t, P_GLSPMODE, 0, 2) : -1;',
        p, 'enable Steiner only in BRUTE mode')
    out[p] = replace(out[p],
        '            switch (ft) {\n            case 0:',
        '            if (sp_mode >= 0) {\n'
        '                y = gl_steiner(&f2c, k2, sx, &b1, &b2, (uint32_t)sp_mode);\n'
        '            } else {\n'
        '            switch (ft) {\n            case 0:',
        p, 'dispatch BRUTE vs original microKORG filter')
    out[p] = replace(out[p],
        '            }\n            y = soft_knee(y, 16000) << 1;',
        '            }\n            }\n            y = soft_knee(y, 16000) << 1;',
        p, 'close original microKORG switch branch')

    p = 'tests/minibrute_brute_factor.c'
    out[p] = replace(out[p],
        '    check(!memcmp(korg0, brute0, sizeof korg0), "BRUTE factor 0 is sample-identical to legacy DSP");',
        '    check(memcmp(korg0, brute0, sizeof korg0) != 0,\n'
        '          "BRUTE factor 0 selects Steiner filter (not legacy DSP)");',
        p, 'update old Stage 2 DSP regression for new filter topology')

    p = 'tests/run_tests.sh'
    out[p] = replace(out[p],
        '    run "miniBRUTE BRUTE FACTOR feedback, DSP bypass, NOR and legacy presets" "$OUT/minibrute_brute_factor"',
        '    run "miniBRUTE BRUTE FACTOR feedback, DSP bypass, NOR and legacy presets" "$OUT/minibrute_brute_factor"\n'
        '    $CC $G1 -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/minibrute_steiner" tests/minibrute_steiner.c -lm\n'
        '    run "miniBRUTE Steiner LP/BP/HP, DSP isolation and legacy presets" "$OUT/minibrute_steiner"',
        p, 'register Stage 3 native host test')

    for f in files:
        if out[f] == orig[f]:
            raise RuntimeError(f'Internal: no change for {f}')
        print('READY ', f)
    print('READY ', testpath, '(new)')
    if opts.check:
        print('CHECK ONLY: source anchors match; no files modified')
        return
    for f in files:
        if Path(f + '.before-steiner-stage3').exists():
            raise RuntimeError('Backup exists for ' + f + '; stopping')
    for f in files:
        shutil.copy2(f, f + '.before-steiner-stage3')
        Path(f).write_text(out[f], encoding='utf-8')
        print('PATCHED', f)
    testpath.write_text(HOST_TEST, encoding='utf-8')
    print('CREATED', testpath)
    print('DONE: Stage 3 installed locally. Run build and host tests; do not flash yet.')

if __name__ == '__main__':
    try:
        main()
    except (RuntimeError, OSError) as exc:
        sys.exit('ERROR: ' + str(exc))
