#!/usr/bin/env python3
"""miniBRUTE dev6 -> dev7: audible, bounded BRUTE feedback saturation.

Run from an already built/tested dev6 miniBRUTE source tree:
  python3 tools/apply_minibrute_fb_v3.py --check
  python3 tools/apply_minibrute_fb_v3.py --apply

Only eng_golana.c is changed. No change to the parameter map, preset
format, sequencer, MIKROKORG DSP, or GOLANA voice allocation.
"""
import argparse
from pathlib import Path
import shutil
import sys

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / 'firmware/src/eng_golana.c'
BACKUP = ROOT / 'firmware/src/eng_golana.c.dev6-before-fb-v3.bak'
TEST = ROOT / 'tests/minibrute_feedback_v3.c'

OLD = '''            /* BRUTE-only: damped VCA -> prefilter feedback, with gentle input drive.
             * Previous ~3x feedback drove the nonlinear filter into limit cycles.
             * At max factor, reinjection is <= ~25% of the filtered VCA signal;
             * high filter resonance reduces it further. Factor 0 stays bit-identical. */
            if (bf) {
                int32_t amount = (bf * (127 - (reso >> 1))) >> 7;
                int32_t injection = (fb * amount) >> 9;
                int32_t driven = clamp(sx + injection, -32767, 32767);
                sx = (driven * (256 + bf)) >> 8;  /* Q8: x1.0 .. x1.5 */
            }
'''
NEW = '''            /* BRUTE-only VCA -> prefilter feedback plus progressive waveshaping.
             * dev5's ~3x unfiltered loop squealed; dev6's linear gain change was
             * almost inaudible. Limit feedback to <= 1/2, damp high resonance,
             * and crossfade a driven soft-saturated signal into the filter.
             * bf==0 is an exact bypass, including the entire MIKROKORG path. */
            if (bf) {
                int32_t amount = (bf * (127 - (reso >> 1))) >> 7;
                int32_t injection = (fb * amount) >> 8; /* <= ~0.5 at RESO 0 */
                int32_t input = clamp(sx + injection, -32767, 32767);
                int32_t drive = 256 + bf * 8;           /* Q8: 1.0 .. 4.97 */
                int32_t wet = softclip(clamp((input * drive) >> 8,
                                               -65535, 65535));
                wet = (wet * (256 - bf)) >> 8;           /* compensate loudness */
                sx += ((wet - sx) * bf) >> 7;           /* dry/wet: 0 .. ~100% */
            }
'''

TEST_SOURCE = r'''/* miniBRUTE Feedback v3 regression test: change *waveform*, not only level.
 * Host/emulator only; test independently from the firmware's target build.
 * C: cc -O1 -w -DGOLANA=1 -Ibuild/gen -Ifirmware/src \
 *    -o build/host/minibrute_feedback_v3 tests/minibrute_feedback_v3.c -lm
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "../web/emu/felucca_web.c"

#define NB 4096u
static int32_t base[NB], mod40[NB], mod127[NB];
static unsigned checks, passes;
static void ck(int pass, const char *desc) {
    ++checks; passes += !!pass;
    printf("%s %s\n", pass ? "PASS" : "FAIL", desc);
}
static void settle(unsigned ms) {
    unsigned n = (unsigned)(ms * 44.1 + 0.5);
    while (n) {
        unsigned k = n > 128 ? 128 : n;
        web_render(k); n -= k;
    }
}
/* Use exactly the same note, oscillator phases and amplitude at all BF settings.
 * First discard 64 blocks (~0.19 s), then capture a held tone, NOT a transient. */
static int capture(int brute, int factor, int mode, int resonance, int cutoff,
                   int saw, int triangle, int32_t *out, uint32_t n, int64_t *meanabs) {
    track_t *t = gu_t();
    voice_t *v = &t->v[0];
    glp_t *g = gl_part(t);
    vmod_t m;
    uint32_t b, j, pos = 0;
    int32_t scratch[128];
    int64_t sum = 0;
    int bounded = 1;
    gl_defaults(t->p);
    memset(g, 0, sizeof *g);
    memset(v, 0, sizeof *v);
    memset(&m, 0, sizeof m);
    g->seed = 0x5EED1234;
    v->note = 48;
    v->vel = 100;
    v->gate = v->active = 1;
    v->ph[1] = 0x40000000u;
    m.pitch16 = 48 * 16;
    m.inc = pitch_inc(m.pitch16);
    m.envq15 = m.amp0 = m.amp1 = 32767;
    t->p[P_GLSYNTH] = (int16_t)brute;
    t->p[P_GLBRUTE] = (int16_t)factor;
    t->p[P_GLSPMODE] = (int16_t)mode;
    t->p[GL_CUTOFF] = (int16_t)cutoff;
    t->p[GL_RESO] = (int16_t)resonance;
    t->p[GL_MIX1] = (int16_t)saw;
    t->p[GL_MIX2] = 0;
    t->p[GL_NOISE] = (int16_t)triangle; /* TRI in BRUTE mixer */
    t->p[P_GLB_SUB] = 0;
    t->p[P_GLBULTRA] = t->p[P_GLBPWM] = t->p[P_GLBMETAL] = 0;
    t->p[GL_FEGINT] = t->p[GL_KTRK] = 0;
    t->p[GL_FOLD] = t->p[GL_DIST] = 0;
    for (j = 0; j < 4u; ++j)
        t->p[GL_P1INT + 3u * j] = 0;
    for (b = 0; b < 64u; ++b) {
        memset(scratch, 0, sizeof scratch);
        gol_render(t, v, scratch, 128, &m);
    }
    while (pos < n) {
        uint32_t count = n - pos > 128u ? 128u : n - pos;
        memset(scratch, 0, sizeof scratch);
        gol_render(t, v, scratch, count, &m);
        for (j = 0; j < count; ++j) {
            int64_t a = scratch[j];
            if (a > 100000 || a < -100000) bounded = 0;
            sum += a < 0 ? -a : a;
            if (out) out[pos+j] = scratch[j];
        }
        pos += count;
    }
    if (meanabs) *meanabs = n ? sum / n : 0;
    return bounded;
}
/* 0 if B is merely a scaled copy of A; positive if the spectral/phase
 * *shape* changes. This detects dev6's near-"volume only" behaviour.
 * Not a subjective audibility measurement; hardware listening still needed. */
static double shape_change(const int32_t *a, const int32_t *b, unsigned n) {
    double aa=0.0, bb=0.0, ab=0.0;
    for (unsigned i = 0; i < n; ++i) {
        aa += (double)a[i] * a[i];
        bb += (double)b[i] * b[i];
        ab += (double)a[i] * b[i];
    }
    if (aa < 1.0 || bb < 1.0) return 0.0;
    { double corr = ab / sqrt(aa * bb);
      return sqrt(fmax(0.0, 1.0 - corr*corr)); }
}
int main(void) {
    int bounded=1, b;
    int64_t e0, e40, e127, ea;
    double sh40, sh127;
    web_nor_erase(); web_boot(); settle(1600);
    ck(gu.ready, "Host emulator boots");
    bounded &= capture(1, 0, 0, 0, 85, 100, 0, base, NB, &e0);
    bounded &= capture(1, 40, 0, 0, 85, 100, 0, mod40, NB, &e40);
    bounded &= capture(1, 127, 0, 0, 85, 100, 0, mod127, NB, &e127);
    sh40 = shape_change(base, mod40, NB);
    sh127 = shape_change(base, mod127, NB);
    printf("  cutoff85/reso0: waveform difference BF40 %.3f BF127 %.3f (gain-normalized)\n", sh40, sh127);
    printf("  cutoff85/reso0: mean absolute BF0 %lld BF40 %lld BF127 %lld\n",
           (long long)e0, (long long)e40, (long long)e127);
    ck(bounded && e0 > 100, "BRUTE basic output stays bounded and audible");
    ck(sh40 > 0.003 && sh127 > 0.03 && sh127 > sh40 * 1.5,
       "BRUTE FACTOR changes waveform, not only its level");
    capture(1, 0, 0, 0, 50, 100, 0, base, NB, &e0);
    capture(1, 127, 0, 0, 50, 100, 0, mod127, NB, &e127);
    { double d50 = shape_change(base, mod127, NB);
      printf("  cutoff50/reso0: waveform difference BF127 %.3f (gain-normalized)\n", d50);
      ck(d50 > 0.015, "BF127 changes waveform at the actual hardware test setting"); }
    b = 1;
    for (int bf = 25; bf <= 127; bf += 17) {
        int ok = capture(1, bf, 0, 0, 50, 100, 0, NULL, 4096, &ea);
        if (!ok || ea < 100) b = 0;
    }
    ck(b, "Cutoff50/resonance0: no sustained-note dropout at BF25..127");
    b = 1;
    for (int mode=0; mode<3; ++mode) {
        if (!capture(1, 127, mode, 127, 50, 100, 0, NULL, 4096, &ea)) b = 0;
    }
    ck(b, "Steiner LP/BP/HP bounded at maximum feedback and resonance");
    b = capture(0, 0, 0, 0, 85, 100, 0, base, NB, NULL);
    b &= capture(0, 127, 0, 0, 85, 100, 0, mod127, NB, NULL);
    ck(b && memcmp(base, mod127, sizeof base) == 0,
       "MIKROKORG audio bit-identical for all BRUTE FACTOR values");
    printf("miniBRUTE Feedback v3: %u/%u passed; %u failed\n", passes, checks, checks-passes);
    return passes != checks;
}
'''

def main():
    ap = argparse.ArgumentParser()
    group = ap.add_mutually_exclusive_group(required=True)
    group.add_argument('--check', action='store_true')
    group.add_argument('--apply', action='store_true')
    args = ap.parse_args()
    if not SRC.exists():
        sys.exit('ERROR: missing firmware/src/eng_golana.c')
    source = SRC.read_text(encoding='utf-8')
    if source.count(OLD) != 1:
        if source.count(NEW) == 1:
            sys.exit('Already applied; no files changed.')
        sys.exit(f'ERROR: dev6 feedback anchor seen {source.count(OLD)} times, expected 1. No files changed.')
    if source.count('                fb += (target - fb) >> 3;') != 1:
        sys.exit('ERROR: dev6 feedback lowpass missing or modified. No files changed.')
    if source.count('int32_t sp_mode = p[P_GLSYNTH] == 1') != 1:
        sys.exit('ERROR: Steiner dev6 BRUTE branch absent. No files changed.')
    print('READY: exact dev6 BRUTE feedback matched; MIKROKORG unchanged')
    if args.check:
        print('CHECK ONLY: no files written')
        return
    if BACKUP.exists():
        sys.exit(f'ERROR: backup already exists: {BACKUP}. No files changed.')
    if TEST.exists():
        sys.exit(f'ERROR: test already exists: {TEST}. No files changed.')
    shutil.copy2(SRC, BACKUP)
    SRC.write_text(source.replace(OLD, NEW, 1), encoding='utf-8')
    TEST.write_text(TEST_SOURCE, encoding='utf-8')
    print('PATCHED firmware/src/eng_golana.c')
    print('BACKUP  firmware/src/eng_golana.c.dev6-before-fb-v3.bak')
    print('CREATED tests/minibrute_feedback_v3.c')
    print('Build dev7 and run host tests; do not flash before results are checked.')

if __name__ == '__main__':
    main()
