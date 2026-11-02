/* analogBEAST USB9: original B1-style DSP study, no Donner firmware/code.
   This is a development-only, fixed-point mono audio chain.
   This module is NOT linked to the FM-1 audio callback yet. */
#ifndef AB_B1_DSP_H
#define AB_B1_DSP_H
#include <stdint.h>
typedef struct {
  uint32_t phase, inc, slide_target;
  int32_t filter_lp, filter_bp, env;
  int32_t delay[256];
  uint16_t delay_pos;
} ab_b1_state;
typedef struct {
  uint16_t cutoff, resonance, decay, drive, delay_mix, feedback;
  uint8_t square, accent, gate, slide;
} ab_b1_params;
void ab_b1_reset(ab_b1_state *s);
int16_t ab_b1_tick(ab_b1_state *s, const ab_b1_params *p);
#endif
