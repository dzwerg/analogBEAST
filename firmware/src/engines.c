/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Engine table (order = PRESETS browsing order and the engine numbers of the editor protocol), the
 * factory patterns (SAVE > PHRASES) and the parts' sounds at power-on. */
#include "dsp.c"

/* The parts' engine memory (1.5): a part plays one engine at a time, so the engines with large per-part state
 * (engine_t.mem: PHYS's slots, GRAIN's grains and index, DRUM's lanes, FM6's notes, SLICE's reverse windows)
 * keep it in one region per part instead of an array each. The region holds the state of the engine that
 * last claimed it (eng_mem_owner); a switch to another such engine zeroes its engine_t.mem bytes, which is
 * the state each had at power-on, so a switch sounds as it did with the arrays (tests/union_test.c). State
 * that runs on across notes and switches stays outside (WHEEL's wheels, GRAIN's random numbers).
 * USB9 Stage3: reclaim/invalidate this region on EVERY engine change, even to an engine
 * with no engine_t.mem. Other global DSP state must be separately audited. This
 * pool is per part (NPART), NOT a single global mono-engine allocation.
 * Sized by the largest, PHYS (3 x SYMP's slot); an engine that needs more than ENG_MEM_PART grows it for
 * every part (x NPART of the pool). Zero-initialised pool section: main.c fm1_cstart clears it. */
#define ENG_MEM_PART 768u
static union {
    uint64_t align;
    uint8_t b[ENG_MEM_PART];
} eng_mem[NPART] __attribute__((section(".pool")));
static uint8_t eng_mem_owner[NPART];       /* engine index + 1 whose state is in the region, 0 = none (zeros) */
#define ENG_MEM(p) ((void *)eng_mem[p].b)  /* part p's region (p < NPART) */
#define ENG_MEM_FITS(name, size) _Static_assert((size) <= ENG_MEM_PART, name " state over ENG_MEM_PART")

#include "eng_analog.c"
#include "eng_phase.c"
#include "eng_lofi.c"
#include "eng_sample.c"
#include "eng_formant.c"
#include "eng_trio.c"
#include "eng_wheel.c"
#include "eng_grain.c"
#include "eng_phys.c"           /* PHYS: DaisySP physical models (phys_dsp.c, MIT) */
#include "eng_drum.c"           /* DRUM: the 8-lane kit (drum_voice.c) */
#include "eng_noise.c"
#include "eng_fm6.c"            /* FM6: 6-operator FM, msfa ported (fm6_core.c, Apache-2.0) */
#include "fm4_convert.c"        /* DIGITAL's tables, and its sounds -> FM6 */
#if FELUCCA_FM4
#include "eng_digital.c"        /* DIGITAL: four-operator FM (retired; FELUCCA_FM4=1 builds it) */
#endif
#if FELUCCA_SLICE
#include "eng_slice.c"
#endif
#if GOLANA
#include "eng_golana.c"         /* GOLANA: Golana's synth */
#include "eng_b1.c"             /* USB9 Stage6: B1 engine */
#endif

/* the editor protocol, user presets and projects store these indices: append, never reorder */
#if GOLANA
/* Legacy indices decode to current Golana: they must never access retired
 * engine state. Engine numbers retained only for existing project ABI. */
static const engine_t *const ENGINES[NENGINES] = {
    &ENG_ANALOG, &ENG_GOLANA, &ENG_GOLANA, &ENG_LOFI,
    &ENG_GOLANA, &ENG_GOLANA, &ENG_GOLANA, &ENG_GOLANA,
    &ENG_GOLANA, &ENG_GOLANA, &ENG_GOLANA, &ENG_GOLANA,
    &ENG_GOLANA, &ENG_GOLANA, &ENG_B1
};
#else
static const engine_t *const ENGINES[NENGINES] = {
    &ENG_ANALOG,                 /* 0 */
#if FELUCCA_FM4
    &ENG_DIGITAL,                /* 1 (ENGI_DIGITAL) */
#else
    &ENG_FM4_GONE,               /* 1: reserved (DIGITAL, retired: its sounds convert to FM6, fm4_convert.c) */
#endif
    &ENG_PHASE,                  /* 2 */
    &ENG_LOFI,                   /* 3 */
    &ENG_SAMPLE,                 /* 4 */
    &ENG_FORMANT,                /* 5 VOICE (eng_formant.c: "voice" is a sounding note in voice.c) */
    &ENG_TRIO,                   /* 6 */
    &ENG_WHEEL,                  /* 7 */
    &ENG_GRAIN,                  /* 8 */
    &ENG_PHYS,                   /* 9 (ENGI_PHYS) */
    &ENG_DRUM,                   /* 10 (ENGI_DRUM) */
    &ENG_NOISE,                  /* 11 */
    &ENG_FM6,                    /* 12 (ENGI_FM6) */
#if FELUCCA_SLICE
    &ENG_SLICE,                  /* 13 (FELUCCA_SLICE=0 builds without it) */
#endif
#if GOLANA
    &ENG_GOLANA,                 /* 13 (ENGI_GOLANA) */
#endif
};
#endif
_Static_assert(NELEM(ENGINES) == NENGINES, "ENGINES: NENGINES of them");

/* a track's engine number as an index (the audio paths: a compare, cheaper than % NENGINES; a bad number: 0) */
static inline uint32_t eng_idx(uint32_t e) {
#if GOLANA
    return (e == 0u || e == 3u || e == ENGI_GOLANA || e == ENGI_B1) ? e : ENGI_GOLANA;
#else
    return e < NENGINES ? e : 0u;
#endif
}

/* part t's engine memory goes to its engine (t->engine; voice.c engine_block, once a block: on a switch, after
 * the old engine's fade, before any note on the new one): zeroed (its engine_t.mem bytes) when another
 * engine's state is in it. An engine without memory leaves it as it is: a switch back finds its state. */
static void eng_mem_claim(const track_t *t)
{
    uint32_t p = (uint32_t)(t - trk), e, k, n;
    uint32_t *w;
    if (p >= NPART)
        return;
    e = eng_idx(t->engine);
    n = ENGINES[e]->mem;
    /* USB9 Stage3: every engine transition invalidates the former DSP state,
     * including changes into engines with no private state.  The single
     * per-part pool stays allocated but its bytes are reused, never stacked.
     * The owner tag changes even when n == 0, so switching back starts clean. */
    if (eng_mem_owner[p] == e + 1u)
        return;
    w = (uint32_t *)ENG_MEM(p);
    for (k = 0; k < ENG_MEM_PART / sizeof(*w); k++)
        w[k] = 0;
    eng_mem_owner[p] = (uint8_t)(e + 1u);
    (void)n;
}

/* the sample source a track's sound plays that has no data (SAMPLE's SET, GRAIN's SRC, SLICE's SRC: an empty or
 * invalid user slot, a set or PIANO this build lacks): its name (it plays a sine: eng_sample.c smp_sine), 0 = none;
 * *code a small number for it (1.., per engine and source) so the UI says it once (ui_input.c sample_notice) */
static uint8_t snd_said[NTRK];   /* per track: the code last said, 0 = none (a project load clears them: said again) */
static const char *snd_missing(const track_t *t, uint32_t *code)
{
    const engine_t *e = ENGINES[eng_idx(t->eng_req)];
    uint32_t si = (uint32_t)t->p[P_E0] % SMP_NALL;
#if FELUCCA_SLICE
    if (e == &ENG_SLICE) {
        si = slc_src_of(t->p);
        *code = 64u + si;
        return slc_get(si) ? 0 : N_SLC_SRC[si];
    }
#endif
    if (e != &ENG_SAMPLE && e != &ENG_GRAIN)
        return 0;
    *code = (e == &ENG_GRAIN ? 32u : 1u) + si % 32u;
    return smp_set_missing(si) ? SMP_ALL_NAMES[si] : 0;
}

/* the order the engines are shown in (PRESETS browsing and its ENG knob, the EDIT layer's keys, the editor's list):
 * engine indices, never DIGITAL's reserved 1 (with FELUCCA_FM4 it follows FM6). The indices stay as they are (the
 * stores and the protocol hold them); only this table orders them */
static const uint8_t ENGINE_ORDER[NENG_SHOWN] = {
#if GOLANA
    ENGI_GOLANA, 0u, 3u, ENGI_B1, /* USB9: GOLANA / ANALOG / LOFI / B1 */
#else
    0,                           /* ANALOG */
    12,                          /* FM6 */
#if FELUCCA_FM4
    1,                           /* DIGITAL */
#endif
    2, 3, 4, 5, 6, 7, 8, 9,      /* PHASE LOFI SAMPLE VOICE TRIO WHEEL GRAIN PHYS */
    11,                          /* NOISE */
#if FELUCCA_SLICE
    13,                          /* SLICE */
#endif
    10,                          /* DRUM */
#endif
};

/* the engines one can pick (engine 1 only with FELUCCA_FM4), in ENGINE_ORDER: eng_ok(e), the n-th of them
 * eng_vis(n), e's place among them eng_rank(e), the next / previous one eng_step(e, dir) (wraps) */
#if GOLANA
static int eng_ok(uint32_t e) { return e == ENGI_GOLANA || e == 0u || e == 3u || e == ENGI_B1; } /* USB9: B1 locked */
#else
static int eng_ok(uint32_t e) { return e < NENGINES && (FELUCCA_FM4 || e != ENGI_DIGITAL); }
#endif
static uint32_t eng_vis(uint32_t n) { return ENGINE_ORDER[n % NENG_SHOWN]; }
static uint32_t eng_rank(uint32_t e)
{
    uint32_t n;
    if (!eng_ok(e)) {
#if GOLANA
        /* USB9: retired engine IDs have no selectable rank; fall back to
         * Golana, never to an unavailable FM6 engine. Keep preset IDs intact. */
        e = ENGI_GOLANA;
#else
        e = ENGI_FM6; /* retired DIGITAL maps to FM6 in Felucca builds */
#endif
    }
    for (n = 0; n < NENG_SHOWN && ENGINE_ORDER[n] != e; n++)
        ;
    return n < NENG_SHOWN ? n : 0u;
}
static uint32_t eng_step(uint32_t e, int32_t dir)
{
    return eng_vis((eng_rank(e % NENGINES) + (dir > 0 ? 1u : NENG_SHOWN - 1u)) % NENG_SHOWN);
}

/* factory sequence patterns: SAVE > PHRASES loads one into the selected track (ui.c pat_load); a preset
 * only suggests one with PAT(n), loading a sound never touches the steps. Absolute notes, loaded as they
 * are (DRUM plays them as GM drums, SLICE as slices; SCL TRANS and OCT transpose): 0 = rest;
 * flags 1 = accent, 2 = slide, 4 = tie (holds the previous note). Names: at most 8 characters */
/* Factory sequence phrases deliberately NOT linked into analogBEAST.
 * All patterns live on the MUSIC DISK; synth sound presets remain intact. */
#define NPATTERNS 0u

/* the parts at power-on (engine, preset, PATTERNS[n - 1] in the sequencer, 0 = empty: all are): bass, pad, lead, drums */
#if GOLANA                       /* Golana: part 1 plays; the others are idle GOLANA parts */
static const uint8_t TRK_DEF[NPART][3] = {{ENGI_GOLANA, 0, 0}, {ENGI_GOLANA, 0, 0}, {ENGI_GOLANA, 0, 0},
                                          {ENGI_GOLANA, 0, 0}};
#else
static const uint8_t TRK_DEF[NPART][3] = {{0, 4, 0}, {ENGI_FM6, 4, 0}, {3, 0, 0}, {ENGI_DRUM, 0, 0}};
#endif /* ANALOG ACID,
                                                                       * FM6 PAD (was DIGITAL PAD), LOFI PULSE LD, DRUM KIT */
static uint32_t trk_def_engine(uint32_t i) { return TRK_DEF[i % NPART][0]; }
