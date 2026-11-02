#!/usr/bin/env python3
"""miniBRUTE Stage 2: per-preset Brute Factor feedback, only in BRUTE mode.

Run in C:\\FM1\\miniBRUTE after Stage 1 has passed. This script never flashes hardware.
"""
import argparse
from pathlib import Path
import shutil
import sys


def sub(s, old, new, path, desc):
    n = s.count(old)
    if n != 1:
        raise RuntimeError(f'{path}: {desc}: expected exactly one anchor, found {n}; no files changed')
    return s.replace(old, new, 1)


TEST = r'''/* miniBRUTE Stage 2: Brute Factor, DSP bypass, stability and NOR preset round trip.
 * Native host simulation only; does not use any physical FM-1. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "../web/emu/felucca_web.c"

static int passed, total;
static void check(int cond, const char *name)
{
    total++;
    if (cond) passed++;
    printf("%s %s\n", cond ? "PASS" : "FAIL", name);
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

/* Reproduce the same OSC, envelope and filter state before each render.
 * No external MIDI clock, playback or effects are involved in this DSP check. */
static void render_case(int mode, int factor, int32_t *audio, unsigned n)
{
    track_t *t = gu_t();
    voice_t *v = &t->v[0];
    glp_t *part = gl_part(t);
    vmod_t m;
    memset(&part->v[0], 0, sizeof part->v[0]);
    memset(v, 0, sizeof *v);
    memset(&m, 0, sizeof m);
    memset(audio, 0, n * sizeof *audio);
    part->seed = 0x5EED1234;
    part->inc1 = part->inc2 = 0;
    v->note = 60;
    v->vel = 100;
    v->gate = v->active = 1;
    v->ph[1] = 0x40000000u;
    m.pitch16 = 60 * 16;
    m.inc = pitch_inc(m.pitch16);
    m.amp0 = m.amp1 = m.envq15 = 32767;
    t->p[P_GLSYNTH] = (int16_t)mode;
    t->p[P_GLBRUTE] = (int16_t)factor;
    t->p[P_VOICE] = V_MONO;
    t->p[GL_O1WAVE] = 0;
    t->p[GL_MIX1] = 127;
    t->p[GL_MIX2] = 0;
    t->p[GL_NOISE] = 0;
    t->p[GL_FOLD] = 0;
    t->p[GL_DIST] = 0;
    t->p[GL_FTYPE] = 0;
    t->p[GL_CUTOFF] = 80;
    t->p[GL_RESO] = 22;
    t->p[GL_FEGINT] = 0;
    t->p[GL_KTRK] = 0;
    for (unsigned k = 0; k < 4u; k++)
        t->p[GL_P1INT + 3u * k] = 0;
    gol_render(t, v, audio, n, &m);
}

int main(void)
{
    int32_t korg0[256], korg127[256], brute0[256], brute127[256];
    int different = 0, nonzero = 0, bounded = 1, page = 0;
    int16_t defaults[P_COUNT], before[P_COUNT - 1], mapped[P_COUNT];
    static const gu_par_t bf = {P_GLBRUTE, "BRUTE FACTOR", 0};
    web_nor_erase();
    web_boot();
    advance(1600);
    web_keys(1u << 12); advance(100); web_keys(0); advance(300);
    for (unsigned j = 0; j < GU_NPAGE; j++)
        if (GU_PAGES[j].btn == B_SCL && GU_PAGES[j].k[0].id == P_GLBRUTE)
            page = 1;
    check(gu.ready && page, "FILTER / BRUTE FACTOR page is available");
    check(gu_get(&bf) == 0, "Old instrument starts with BRUTE FACTOR 0");
    render_case(0, 0, korg0, 256);
    render_case(0, 127, korg127, 256);
    render_case(1, 0, brute0, 256);
    render_case(1, 127, brute127, 256);
    check(!memcmp(korg0, korg127, sizeof korg0), "MIKROKORG bypass is sample-identical at factor 0/127");
    check(!memcmp(korg0, brute0, sizeof korg0), "BRUTE factor 0 is sample-identical to legacy DSP");
    for (unsigned i = 0; i < 256; i++) {
        if (brute127[i] != brute0[i]) different = 1;
        if (brute127[i] != 0) nonzero = 1;
        if (brute127[i] > 100000 || brute127[i] < -100000) bounded = 0;
    }
    check(different && nonzero, "BRUTE FACTOR 127 changes the audio waveform");
    check(bounded, "Maximum feedback remains bounded");
    gu_t()->p[P_GLSYNTH] = 1;
    gu_set(&bf, 85);
    check(gu_get(&bf) == 85, "BRUTE FACTOR knob accepts 0..127");
    check(gu_store(31u, "BRUTE 85") == 0, "Simulated NOR writes BRUTE sound to P32");
    gu_load(0u);
    check(gu_get(&bf) == 0, "Old factory preset defaults to factor 0");
    gu_load(31u);
    check(gu_get(&bf) == 85 && gu_t()->p[P_GLSYNTH] == 1,
          "Factor and BRUTE mode reload together from P32");
    gl_defaults(defaults);
    for (unsigned i = 0; i < P_COUNT - 1u - 8u; i++) before[i] = defaults[i];
    before[P_GLSYNTH] = 1;  /* a Stage 1 BRUTE preset without a factor field */
    for (unsigned i = 0; i < 8; i++) before[P_COUNT - 1u - 8u + i] = defaults[P_E0 + i];
    params_by_count(mapped, before, P_COUNT - 1u, defaults);
    check(mapped[P_GLSYNTH] == 1 && mapped[P_GLBRUTE] == 0 &&
          mapped[P_E0] == defaults[P_E0] && mapped[P_E7] == defaults[P_E7],
          "Stage 1 BRUTE presets remain compatible (factor 0)");
    check(GL_NPROG == 32u && sizeof(up_rec_t) == 192u,
          "32 sound slots and 192-byte NOR record remain unchanged");
    printf("\nminiBRUTE BRUTE FACTOR: %d/%d passed; %d failures\n",
           passed, total, total - passed);
    return passed != total;
}
'''


def main():
    parser = argparse.ArgumentParser()
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--check', action='store_true')
    mode.add_argument('--apply', action='store_true')
    args = parser.parse_args()

    if Path.cwd().name.lower() != 'minibrute' or not Path('firmware/src/golana_gfx.c').is_file():
        raise RuntimeError('Safety guard: run only in C:\\FM1\\miniBRUTE (not Golana-DZ)')
    files = ['firmware/src/core.h', 'firmware/src/params.c', 'firmware/src/golana_prog.c',
             'firmware/src/golana_ui.c', 'firmware/src/eng_golana.c', 'tests/run_tests.sh']
    orig = {p: Path(p).read_text(encoding='utf-8') for p in files}
    new = dict(orig)
    if 'P_GLSYNTH' not in orig['firmware/src/core.h']:
        raise RuntimeError('Stage 1 is not installed; BRUTE FACTOR requires SYNTH MODE first')
    if 'P_GLBRUTE' in orig['firmware/src/core.h']:
        raise RuntimeError('BRUTE FACTOR appears installed already; stopping')
    tfile = Path('tests/minibrute_brute_factor.c')
    if tfile.exists():
        raise RuntimeError('New Brute Factor test exists; stopping to avoid overwriting')

    p = 'firmware/src/core.h'
    new[p] = sub(new[p],
       '    P_GLSYNTH, /* miniBRUTE: 0 MIKROKORG, 1 BRUTE; placed before E0 for legacy mapping */',
       '    P_GLSYNTH, /* miniBRUTE: 0 MIKROKORG, 1 BRUTE; placed before E0 for legacy mapping */\n'
       '    P_GLBRUTE, /* miniBRUTE: per-sound BRUTE FACTOR, 0..127; old sounds default 0 */',p,'append before E0')

    p = 'firmware/src/params.c'
    new[p] = sub(new[p],
       '    [P_GLSYNTH] = PE("SYNTH", N_GLSYNTH, 0),   /* old presets default MIKROKORG */',
       '    [P_GLSYNTH] = PE("SYNTH", N_GLSYNTH, 0),   /* old presets default MIKROKORG */\n'
       '    [P_GLBRUTE] = PD("BFACT", F_PCT, 0, 127, 0),',p,'factor param descriptor')

    p = 'firmware/src/golana_prog.c'
    new[p] = sub(new[p],
       '    p[P_GLSYNTH] = 0;                       /* factory sounds use MIKROKORG */',
       '    p[P_GLSYNTH] = 0;                       /* factory sounds use MIKROKORG */\n'
       '    p[P_GLBRUTE] = 0;                     /* no feedback on all factory sounds */',p,'safe default')

    p = 'firmware/src/golana_ui.c'
    anchor = ('    {B_SCL, "KEY TRACK", "KEY TRK", {{GL_KTRK, "KEY TRACK", 0}, '
              '{GV_NONE, 0, 0}, {GV_NONE, 0, 0}, {GV_NONE, 0, 0}}},')
    new[p] = sub(new[p], anchor, anchor + '\n'
       '    {B_SCL, "BRUTE FACTOR", "BRUTE",\n'
       '     {{P_GLBRUTE, "FACTOR", 0}, {GL_CUTOFF, "CUTOFF", 0}, '
       '{GL_RESO, "RESO", 0}, {P_GLSYNTH, "SYNTH MODE", 0}}},',
       p,'add FILTER/BRUTE page')

    p = 'firmware/src/eng_golana.c'
    new[p] = sub(new[p],
       '    int32_t feg;                 /* filter EG, Q24 */',
       '    int32_t feg;                 /* filter EG, Q24 */\n'
       '    int32_t brute_fb;            /* one-sample VCA -> filter input feedback, per voice */',
       p,'per-voice state')
    new[p] = sub(new[p],
       '        s->feg = 0;\n        s->jl = 0;',
       '        s->feg = 0;\n        s->brute_fb = 0;\n        s->jl = 0;',
       p,'zero feedback on fresh note')
    new[p] = sub(new[p],
       '    int32_t dist = p[GL_DIST] != 0;',
       '    int32_t dist = p[GL_DIST] != 0;\n'
       '    /* BRUTE FACTOR does not touch MIKROKORG; zero is a strict audio bypass. */\n'
       '    int32_t bf = p[P_GLSYNTH] == 1 ? gp(t, P_GLBRUTE, 0, 127) : 0;',
       p,'mode-gated feedback coefficient')
    new[p] = sub(new[p],
       '        int32_t a1 = s->f1, a2 = s->f2, b1 = s->f3, b2 = s->f4, nst = s->nst, jl = s->jl;',
       '        int32_t a1 = s->f1, a2 = s->f2, b1 = s->f3, b2 = s->f4, nst = s->nst, jl = s->jl;\n'
       '        int32_t fb = bf ? s->brute_fb : 0;',
       p,'block local feedback state')
    new[p] = sub(new[p],
       '            switch (ft) {',
       '            /* One-sample internal VCA feedback, with bounded input saturation.\n'
       '             * Amount 0 = bit-identical Golana processing; maximum about 3x feedback.\n'
       '             * The limiter prevents runaway and softclips the reinjection, not\n'
       '             * the final speaker signal. Effects do not enter this loop. */\n'
       '            if (bf) {\n'
       '                int32_t injection = (fb * (bf * 3)) >> 7;\n'
       '                sx = softclip(clamp(sx + injection, -131070, 131070));\n'
       '            }\n'
       '            switch (ft) {',
       p,'feed after VCA back into filter input')
    new[p] = sub(new[p],
       '            out[i] += voice_amp(clamp(y, -65535, 65535), m, i) << 1;',
       '            if (bf) {\n'
       '                int32_t amp = voice_amp(clamp(y, -65535, 65535), m, i);\n'
       '                fb = clamp(amp * 2, -32767, 32767); /* VCA output, prior to FX */\n'
       '                out[i] += amp << 1;\n'
       '            } else {\n'
       '                out[i] += voice_amp(clamp(y, -65535, 65535), m, i) << 1;\n'
       '            }',
       p,'capture VCA sample without changing bypass')
    new[p] = sub(new[p],
       '        s->jl = jl;\n    }',
       '        s->jl = jl;\n        s->brute_fb = bf ? fb : 0;\n    }',
       p,'persist feedback to next DSP block')

    p = 'tests/run_tests.sh'
    new[p] = sub(new[p],
       '    run "miniBRUTE per-preset SYNTH MODE, mono and legacy records" "$OUT/minibrute_synth_mode"',
       '    run "miniBRUTE per-preset SYNTH MODE, mono and legacy records" "$OUT/minibrute_synth_mode"\n'
       '    $CC $G1 -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/minibrute_brute_factor" tests/minibrute_brute_factor.c -lm\n'
       '    run "miniBRUTE BRUTE FACTOR feedback, DSP bypass, NOR and legacy presets" "$OUT/minibrute_brute_factor"',
       p,'register host test')

    for p in files:
        if new[p] == orig[p]:
            raise RuntimeError(f'Internal failure: unchanged file {p}')
        print('READY ', p)
    print('READY ', tfile, '(new)')
    if args.check:
        print('CHECK ONLY: all anchors match; no files modified')
        return
    for p in files:
        backup = Path(p + '.before-brute-factor')
        if backup.exists():
            raise RuntimeError(f'Existing backup {backup}; refusing to overwrite')
    for p in files:
        shutil.copy2(p, p + '.before-brute-factor')
        Path(p).write_text(new[p], encoding='utf-8')
        print('PATCHED', p)
    tfile.write_text(TEST, encoding='utf-8')
    print('CREATED', tfile)
    print('DONE: Stage 2 installed. Run host tests and build before flashing.')
    print('Steiner-Parker and Brute oscillator are NOT implemented in Stage 2.')

if __name__ == '__main__':
    try:
        main()
    except (RuntimeError, OSError) as e:
        sys.exit('ERROR: ' + str(e))
