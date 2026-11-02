#!/usr/bin/env python3
"""miniBRUTE dev5 BRUTE FACTOR hardware-feedback fix (dev6 experiment).

Usage from C:\\FM1\\miniBRUTE:
    python3 tools/apply_minibrute_fb_v2.py --check
    python3 tools/apply_minibrute_fb_v2.py --apply
Only eng_golana.c is edited; backup created before any modification.
"""
import argparse
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / 'firmware/src/eng_golana.c'
BACKUP = ROOT / 'firmware/src/eng_golana.c.dev5-before-fb-fix.bak'
TEST = ROOT / 'tests/minibrute_feedback_v2.c'

OLD_IN = '''            /* One-sample internal VCA feedback, with bounded input saturation.
             * Amount 0 = bit-identical Golana processing; maximum about 3x feedback.
             * The limiter prevents runaway and softclips the reinjection, not
             * the final speaker signal. Effects do not enter this loop. */
            if (bf) {
                int32_t injection = (fb * (bf * 3)) >> 7;
                sx = softclip(clamp(sx + injection, -131070, 131070));
            }
'''
NEW_IN = '''            /* BRUTE-only: damped VCA -> prefilter feedback, with gentle input drive.
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
OLD_OUT = '''                fb = clamp(amp * 2, -32767, 32767); /* VCA output, prior to FX */'''
NEW_OUT = '''                /* Dampen fast feedback oscillations: one-pole LP, ~8-sample time constant.
                 * Saved between blocks in the existing per-voice state. */
                int32_t target = clamp(amp * 2, -32767, 32767);
                fb += (target - fb) >> 3;'''

TEST_SOURCE = r'''/* miniBRUTE Feedback v2: sustained notes must NOT vanish at BF > 40.
 * Host emulator only, no hardware flash; runs alongside stages 1-4. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "../web/emu/felucca_web.c"

static unsigned checks, passed;
static void ck(int good, const char *label)
{
    ++checks;
    passed += !!good;
    printf("%s %s\n", good ? "PASS" : "FAIL", label);
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
static int64_t held_mean_abs(int bf, int res, int mode, int *bounded)
{
    track_t *t = gu_t();
    voice_t *v = &t->v[0];
    glp_t *g = gl_part(t);
    vmod_t m;
    int32_t out[512];
    int64_t sum = 0;
    int64_t count = 0;
    unsigned b, i;
    gl_defaults(t->p);
    memset(&g->v[0], 0, sizeof g->v[0]);
    memset(v, 0, sizeof *v);
    memset(&m, 0, sizeof m);
    g->seed = 0x5EED1234;
    g->inc1 = g->inc2 = 0;
    v->note = 48;
    v->vel = 100;
    v->gate = v->active = 1;
    v->ph[1] = 0x40000000u;
    m.pitch16 = 48 * 16;
    m.inc = pitch_inc(m.pitch16);
    m.envq15 = m.amp0 = m.amp1 = 32767;
    t->p[P_GLSYNTH] = 1;
    t->p[P_GLSPMODE] = (int16_t)mode;
    t->p[P_GLBRUTE] = (int16_t)bf;
    t->p[GL_CUTOFF] = 50;
    t->p[GL_RESO] = (int16_t)res;
    t->p[GL_MIX1] = 100;
    t->p[GL_MIX2] = 0;
    t->p[GL_NOISE] = 0;
    t->p[P_GLB_SUB] = 0;
    t->p[P_GLBULTRA] = 0;
    t->p[P_GLBPWM] = 0;
    t->p[P_GLBMETAL] = 0;
    t->p[GL_FEGINT] = t->p[GL_KTRK] = 0;
    t->p[GL_FOLD] = t->p[GL_DIST] = 0;
    for (i = 0; i < 4u; ++i)
        t->p[GL_P1INT + 3u * i] = 0;
    *bounded = 1;
    /* Ignore the first 12 blocks (attack/filter settling), then measure ~0.4 seconds. */
    for (b = 0; b < 48u; ++b) {
        memset(out, 0, sizeof out);
        gol_render(t, v, out, 512, &m);
        for (i = 0; i < 512u; ++i) {
            int64_t a = out[i] < 0 ? -(int64_t)out[i] : (int64_t)out[i];
            if (a > 100000) *bounded = 0;
            if (b >= 12u) { sum += a; ++count; }
        }
    }
    return sum / (count ? count : 1);
}
int main(void)
{
    int good = 1, bounded, b2;
    int64_t reference, e;
    static const int bfv[] = {25, 40, 50, 75, 100, 127};
    web_nor_erase(); web_boot(); advance(1600);
    web_keys(1u << 12); advance(100); web_keys(0); advance(300);
    ck(gu.ready, "Host emulator boots");
    reference = held_mean_abs(0, 0, 0, &bounded);
    ck(bounded && reference > 100, "BRUTE Factor 0: sustained note is present");
    for (unsigned i = 0; i < sizeof bfv / sizeof bfv[0]; ++i) {
        e = held_mean_abs(bfv[i], 0, 0, &b2);
        printf("  factor %3d : mean absolute %lld vs %lld at zero\n",
               bfv[i], (long long)e, (long long)reference);
        /* Hardware defect: BF50 only a tap, then silence. Conservatively require
         * at least 25% of the original steady-state amplitude. */
        if (!b2 || e * 4 < reference) good = 0;
    }
    ck(good, "At resonance 0 and cutoff 50, sustained note not swallowed at BF 25..127");
    good = 1;
    for (int mode = 0; mode < 3; ++mode) {
        (void)held_mean_abs(127, 127, mode, &bounded);
        if (!bounded) good = 0;
    }
    ck(good, "At max resonance/factor, LP/BP/HP output stays bounded");
    printf("miniBRUTE Feedback v2: %u/%u passed, %u failed\n", passed, checks, checks-passed);
    return passed != checks;
}
'''

def main():
    ap = argparse.ArgumentParser()
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument('--check', action='store_true')
    g.add_argument('--apply', action='store_true')
    args = ap.parse_args()
    if not SOURCE.is_file():
        raise SystemExit('ERROR: cannot find ' + str(SOURCE))
    s = SOURCE.read_text(encoding='utf-8')
    counts = [s.count(OLD_IN), s.count(OLD_OUT)]
    if counts != [1,1]:
        if s.count(NEW_IN) == 1 and s.count(NEW_OUT) == 1:
            raise SystemExit('Already applied; no files changed.')
        raise SystemExit(f'ERROR: unexpected source anchors {counts}; no files changed.')
    print('READY: exact dev5 feedback loop + VCA output anchors found once each')
    if args.check:
        print('CHECK ONLY: no changes')
        return
    if BACKUP.exists():
        raise SystemExit('ERROR: backup already exists; refusing to overwrite ' + str(BACKUP))
    if TEST.exists():
        raise SystemExit('ERROR: test already exists; refusing to overwrite ' + str(TEST))
    shutil.copy2(SOURCE, BACKUP)
    SOURCE.write_text(s.replace(OLD_IN, NEW_IN, 1).replace(OLD_OUT, NEW_OUT, 1), encoding='utf-8')
    TEST.write_text(TEST_SOURCE, encoding='utf-8')
    print('PATCHED ' + str(SOURCE.relative_to(ROOT)))
    print('BACKUP  ' + str(BACKUP.relative_to(ROOT)))
    print('CREATED ' + str(TEST.relative_to(ROOT)))
    print('Now run the host tests and the dev6 firmware build. DO NOT FLASH BEFORE TESTING.')

if __name__ == '__main__':
    main()
