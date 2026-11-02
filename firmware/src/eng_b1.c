/* USB9 Stage6: experimental Donner B1-inspired engine, not a circuit emulation.
 * Included by engines.c after the common per-part engine state allocator.
 * Only one B1 voice per part; DSP and delay are claimed together.
 */
#include "ab_b1_dsp.c"
typedef struct {
    ab_b1_state synth;
    ab_b1_delay delay;
} ab_b1_part;
ENG_MEM_FITS("B1", sizeof(ab_b1_part));
static ab_b1_part *b1_part(const track_t *t)
{
    return (ab_b1_part *)ENG_MEM((uint32_t)(t - trk) % NPART);
}
static void b1_note_on(track_t *t, voice_t *v)
{
    ab_b1_part *s = b1_part(t);
    /* Deliberately retain the delay tail across retriggers. Engine switch
       invalidates both synth and delay through eng_mem_claim(). */
    /* An accented/sliding note must inherit the oscillator and filter state.
     * In particular, resetting inc=0 on every note makes SLIDE glide up from
     * zero frequency instead of from the prior pitch. The target increment
     * is supplied by b1_render() for the new note. */
    /* First note after engine switch has no previous pitch to glide from. */
    if (!t->p[P_E7] || s->synth.inc == 0u) {
        s->synth.env = 0;
        s->synth.filter_lp = s->synth.filter_bp = 0;
        s->synth.phase = 0;
        s->synth.inc = 0;
    }
    (void)v;
}
static void b1_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    ab_b1_part *s = b1_part(t);
    ab_b1_params p;
    uint32_t i;
    p.square = (uint8_t)(t->p[P_E0] != 0);
    p.cutoff = (uint16_t)(clamp(t->p[P_E1], 0, 127) * 258);
    p.resonance = (uint16_t)(clamp(t->p[P_E2], 0, 127) * 258);
    p.decay = (uint16_t)(clamp(t->p[P_E3], 0, 127) * 258);
    p.drive = (uint16_t)(clamp(t->p[P_E4], 0, 127) * 258);
    p.delay_mix = (uint16_t)(clamp(t->p[P_E5], 0, 127) * 258);
    p.feedback = (uint16_t)(clamp(t->p[P_E6], 0, 127) * 258);
    p.slide = (uint8_t)(t->p[P_E7] != 0);
    p.accent = (uint8_t)(v->vel >= 100u);
    p.gate = (uint8_t)(v->gate != 0);
    s->synth.slide_target = m->inc;
    for (i = 0; i < n; ++i) {
        int32_t x = (int32_t)ab_b1_tick(&s->synth, &s->delay, &p);
        out[i] += voice_amp(x, m, i) * 2; /* signed left shift of negative audio is undefined C */
    }
}
static const preset_t B1_PRESETS[] = {
    {"B1 INIT", {0, 90, 45, 60, 20, 0, 40, 0}, {0, 65, 110, 30}, 0, 1, FX(0, 0, 0, 0), 0},
    {"B1 ACID", {0, 70, 95, 92, 75, 20, 55, 1}, {0, 50, 105, 25}, 0, 1, FX(0, 0, 0, 0), 0},
};
static const engine_t ENG_B1 = {
    .name = "B1",
    .page_title = {"VCO/FLT", "DRIVE/FX"},
    .edit = {
        {"WAVE", F_ENUM, 0, 1, 0, 0, 0},
        {"CUT", F_CUTOFF, 0, 127, 90, 0, 0},
        {"RES", F_PCT, 0, 127, 45, 0, 0},
        {"DEC", F_TIME, 0, 127, 60, 0, 0},
        {"DRV", F_PCT, 0, 127, 20, 0, 0},
        {"DLY", F_PCT, 0, 127, 0, 0, 0},
        {"FDBK", F_PCT, 0, 127, 40, 0, 0},
        {"SLIDE", F_ONOFF, 0, 1, 0, 0, 0},
    },
    .presets = B1_PRESETS,
    .npresets = NELEM(B1_PRESETS),
    .knob = {P_E1, P_E2, P_E3, P_E4},
    .poly = 1,
    .note_on = b1_note_on,
    .render = b1_render,
    .mem = sizeof(ab_b1_part),
};
