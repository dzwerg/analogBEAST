#!/usr/bin/env python3
"""miniBRUTE Stage 4 -- simultaneous Brute waves, Ultrasaw, PWM, Metalizer and Sub.

Run from C:\\FM1\\miniBRUTE AFTER dev4/Steiner tests. Does not flash hardware.
"""
import argparse
from pathlib import Path
import shutil
import sys


def once(s, a, b, file, label):
    n = s.count(a)
    if n != 1:
        raise RuntimeError(f'{file}: {label}: expected one anchor, found {n}; nothing modified')
    return s.replace(a, b, 1)


TEST = r'''/* miniBRUTE Stage 4: simultaneous Brute oscillator and legacy DSP/preset regression.
 * Emulated hardware and emulated NOR only, NEVER real flash. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "../web/emu/felucca_web.c"

static unsigned tested, ok;
static void ck(int yes, const char *name)
{
    tested++;
    if (yes) ok++;
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
/* Reset every voice/oscillator/filter state so output comparisons are deterministic. */
static void tone(int brute, int saw, int pulse, int tri, int sub, int ultra,
                 int pwm, int metal, int oct, int shape, int32_t *out, unsigned n)
{
    track_t *t = gu_t();
    voice_t *v = &t->v[0];
    glp_t *g = gl_part(t);
    vmod_t m;
    memset(&g->v[0], 0, sizeof g->v[0]);
    memset(v, 0, sizeof *v);
    memset(&m, 0, sizeof m);
    memset(out, 0, n * sizeof *out);
    g->seed = 0x5EED1234;
    g->inc1 = g->inc2 = 0;
    v->note = 60;
    v->vel = 100;
    v->active = v->gate = 1;
    v->ph[1] = 0x40000000u;
    m.pitch16 = 60 * 16;
    m.inc = pitch_inc(m.pitch16);
    m.amp0 = m.amp1 = m.envq15 = 32767;
    t->p[P_GLSYNTH] = (int16_t)brute;
    t->p[P_GLBULTRA] = (int16_t)ultra;
    t->p[P_GLBPWM] = (int16_t)pwm;
    t->p[P_GLBMETAL] = (int16_t)metal;
    t->p[P_GLB_SUB] = (int16_t)sub;
    t->p[P_GLSPMODE] = 0;
    t->p[P_GLBRUTE] = 0;
    t->p[P_VOICE] = V_MONO;
    t->p[GL_O1WAVE] = 0;
    t->p[GL_O1C1] = t->p[GL_O1C2] = 0;
    t->p[GL_O2WAVE] = (int16_t)shape;
    t->p[GL_O2MOD] = (int16_t)oct;
    t->p[GL_O2SEMI] = t->p[GL_O2TUNE] = 0;
    t->p[GL_MIX1] = (int16_t)saw;
    t->p[GL_MIX2] = (int16_t)pulse;
    t->p[GL_NOISE] = (int16_t)tri;
    t->p[GL_FOLD] = t->p[GL_DIST] = 0;
    t->p[GL_CUTOFF] = 90;
    t->p[GL_RESO] = 20;
    t->p[GL_FEGINT] = t->p[GL_KTRK] = 0;
    for (unsigned k = 0; k < 4u; k++)
        t->p[GL_P1INT + 3u * k] = 0;
    gol_render(t, v, out, n, &m);
}
static int changed(const int32_t *a, const int32_t *b, unsigned n)
{
    return memcmp(a, b, n * sizeof *a) != 0;
}
static int audible(const int32_t *a, unsigned n)
{
    for (unsigned i = 0; i < n; i++) if (a[i]) return 1;
    return 0;
}
static int bounded(const int32_t *a, unsigned n)
{
    for (unsigned i = 0; i < n; i++)
        if (a[i] < -100000 || a[i] > 100000) return 0;
    return 1;
}
int main(void)
{
    int32_t base[512], alt[512], saw[512], pulse[512], tri[512], sub[512], mixed[512];
    int16_t def[P_COUNT], old[P_COUNT - 4], mapped[P_COUNT];
    int page_mod = 0, page_sub = 0, page_mix = 0, safe = 1;
    static const gu_par_t u = {P_GLBULTRA, "ULTRASAW", 0};
    static const gu_par_t pw = {P_GLBPWM, "PWM", 0};
    static const gu_par_t me = {P_GLBMETAL, "METALIZER", 0};
    static const gu_par_t su = {P_GLB_SUB, "SUB", 0};
    web_nor_erase(); web_boot(); advance(1600);
    web_keys(1u << 12); advance(100); web_keys(0); advance(300);
    ck(gu.ready, "Stage 4 host emulator boots");
    ck(P_COUNT < 128 && sizeof(up_rec_t) == 192u && GL_NPROG == 32u,
       "Original 32 presets / 192-byte record / 7-bit parameter count");
    ck(gu_t()->p[P_GLBULTRA] == 0 && gu_t()->p[P_GLBPWM] == 0 &&
       gu_t()->p[P_GLBMETAL] == 0 && gu_t()->p[P_GLB_SUB] == 0,
       "Legacy instruments default all four BRUTE oscillator parameters to zero");

    for (unsigned i = 0; i < GU_NPAGE; i++) {
        const gu_page_t *p = &GU_PAGES[i];
        if (p->k[0].id == GL_O1WAVE) {
            gu_t()->p[P_GLSYNTH] = 1;
            if (gu_pk(p, 0)->id == P_GLBULTRA && gu_pk(p, 1)->id == P_GLBPWM &&
                gu_pk(p, 2)->id == P_GLBMETAL) page_mod = 1;
            gu_t()->p[P_GLSYNTH] = 0;
            if (gu_pk(p, 0)->id != GL_O1WAVE) page_mod = 0;
        }
        if (p->k[0].id == GL_O2WAVE) {
            gu_t()->p[P_GLSYNTH] = 1;
            if (gu_pk(p, 0)->id == GL_O2WAVE && gu_pk(p, 3)->id == P_GLB_SUB)
                page_sub = 1;
        }
        if (p->k[0].id == GL_MIX1) {
            gu_t()->p[P_GLSYNTH] = 1;
            if (gu_pk(p, 0)->id == GL_MIX1 && gu_pk(p, 1)->id == GL_MIX2 &&
                gu_pk(p, 2)->id == GL_NOISE && gu_pk(p, 3)->id == P_GLB_SUB)
                page_mix = 1;
            gu_t()->p[P_GLSYNTH] = 0;
            if (gu_pk(p, 3)->id != GL_FOLD) page_mix = 0;
        }
    }
    ck(page_mod && page_sub && page_mix, "BRUTE OSC1/OSC2/MIXER controls coexist with original MIKROKORG UI");

    /* Every independent wave must contribute sound. */
    tone(1,127,0,0,0,0,0,0,0,0,saw,512);
    tone(1,0,127,0,0,0,0,0,0,0,pulse,512);
    tone(1,0,0,127,0,0,0,0,0,0,tri,512);
    tone(1,0,0,0,127,0,0,0,0,0,sub,512);
    ck(audible(saw,512) && audible(pulse,512) && audible(tri,512) && audible(sub,512),
       "SAW, PULSE, TRIANGLE and SUB each generate audible samples");
    ck(changed(saw,pulse,512) && changed(saw,tri,512) && changed(tri,sub,512),
       "All four oscillators have distinct waveforms");
    tone(1,127,127,127,127,0,0,0,0,0,mixed,512);
    ck(changed(mixed,saw,512) && changed(mixed,tri,512),
       "BRUTE mixes all four waveforms simultaneously");

    tone(1,127,0,0,0,0,0,0,0,0,base,512);
    tone(1,127,0,0,0,127,0,0,0,0,alt,512);
    ck(changed(base,alt,512), "ULTRASAW creates detuned saw layers");
    tone(1,0,127,0,0,0,0,0,0,0,base,512);
    tone(1,0,127,0,0,0,127,0,0,0,alt,512);
    ck(changed(base,alt,512), "PWM adjusts the pulse width");
    tone(1,0,0,127,0,0,0,0,0,0,base,512);
    tone(1,0,0,127,0,0,0,127,0,0,alt,512);
    ck(changed(base,alt,512), "METALIZER folds the triangle waveform");
    tone(1,0,0,0,127,0,0,0,0,0,base,512);
    tone(1,0,0,0,127,0,0,0,1,1,alt,512);
    ck(changed(base,alt,512), "SUB offers alternative octave and waveform");
    tone(1,0,0,0,127,0,0,0,2,0,alt,512);
    ck(changed(base,alt,512), "SUB also generates overtone (+1 octave)");

    /* Stage 4 parameters and wave modifiers MUST be entirely ignored by MIKROKORG. */
    tone(0,127,0,0,0,0,0,0,0,0,base,512);
    tone(0,127,0,0,127,127,127,127,0,0,alt,512);
    ck(!changed(base,alt,512), "MIKROKORG unchanged by BRUTE-only oscillator parameters");

    /* Max waveform levels + all modifiers + feedback and resonance, all Steiner types. */
    for (int mode=0;mode<3;mode++) {
        tone(1,127,127,127,127,127,127,127,2,1,alt,512);
        if (!bounded(alt,512) || !audible(alt,512)) safe=0;
        gu_t()->p[P_GLSPMODE]=(int16_t)mode;
        gu_t()->p[P_GLBRUTE]=127;
        /* A second full render with active feedback/filter state. */
        {
            track_t *t=gu_t(); voice_t *v=&t->v[0]; glp_t *g=gl_part(t); vmod_t m;
            memset(&m,0,sizeof m); m.pitch16=960; m.inc=pitch_inc(960);
            m.amp0=m.amp1=m.envq15=32767;
            t->p[P_GLBRUTE]=127; t->p[P_GLSPMODE]=(int16_t)mode;
            memset(mixed,0,sizeof mixed);
            gol_render(t,v,mixed,512,&m);
            if (!bounded(mixed,512)) safe=0;
            (void)g;
        }
    }
    ck(safe,"All four waves at maximum: output stays bounded including feedback");

    gu_t()->p[P_GLSYNTH]=1;
    gu_set(&u,91); gu_set(&pw,43); gu_set(&me,113); gu_set(&su,88);
    ck(gu_get(&u)==91 && gu_get(&pw)==43 && gu_get(&me)==113 && gu_get(&su)==88,
       "All four BRUTE parameters editable over 0..127");
    ck(gu_store(31u,"BRUTE WAVES")==0, "Save BRUTE oscillator patch to simulated P32");
    gu_load(0u);
    ck(gu_t()->p[P_GLBULTRA]==0 && gu_t()->p[P_GLBPWM]==0 && gu_t()->p[P_GLB_SUB]==0,
       "Factory instrument still defaults to BRUTE wave modifiers off");
    gu_load(31u);
    ck(gu_t()->p[P_GLSYNTH]==1 && gu_t()->p[P_GLBULTRA]==91 &&
       gu_t()->p[P_GLBPWM]==43 && gu_t()->p[P_GLBMETAL]==113 && gu_t()->p[P_GLB_SUB]==88,
       "Four oscillator parameters and SYNTH MODE restored from simulated NOR");

    gl_defaults(def);
    for (unsigned i=0;i<P_COUNT-4u-8u;i++) old[i]=def[i];
    old[P_GLSYNTH]=1;
    old[P_GLBRUTE]=66;
    old[P_GLSPMODE]=2;
    for (unsigned i=0;i<8;i++) old[P_COUNT-4u-8u+i]=def[P_E0+i];
    params_by_count(mapped,old,P_COUNT-4u,def);
    ck(mapped[P_GLSYNTH]==1 && mapped[P_GLBRUTE]==66 && mapped[P_GLSPMODE]==2 &&
       mapped[P_GLBULTRA]==0 && mapped[P_GLBPWM]==0 && mapped[P_GLBMETAL]==0 &&
       mapped[P_GLB_SUB]==0 && mapped[P_E0]==def[P_E0] && mapped[P_E7]==def[P_E7],
       "All Stage 3 presets map correctly, preserving prior parameters and E0..E7");
    printf("\nminiBRUTE OSC Stage 4: %u/%u passed; %u failures\n",ok,tested,tested-ok);
    return ok!=tested;
}
'''


def main():
    parser = argparse.ArgumentParser()
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--check', action='store_true')
    mode.add_argument('--apply', action='store_true')
    args = parser.parse_args()
    if Path.cwd().name.lower() != 'minibrute' or not Path('firmware/src/golana_gfx.c').is_file():
        raise RuntimeError('Safety stop: run only from C:\\FM1\\miniBRUTE')
    paths = [
        'firmware/src/core.h', 'firmware/src/params.c', 'firmware/src/golana_prog.c',
        'firmware/src/golana_ui.c', 'firmware/src/golana_gfx.c',
        'firmware/src/eng_golana.c', 'tests/run_tests.sh',
    ]
    original = {p: Path(p).read_text(encoding='utf-8') for p in paths}
    out = dict(original)
    if 'P_GLSYNTH' not in original['firmware/src/core.h'] or 'P_GLBRUTE' not in original['firmware/src/core.h'] or 'P_GLSPMODE' not in original['firmware/src/core.h']:
        raise RuntimeError('Stage 3 is missing; install and test dev4 first')
    if 'P_GLBULTRA' in original['firmware/src/core.h']:
        raise RuntimeError('Stage 4 already installed; no double patch')
    if not Path('tests/minibrute_steiner.c').is_file():
        raise RuntimeError('Stage 3 native host tests missing; aborting')
    testfile = Path('tests/minibrute_osc.c')
    if testfile.exists():
        raise RuntimeError('tests/minibrute_osc.c already exists')

    f = 'firmware/src/core.h'
    out[f] = once(out[f],
        '    P_GLSPMODE, /* BRUTE Steiner LP/BP/HP; old sounds default LP */',
        '    P_GLSPMODE, /* BRUTE Steiner LP/BP/HP; old sounds default LP */\n'
        '    P_GLBULTRA, /* BRUTE: Ultrasaw amount, 0..127 */\n'
        '    P_GLBPWM,   /* BRUTE: PWM width, 0..127 */\n'
        '    P_GLBMETAL, /* BRUTE: triangle Metalizer, 0..127 */\n'
        '    P_GLB_SUB,  /* BRUTE: sub/overtone level, 0..127 */',
        f, 'four BRUTE-only preset parameters before P_E0')
    out[f] = once(out[f], 'uint8_t rsv_sz[10];', 'uint8_t rsv_sz[2];',
                  f, 'preserve sizeof(track_t) multiple-of-16 after four new int16 params')

    f = 'firmware/src/params.c'
    out[f] = once(out[f],
        '    [P_GLSPMODE] = PE("SPMODE", N_GLSPMODE, 0),',
        '    [P_GLSPMODE] = PE("SPMODE", N_GLSPMODE, 0),\n'
        '    [P_GLBULTRA] = PD("ULTRA", F_PCT, 0, 127, 0),\n'
        '    [P_GLBPWM] = PD("PWM", F_PCT, 0, 127, 0),\n'
        '    [P_GLBMETAL] = PD("METAL", F_PCT, 0, 127, 0),\n'
        '    [P_GLB_SUB] = PD("SUB", F_PCT, 0, 127, 0),',
        f, 'Golana defaults and enum descriptions')

    f = 'firmware/src/golana_prog.c'
    out[f] = once(out[f],
        '    p[P_GLSPMODE] = 0;                   /* new/old BRUTE sounds default to LP */',
        '    p[P_GLSPMODE] = 0;                   /* new/old BRUTE sounds default to LP */\n'
        '    p[P_GLBULTRA] = p[P_GLBPWM] = p[P_GLBMETAL] = p[P_GLB_SUB] = 0; /* off on legacy sounds */',
        f, 'zero modulation on old presets')

    f = 'firmware/src/golana_ui.c'
    out[f] = once(out[f],
        'static const char *const N_GVOICE[] = {"POLY", "MONO", "LEGATO", "UNISON"};',
        'static const char *const N_GVOICE[] = {"POLY", "MONO", "LEGATO", "UNISON"};\n'
        'static const char *const N_BSOCT[] = {"-1 OCT", "-2 OCT", "+1 OCT", "+2 OCT"};\n'
        'static const char *const N_BSWAVE[] = {"SQUARE", "SAW", "TRI"};',
        f, 'sub oscillator choices')
    out[f] = once(out[f],
        '    static const gu_par_t STEINER_TYPE = {P_GLSPMODE, "TYPE", 0};',
        '    static const gu_par_t STEINER_TYPE = {P_GLSPMODE, "TYPE", 0};\n'
        '    static const gu_par_t BR_OSC1[4] = {\n'
        '        {P_GLBULTRA, "ULTRASAW", 0}, {P_GLBPWM, "PWM", 0},\n'
        '        {P_GLBMETAL, "METALIZER", 0}, {GL_O2MOD, "SUB OCT", GN(N_BSOCT)}\n'
        '    };\n'
        '    static const gu_par_t BR_OSC2[4] = {\n'
        '        {GL_O2WAVE, "SUB WAVE", GN(N_BSWAVE)}, {GL_O2SEMI, "SUB SEMI", 0},\n'
        '        {GL_O2TUNE, "SUB TUNE", 0}, {P_GLB_SUB, "SUB LVL", 0}\n'
        '    };\n'
        '    static const gu_par_t BR_MIX[4] = {\n'
        '        {GL_MIX1, "SAW", 0}, {GL_MIX2, "PULSE", 0},\n'
        '        {GL_NOISE, "TRI", 0}, {P_GLB_SUB, "SUB", 0}\n'
        '    };\n'
        '    if (gu_t()->p[P_GLSYNTH] == 1) {\n'
        '        if (pg->k[0].id == GL_O1WAVE) return &BR_OSC1[k];\n'
        '        if (pg->k[0].id == GL_O2WAVE) return &BR_OSC2[k];\n'
        '        if (pg->k[0].id == GL_MIX1) return &BR_MIX[k];\n'
        '    }',
        f, 'conditional BRUTE pages without modifying original MIKRO controls')

    f = 'firmware/src/golana_gfx.c'
    out[f] = once(out[f],
        '    switch (pg->k[0].id) {',
        '    if (gu_t()->p[P_GLSYNTH] == 1 &&\n'
        '        (pg->k[0].id == GL_O1WAVE || pg->k[0].id == GL_O2WAVE))\n'
        '        return GG_NONE; /* do not draw the old OSC waveform for the new 4-wave mix */\n'
        '    switch (pg->k[0].id) {',
        f, 'disable misleading MIKRO waveform graphs in BRUTE')

    f = 'firmware/src/eng_golana.c'
    out[f] = once(out[f],
        '    int32_t brute_fb;            /* one-sample VCA -> filter input feedback, per voice */',
        '    int32_t brute_fb;            /* one-sample VCA -> filter input feedback, per voice */\n'
        '    uint32_t ua, ub;             /* independent detuned ULTRASAW phases */',
        f, '2 new oscillator phases in shared per-engine pool')
    out[f] = once(out[f],
        '        s->brute_fb = 0;\n        s->jl = 0;',
        '        s->brute_fb = 0;\n        s->ua = s->ub = 0;\n        s->jl = 0;',
        f, 'reset detuned phases on new note')
    out[f] = once(out[f],
        '    int32_t pitch1, pitch2, fold;\n    uint32_t inc1, inc2;',
        '    int32_t pitch1, pitch2, fold;\n    uint32_t inc1, inc2, uinc_a, uinc_b;',
        f, 'phase steps for both additional saws')
    out[f] = once(out[f],
        '    int32_t sp_mode = p[P_GLSYNTH] == 1 ? gp(t, P_GLSPMODE, 0, 2) : -1;',
        '    int32_t sp_mode = p[P_GLSYNTH] == 1 ? gp(t, P_GLSPMODE, 0, 2) : -1;\n'
        '    int32_t is_brute = p[P_GLSYNTH] == 1;\n'
        '    int32_t ultra = is_brute ? gp(t, P_GLBULTRA, 0, 127) : 0;\n'
        '    int32_t pwm = is_brute ? gp(t, P_GLBPWM, 0, 127) : 0;\n'
        '    int32_t metal = is_brute ? gp(t, P_GLBMETAL, 0, 127) : 0;\n'
        '    int32_t submix = is_brute ? gp(t, P_GLB_SUB, 0, 127) * 258 : 0;',
        f, 'read BRUTE-only parameters at control-rate')
    out[f] = once(out[f],
        '    inc2 = cents_inc(clamp(pitch2, 0, 2047), gp(t, GL_O2TUNE, -63, 63),\n'
        '                     t->p[P_VOICE] == V_UNISON ? m->fine - 2 * v->fine : m->fine);',
        '    inc2 = cents_inc(clamp(pitch2, 0, 2047), gp(t, GL_O2TUNE, -63, 63),\n'
        '                     t->p[P_VOICE] == V_UNISON ? m->fine - 2 * v->fine : m->fine);\n'
        '    if (is_brute) {\n'
        '        /* Existing OSC2 phase doubles as BRUTE sub (-2/-1 oct) or overtone (+1/+2 oct). */\n'
        '        int32_t oct = gp(t, GL_O2MOD, 0, 3);\n'
        '        int32_t offset = oct == 0 ? -12 : oct == 1 ? -24 : oct == 2 ? 12 : 24;\n'
        '        int32_t ps = pitch1 + (offset + gp(t, GL_O2SEMI, -24, 24)) * 16;\n'
        '        inc2 = cents_inc(clamp(ps, 0, 2047), gp(t, GL_O2TUNE, -63, 63), m->fine);\n'
        '    }',
        f, 'sub note ratio and octave selector')
    out[f] = once(out[f],
        '    gain = clamp(32767 + (d[GD_AMP] >> 6), 0, 49151);  /* AMP: the patch\'s gain, up to 1.5 */',
        '    gain = clamp(32767 + (d[GD_AMP] >> 6), 0, 49151);  /* AMP: the patch\'s gain, up to 1.5 */\n'
        '    uinc_a = uinc_b = inc1;\n'
        '    if (ultra && x1) {\n'
        '        int32_t cents = 2 + (ultra * 28) / 127;  /* up to ~30 cents spread */\n'
        '        uinc_a = cents_inc(clamp(pitch1, 0, 2047), -cents, m->fine);\n'
        '        uinc_b = cents_inc(clamp(pitch1, 0, 2047), cents, m->fine);\n'
        '    }',
        f, 'detuned phase increments once per audio block')
    out[f] = once(out[f],
        '        uint32_t ph0 = v->ph[0], ph1 = v->ph[1], pw;',
        '        uint32_t ph0 = v->ph[0], ph1 = v->ph[1], pw, ua = s->ua, ub = s->ub;',
        f, 'local oscillator phase accumulators')
    out[f] = once(out[f],
        '        pw = 0x80000000u + (uint32_t)ctrl1 * 0xF00000u; /* PULSE: 50 % .. ~6 % */',
        '        pw = 0x80000000u + (uint32_t)ctrl1 * 0xF00000u; /* PULSE: 50 % .. ~6 % */\n'
        '        if (is_brute) pw = 0x80000000u + (uint32_t)pwm * 0xC00000u; /* 50 % .. ~87 % */',
        f, 'BRUTE PWM pulse width')
    synth = '''            if (is_brute) {
                int32_t saw = x1 ? osc_saw(ph0, inc1) : 0;
                int32_t pulse = x2 ? osc_pulse(ph0, inc1, pw) : 0;
                int32_t tri = xn ? osc_tri(ph0) : 0;
                if (x1 && ultra) { /* two independent, slightly detuned band-limited saws */
                    int32_t layered = (2 * saw + osc_saw(ua, uinc_a) + osc_saw(ub, uinc_b)) >> 2;
                    saw += mulq15(layered - saw, ultra * 258);
                }
                if (xn && metal) { /* folding triangle, original Golana fixed-point algorithm */
                    int32_t drive = 32768 + metal * 768;
                    int32_t u = (tri * (drive >> 4)) >> 11;
                    u = (int32_t)((uint32_t)(u + 32768) & 0x1FFFFu);
                    tri = u < 65536 ? u - 32768 : 98303 - u;
                }
                sx = mulq15(saw, x1) + mulq15(pulse, x2) + mulq15(tri, xn);
                if (submix) {
                    uint32_t shape = (uint32_t)gp(t, GL_O2WAVE, 0, 2);
                    int32_t sub = shape == 0u ? osc_pulse(ph1, inc2, 0x80000000u) :
                                  shape == 1u ? osc_saw(ph1, inc2) : osc_tri(ph1);
                    sx += mulq15(sub, submix);
                }
                ph0 += inc1;
                ph1 += inc2;
                ua += uinc_a;
                ub += uinc_b;
            } else {
'''
    out[f] = once(out[f],
        '            /* OSC 2 first (SINE\'s cross modulation reads it); its sync restart rounded as in ANALOG\'s */',
        synth + '            /* OSC 2 first (SINE\'s cross modulation reads it); its sync restart rounded as in ANALOG\'s */',
        f, 'new BRUTE oscillator branch before legacy MIKROKORG code')
    out[f] = once(out[f],
        '            if (xn)\n                sx += mulq15((int32_t)(noise32(&nst) >> 17) - 16384, xn);\n'
        '            sx = (sx * 3) >> 3;',
        '            if (xn)\n                sx += mulq15((int32_t)(noise32(&nst) >> 17) - 16384, xn);\n'
        '            } /* END untouched MIKROKORG oscillator */\n'
        '            sx = (sx * 3) >> 3;',
        f, 'close untouched MIKROKORG branch before common mixer scaling')
    out[f] = once(out[f],
        '        s->jl = jl;\n        s->brute_fb = bf ? fb : 0;',
        '        s->jl = jl;\n        s->ua = ua;\n        s->ub = ub;\n        s->brute_fb = bf ? fb : 0;',
        f, 'keep saw phases across render blocks')

    f = 'tests/run_tests.sh'
    out[f] = once(out[f],
        '    run "miniBRUTE Steiner LP/BP/HP, DSP isolation and legacy presets" "$OUT/minibrute_steiner"',
        '    run "miniBRUTE Steiner LP/BP/HP, DSP isolation and legacy presets" "$OUT/minibrute_steiner"\n'
        '    $CC $G1 -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/minibrute_osc" tests/minibrute_osc.c -lm\n'
        '    run "miniBRUTE Brute oscillator Ultrasaw PWM Metalizer Sub" "$OUT/minibrute_osc"',
        f, 'register Stage 4 regression')

    for p in paths:
        if original[p] == out[p]:
            raise RuntimeError(f'Internal failure: unchanged {p}')
        print('READY ', p)
    print('READY ', testfile, '(new)')
    if args.check:
        print('CHECK ONLY: all seven source files have unique matching anchors; nothing written')
        return
    backups = [Path(p + '.before-brute-osc-stage4') for p in paths]
    if any(p.exists() for p in backups):
        raise RuntimeError('Stage 4 backup already exists; refusing to overwrite')
    for p in paths:
        shutil.copy2(p, p + '.before-brute-osc-stage4')
        Path(p).write_text(out[p], encoding='utf-8')
        print('PATCHED', p)
    testfile.write_text(TEST, encoding='utf-8')
    print('CREATED', testfile)
    print('DONE: Stage 4 installed. Build and run tests BEFORE flashing.')
    print('Digital MicroBrute-inspired oscillator, not component-level analog circuit emulation.')


if __name__ == '__main__':
    try:
        main()
    except (OSError, RuntimeError) as ex:
        sys.exit('ERROR: ' + str(ex))
