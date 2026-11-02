/* USB9 stage5: Donner B1-inspired DSP; approximation, not a circuit clone.
 * Split core state from effect RAM for future shared-allocated delay. */
#ifndef AB_B1_DSP_H
#define AB_B1_DSP_H
#include <stdint.h>
#define AB_B1_DELAY_SAMPLES 256u
typedef struct {
 uint32_t phase, inc, slide_target;
 int32_t filter_lp, filter_bp, env;
 uint16_t delay_pos;
} ab_b1_state;
typedef struct {
 int16_t sample[AB_B1_DELAY_SAMPLES];
} ab_b1_delay;
typedef struct {
 uint16_t cutoff, resonance, decay, drive, delay_mix, feedback;
 uint8_t square, accent, gate, slide;
} ab_b1_params;
void ab_b1_reset(ab_b1_state *s, ab_b1_delay *delay);
int16_t ab_b1_tick(ab_b1_state *s, ab_b1_delay *delay, const ab_b1_params *p);
#endif
