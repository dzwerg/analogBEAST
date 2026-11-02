#include "ab_b1_dsp.h"
/* No libc headers: embedded JieLi toolchain is freestanding. */
static void ab_b1_zero(void *ptr, unsigned long len) {
    unsigned char *p = (unsigned char *)ptr;
    while (len--) *p++ = 0;
}
static int32_t ab_clamp16(int32_t x) { return x > 32767 ? 32767 : x < -32768 ? -32768 : x; }
void ab_b1_reset(ab_b1_state *s, ab_b1_delay *delay) {
 ab_b1_zero(s, sizeof(*s));
 ab_b1_zero(delay, sizeof(*delay));
}
int16_t ab_b1_tick(ab_b1_state *s, ab_b1_delay *delay, const ab_b1_params *p) {
 /* Monotonic unsigned pitch glide: no signed overflow, no sub-Hz stall. */
 if (p->slide) {
   if (s->inc < s->slide_target) {
     uint32_t delta = s->slide_target - s->inc;
     uint32_t step = delta / 64u;
     s->inc += step ? step : 1u;
   } else if (s->inc > s->slide_target) {
     uint32_t delta = s->inc - s->slide_target;
     uint32_t step = delta / 64u;
     s->inc -= step ? step : 1u;
   }
 } else s->inc = s->slide_target;
 s->phase += s->inc;
 int32_t osc = p->square ? ((s->phase & 0x80000000u) ? -22000 : 22000)
                         : (((int32_t)(s->phase >> 16) - 32768) * 2 / 3);
 int32_t target = p->gate ? (p->accent ? 32767 : 24576) : 0;
 int32_t step = (int32_t)p->decay + 64;
 s->env += (target - s->env) / (1 + ((32767 - step) >> 7));
 int32_t x = (osc * s->env) >> 15;
 /* B1 ACCENT modulates filter brightness as well as VCA amplitude. */
 int32_t f = 256 + (p->cutoff >> 5) + (p->accent ? 384 : 0);
 int32_t r = 512 + (p->resonance >> 5);
 s->filter_bp += ((f * (x - s->filter_lp)) >> 13) - ((r * s->filter_bp) >> 13);
 s->filter_bp = ab_clamp16(s->filter_bp);
 s->filter_lp += (f * s->filter_bp) >> 13;
 s->filter_lp = ab_clamp16(s->filter_lp);
 x = s->filter_lp;
 x = ab_clamp16(x + (((int32_t)p->drive * x) >> 16));
 x = (x * 32768) / (32768 + (x < 0 ? -x : x));
 int32_t delayed = delay->sample[s->delay_pos];
 int32_t wet = (delayed * p->delay_mix) >> 15;
 int32_t fb = (delayed * p->feedback) >> 15;
 delay->sample[s->delay_pos] = (int16_t)ab_clamp16(x + fb);
 s->delay_pos = (uint16_t)((s->delay_pos + 1u) & (AB_B1_DELAY_SAMPLES - 1u));
 return (int16_t)ab_clamp16(x + wet);
}
